/* board.h - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/uart.h"
#include "driver/gpio.h"
#include "../Secret/NetworkConfig.h"

#ifndef _BOARD_H_
#define _BOARD_H_

// #define UART_NUM_C6 UART_NUM_0
// #define TX_PIN_C6 16
// #define RX_PIN_C6 17
// #define RTS_PIN_C6 2
// #define CTS_PIN_C6 3

#define UART_NUM_H2 UART_NUM_0
#define TX_PIN_H2 24
#define RX_PIN_H2 23

#define UART_NUM    UART_NUM_H2
#define TXD_PIN     TX_PIN_H2
#define RXD_PIN     RX_PIN_H2
#define RTS_PIN     UART_PIN_NO_CHANGE // not using
#define CTS_PIN     UART_PIN_NO_CHANGE // not using
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024

void board_init(void);
int uart_write(const char* logName, const uint8_t* data, size_t length);
int uart_sendData(const char* logName, const uint8_t* data, size_t length);
int uart_sendMsg(const char* logName, const char* msg);

#endif /* _BOARD_H_ */
