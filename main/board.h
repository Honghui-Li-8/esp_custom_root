/* board.h - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/uart.h"
#include "driver/gpio.h"
#include <arpa/inet.h>
#include "../Secret/NetworkConfig.h"

#ifndef _BOARD_H_
#define _BOARD_H_

// #define UART_NUM_C6 UART_NUM_0
// #define TX_PIN_C6 16
// #define RX_PIN_C6 17
// #define RTS_PIN_C6 2
// #define CTS_PIN_C6 3
// #define UART_NUM    UART_NUM_C6
// #define TXD_PIN     TX_PIN_C6
// #define RXD_PIN     RX_PIN_C6

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

// #define UART_NUM_C6 UART_NUM_0
// #define TX_PIN_C6 16
// #define RX_PIN_C6 17
// #define RTS_PIN_C6 2
// #define CTS_PIN_C6 3

#define UART_NUM_H2 UART_NUM_0 // defult log port, root modul,e connect to pc via usb-uart
#define TX_PIN_H2 24 // dpin connected with usb-uart
#define RX_PIN_H2 23 // dpin connected with usb-uart

// #define UART_NUM_H2 UART_NUM_1 // UART_NUM_0 is used for usb monitor already
// #define TX_PIN_H2 0 // we can define any gpio pin
// #define RX_PIN_H2 1 // we can define any gpio pin

#define UART_NUM    UART_NUM_H2
#define TXD_PIN     TX_PIN_H2
#define RXD_PIN     RX_PIN_H2
#define RTS_PIN     UART_PIN_NO_CHANGE // not using
#define CTS_PIN     UART_PIN_NO_CHANGE // not using
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024

#define BUTTON_IO_NUM           9
#define BUTTON_ACTIVE_LEVEL     0
#define ESCAPE_BYTE 0xFA
#define UART_START 0xFF
#define UART_END 0xFE

void board_init(void);

/**
 * @brief Encoded bytes and write to a UART port.
 * 
 * @param uart_num UART port number.
 * @param data Pointer to the data to be written.
 * @param length Length of the data.
 * @return Status of the write operation.
 */
int uart_write_encoded_bytes(uart_port_t uart_num, uint8_t* data, size_t length);

/**
 * @brief Decode bytes from the provided data.
 * 
 * @param data Pointer to the encoded data.
 * @param length Length of the encoded data.
 * @param decoded_data Pointer to the buffer for the decoded data.
 * @return Status of the decode operation.
 */
int uart_decoded_bytes(uint8_t* data, size_t length, uint8_t* decoded_data);

/**
 * @brief Send data to a specific node address over UART.
 * 
 * @param node_addr Node address to send the data to.
 * @param data Pointer to the data to be sent.
 * @param length Length of the data.
 * @return Status of the send operation.
 */
int uart_sendData(uint16_t node_addr, uint8_t* data, size_t length);

/**
 * @brief Send a message to a specific node address over UART.
 * 
 * @param node_addr Node address to send the message to.
 * @param msg Pointer to the message to be sent.
 * @return Status of the send operation.
 */
int uart_sendMsg(uint16_t node_addr, char* msg);

#endif /* _BOARD_H_ */