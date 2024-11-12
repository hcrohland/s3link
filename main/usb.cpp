/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/cdc_acm_host.h"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"
#include "usb/vcp.hpp"
#include "usb/usb_host.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "usb.h"

using namespace esp_usb;

// Change these values to match your needs
#define EXAMPLE_BAUDRATE (115200)
#define EXAMPLE_STOP_BITS (0) // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
#define EXAMPLE_PARITY (0)    // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
#define EXAMPLE_DATA_BITS (8)

static const char *TAG = "USB Handler";
// static SemaphoreHandle_t device_disconnected_sem;

/**
 * @brief Device event callback
 *
 * Apart from handling device disconnection it doesn't do anything useful
 *
 * @param[in] event    Device event type and data
 * @param[in] user_ctx Argument we passed to the device open function
 */
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %d", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        // xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        break;
    }
}

/**
 * @brief USB Host library handling task
 *
 * @param arg Unused
 */
static void usb_lib_task(void *arg)
{
    while (1)
    {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_LOGI(TAG, "USB: No clients");
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG, "USB: All devices freed");
            // Continue handling USB events to allow device reconnection
        }
    }
}

/**
 * @brief Main application
 *
 * This function shows how you can use Virtual COM Port drivers
 */
extern "C" void usb_init(void)
{
    // device_disconnected_sem = xSemaphoreCreateBinary();
    // assert(device_disconnected_sem);

    // Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    // Register VCP drivers to VCP service
    VCP::register_driver<FT23x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<CH34x>();
}

extern "C" ssize_t usb_open(const int sock, cdc_acm_data_callback_t handle_rx)
{
    // Do everything else in a loop, so we can demonstrate USB device reconnections
    while (true)
    {
        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 60000, // 5 seconds, enough time to plug the device in or experiment with timeout
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = (void *)&sock,
        };

        // You don't need to know the device's VID and PID. Just plug in any device and the VCP service will load correct (already registered) driver for the device
        ESP_LOGI(TAG, "Opening any VCP device...");
        auto vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev_config));

        if (vcp == nullptr)
        {
            ESP_LOGI(TAG, "Failed to open VCP device");
            continue;
        }
        vTaskDelay(10);

        ESP_LOGI(TAG, "Setting up line coding");
        cdc_acm_line_coding_t line_coding = {
            .dwDTERate = EXAMPLE_BAUDRATE,
            .bCharFormat = EXAMPLE_STOP_BITS,
            .bParityType = EXAMPLE_PARITY,
            .bDataBits = EXAMPLE_DATA_BITS,
        };
        ESP_ERROR_CHECK(vcp->line_coding_set(&line_coding));

        /*
        Now the USB-to-UART converter is configured and receiving data.
        You can use standard CDC-ACM API to interact with it. E.g.

        ESP_ERROR_CHECK(vcp->set_control_line_state(false, true));
        ESP_ERROR_CHECK(vcp->tx_blocking((uint8_t *)"Test string", 12));
        */

        int len;
        uint8_t rx_buffer[512];

        do
        {
            fd_set read;
            FD_ZERO(&read);
            FD_SET(sock, &read);
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            if (select(sock + 1, &read, NULL, NULL, &timeout) > 0)
            {
                len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
                if (len <= 0)
                {
                    if (len < 0)
                    {
                        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Socket closed");
                    }
                    ESP_LOGI(TAG, "Closing usb");
                    vcp->close();
                    return len;
                }
                ESP_LOG_BUFFER_CHAR_LEVEL("Received", rx_buffer, len, ESP_LOG_INFO);

                esp_err_t err_rc = vcp->tx_blocking(rx_buffer, len);
                if (err_rc != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error sending to USB: %s", esp_err_to_name(err_rc));
                    break;
                } 
            }
            else
            {
                // check usb connection
                esp_err_t err_rc = vcp->set_control_line_state(false, true);
                if (err_rc != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error testing USB: %s", esp_err_to_name(err_rc));
                    break;
                } 
                // continue loop
                len = 1;
            }
        } while (len > 0);

        ESP_LOGI(TAG, "Closing usb");
        vcp->close();
    }
}
