| Supported Targets | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | -------- |


# s3link

A usb serial to wifi bridge based on the esp tcp server and usb host examples

## How to use s3link

My use case is to connect my 3d printer over Wifi with octoprint on a remote host


## Hardware Required

This example can be run on ESP32-S3 and probably -S2 development board.

## Configure the project

```
idf.py menuconfig
```

Set following parameters under Example Configuration Options:

* Set `IP version` of the example to be IPV4 or IPV6.

* Set `Port` number of the socket, that server example will create.

* Set `TCP keep-alive idle time(s)` value of TCP keep alive idle time. This time is the time between the last data transmission.

* Set `TCP keep-alive interval time(s)` value of TCP keep alive interval time. This time is the interval time of keepalive probe packets.

* Set `TCP keep-alive packet retry send counts` value of TCP keep alive packet retry send counts. This is the number of retries of the keepalive probe packet.

Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu. See "Establishing Wi-Fi or Ethernet Connection" section in [examples/protocols/README.md](../../README.md) for more details.

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
