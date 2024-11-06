#ifdef __cplusplus
extern "C"
{
#endif
    #include "usb/cdc_acm_host.h"

    ssize_t usb_open(const int sock, const cdc_acm_data_callback_t handle_rx);
    void usb_init(void);

#ifdef __cplusplus
}
#endif
