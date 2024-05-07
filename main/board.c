/* board.c - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "iot_button.h"
#include "board.h"

#define TAG_B "BOARD"
#define TAG_W "Debug"

#define BUTTON_IO_NUM           9
#define BUTTON_ACTIVE_LEVEL     0

extern void send_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr);

static void uart_init() {  // Uart ===========================================================
    const int uart_num = UART_NUM;
    const int uart_buffer_size = UART_BUF_SIZE * 2;
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl= UART_HW_FLOWCTRL_DISABLE, // = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = UART_SCLK_DEFAULT, // = 122,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size,
                                        uart_buffer_size, 0, NULL, 0)); // not using queue
                                        // uart_buffer_size, 20, &uart_queue, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    // Set UART pins                      (TX,      RX,      RTS,     CTS)
    ESP_ERROR_CHECK(uart_set_pin(uart_num, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN));

    ESP_LOGI(TAG_B, "Uart init done");
}

int uart_sendEndOfMsg() {
    return uart_write_bytes(UART_NUM, END_OF_MSG, strlen(END_OF_MSG)); // end of message symbol
}

int uart_sendData(const char* logName, const uint8_t* data, size_t length)
{
    int txBytes = uart_write_bytes(UART_NUM, data, length);
    txBytes += uart_sendEndOfMsg(); // end of message symbol

    if (logName != NULL) {
        ESP_LOGI(logName, "Wrote %d bytes on uart-tx", txBytes);
    }
    return txBytes;
}

int uart_sendMsg(const char* logName, const char* msg)
{
    size_t length = strlen(msg);
    int txBytes = uart_write_bytes(UART_NUM, msg, length);
    txBytes += uart_sendEndOfMsg(); // end of message symbol

    if (logName != NULL) {
        ESP_LOGI(logName, "Wrote %d bytes on uart-tx", txBytes);
    }
    return txBytes;
}

static void button_tap_cb(void* arg)
{
    ESP_LOGW(TAG_W, "button pressed ------------------------- ");
    static uint8_t *data_buffer = NULL;
    if (data_buffer == NULL) {
        data_buffer = (uint8_t*)malloc(128);
        if (data_buffer == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
    }
    
    strcpy((char*)data_buffer, "[CMD]root write serial\n");
    uart_sendMsg(TAG_B, (char*) data_buffer);
}

static void board_button_init(void)
{
    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    }
}

void board_init(void)
{
    uart_init();
    board_button_init();
}
