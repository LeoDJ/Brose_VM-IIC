//  Copyright 2017 by Malte Janduda
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//  Modified by LeoDJ

#include "udp_logging.h"

#include <Arduino.h>

#include "esp_log.h"
#include "esp_system.h"

#include <string.h>

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

int udp_log_fd;
static struct sockaddr_in serveraddr;
static uint8_t buf[UDP_LOGGING_MAX_PAYLOAD_LEN];
static uint8_t bufChar[UDP_LOGGING_MAX_PAYLOAD_LEN];
static uint16_t bufCharIdx = 0;
vprintf_like_t prevLogger = NULL;



int get_socket_error_code(int socket) {
    int result;
    u32_t optlen = sizeof(int);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen) == -1) {
        printf("getsockopt failed");
        return -1;
    }
    return result;
}

int show_socket_error_reason(int socket) {
    int err = get_socket_error_code(socket);
    printf("UDP socket error %d %s", err, strerror(err));
    return err;
}

void udp_logging_free(va_list l) {
    int err = 0;
    char *err_buf;
    esp_log_set_vprintf(vprintf);
    if ((err = shutdown(udp_log_fd, 2)) == 0) {
        vprintf("\nUDP socket shutdown!", l);
    } else {
        asprintf(&err_buf, "\nShutting-down UDP socket failed: %d!\n", err);
        vprintf(err_buf, l);
    }

    if ((err = close(udp_log_fd)) == 0) {
        vprintf("\nUDP socket closed!", l);
    } else {
        asprintf(&err_buf, "\n Closing UDP socket failed: %d!\n", err);
        vprintf(err_buf, l);
    }
    udp_log_fd = 0;
}



int udp_logging_vprintf(const char *str, va_list l) {
    // also call prev logger
    // if (prevLogger != NULL) {
    //     prevLogger(str, l);
    // }

    int err = 0;
    int len;
    char task_name[16];
    char *cur_task = pcTaskGetTaskName(xTaskGetCurrentTaskHandle());
    strncpy(task_name, cur_task, 16);
    task_name[15] = 0;
    if (strncmp(task_name, "tiT", 16) != 0) {
        len = vsprintf((char *)buf, str, l);
        if ((err = sendto(udp_log_fd, buf, len, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr))) < 0) {
            show_socket_error_reason(udp_log_fd);
            vprintf("\nFreeing UDP Logging. sendto failed!\n", l);
            udp_logging_free(l);
            return vprintf("UDP Logging freed!\n\n", l);
        }
    }

    // return vprintf(str, l);  // also print to default output
    return len;
}

int udp_logging_printf(const char *fmt, ...) {
    va_list ap;
    int res = 0;

    va_start(ap, fmt);
    res = udp_logging_vprintf(fmt, ap);
    va_end(ap);

    return res;
}

void udp_logging_char(char c) {
    bufChar[bufCharIdx++] = c;
    if (c == '\n') {
        bufChar[bufCharIdx] = 0;
        udp_logging_printf((const char *)bufChar);
        bufCharIdx = 0;
    }
}


int udp_logging_init(const char *ipaddr, unsigned long port) {
    struct timeval send_timeout = {1, 0};
    udp_log_fd = 0;
    ESP_LOGI("UDP_LOGGING", "initializing udp logging...");
    if ((udp_log_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ESP_LOGE("UDP_LOGGING", "Cannot open socket!");
        return -1;
    }


    uint32_t ip_addr_bytes;
    inet_aton(ipaddr, &ip_addr_bytes);
    ESP_LOGI("UDP_LOGGING", "Logging to %s (0x%x)", ipaddr, ip_addr_bytes);

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = ip_addr_bytes;

    int err = setsockopt(udp_log_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&send_timeout, sizeof(send_timeout));
    if (err < 0) {
        ESP_LOGE("UDP_LOGGING", "Failed to set SO_SNDTIMEO. Error %d", err);
    }

    // // Set the socket to reuse the address
    // int reuse = 1;
    // err = setsockopt(udp_log_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // if (err < 0) {
    //     ESP_LOGE("UDP_LOGGING", "Failed to set SO_REUSEADDR. Error %d", err);
    // }

    // // Set the socket to send multicast packets
    // int loop = 1;
    // err = setsockopt(udp_log_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    // if (err < 0) {
    //     ESP_LOGE("UDP_LOGGING", "Failed to set IP_MULTICAST_LOOP. Error %d", err);
    // }

    // // Set the time-to-live (TTL) for the multicast packet
    // int ttl = 2;
    // err = setsockopt(udp_log_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    // if (err < 0) {
    //     ESP_LOGE("UDP_LOGGING", "Failed to set IP_MULTICAST_TTL. Error %d", err);
    // }

    // prevLogger = esp_log_set_vprintf(udp_logging_vprintf);
    ets_install_putc2(&udp_logging_char);   // needed for Arduino ESP32 framework

    return 0;
}
