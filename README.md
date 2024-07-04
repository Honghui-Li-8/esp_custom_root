| Supported Targets | ESP32-H2 | 
| ----------------- | -------- | 

ESP32 Root Network Module
==================================
## Table of Contents
- [ESP32 Root Network Module](#esp32-root-network-module)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Hardware Components](#hardware-components)
  - [Software Components](#software-components)
  - [Setup and Configuration](#setup-and-configuration)
  - [Communication Protocols](#communication-protocols)
  - [Code Structure](#code-structure)
  - [Code Flow](#code-flow)
    - [1) Initialization](#1-initialization)
    - [2) UART Channel Managing](#2-uart-channel-managing)
    - [3) Network Command Execution](#3-network-command-execution)
    - [4) Event Handler](#4-event-handler)
  - [Error Handling](#error-handling)
  - [Testing and Troubleshooting](#testing-and-troubleshooting)
  - [References](#references)

## Overview
The ESP32 Root Module served as the Initializer to the ble-mesh Network. It is responsible for managing and coordinating the edge devices and also ensuring efficient communication and data processing. The ESP32 Root is equipped with WiFi and Bluetooth, but we're focusing more on the BLE Mesh Features. Features that are included are as follows:
- Network Access Point
  - Listen to `uart` channle of network commands and transmit bytes via `ble low energy`
  - Write incoming messages (bytes) to `uart` channle
- Provisioner of Network
  - Acts as a `main provisioner` for our mesh topology network to register new nodes
  - Ability to triger `Remote Provision` (Implitly provision nodes outside the range of the root via hopping)
- Core of Network
  - Initialize and Maintain the ble Network
  - Root keeps tracks the overall Network Status including Registered Nodes
  - Option of Non-Volatile or Volatile (Persistent or Non-Persistent Memory) of Network Status
      
## Hardware Components
[not sure if the Custom PCB and the Antenna will be included here or something].

## Software Components
<<<<<<< HEAD
- ESP-IDF v 5.2.0 (Espressif IoT Development Framework)
=======
- ESP-IDF version 5.2.0 (Espressif IoT Development Framework)
>>>>>>> a0aaf3ecff9a12af5b1149bc8b3b4676bef58996
  - Description: Official development framework for ESP32
  - Function: Provides libraries and tools for developing applications on the ESP32
    - Build, Flash, Monitor, etc.
  - Instalation: [link to ESP's website](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

## Setup and Configuration
In this section, we will be explaining 2 ways on using our program, specifically ESP-IDF.

### 1. Downloading ESP-IDF Extension on VSCode
- You can download ESP-IDF Extension 
  
## Communication Protocols
[Network commands from uart]?

## Code Structure
This repo contained several files and directories, but the important ones will be listed below:
- **`/main`:** Contains the main source code files.
  - **`ble_meshconfig_edge.c`:** Functions interact with esp-idf's ble APIs
  - **`ble_meshconfig_edge.h`**
  - **`board.c`:** Functions interacts with hardware. uart, led, button, etc.
  - **`board.h`**
  - **`CMakeList.txt`**
  - **`idf_componennt.yml`**
  - **`main.c`:** Function interacts with API level commands and Network event handlers
- **`/Secret`:** Contains our Network Configuration for the Mesh Network and Headers
- **`CMakeList.txt`:** Header files and definitions.
- **`sdkconfig.defaults`:** Contain ESP Configurations as a default config if no `sdkconfig` exist

## Code Flow
### 1) Initialization
The module is initialized and configured in `app_main()` when power on or resetted. It initialized all the `hardware componets`, `ble-mesh configurations`, and `uart procssing thread`, then attachs all event handlers. After initialization, root module sends and message to uart channel signaling root module online.

In the code below, `line 3`, `esp_log_level_set(TAG_ALL, ESP_LOG_NONE);` disables esp logs that's used for develpment debug logging which will pollute uart channle on root module.

```c
void app_main(void)
{
    esp_log_level_set(TAG_ALL, ESP_LOG_NONE); // disable esp logs
    
    esp_err_t err = esp_module_root_init(prov_complete_handler, config_complete_handler, recv_message_handler,       
                              recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_M, "Network Module Initialization failed (err %d)", err);
        uart_sendMsg(0, "Error: Network Module Initialization failed\n");
        return;
    }

    board_init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);

    char message[15] = "online\n";
    uint8_t message_byte[15];
    message_byte[0] = 0x03; // Root Reset
    memcpy(message_byte + 1, message, strlen(message));
    uart_sendData(0, message_byte, strlen(message) + 1);
    printNetworkInfo(); // esp log for debug
}
```
In `line 5`, `esp_module_root_init` is called to initialize the ESP Module which includes multiple functions that are used as callback functions for Network events.
```c
esp_err_t esp_module_root_init(
    void (*prov_complete_handler)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, 
                                  uint16_t addr, uint8_t element_num, uint16_t net_idx),
    void (*config_complete_handler)(uint16_t addr),
    void (*recv_message_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*recv_response_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*timeout_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode),
    void (*broadcast_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*connectivity_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr)
) { ... }
```
Each handler function will get trigers by corresponding event [link here](#event-handler)

### 2) UART Channel Managing
### 3) Network Command Execution
### 4) Event Handler
- prov_complete_handler
- config_complete_handler
- recv_message_handler
- recv_response_handler
- timeout_handler
- broadcast_handler
- connectivity_handler


<<<<<<< HEAD
## Error Handling
=======
First is the callback function that we have in `main.c`. Each function will have a description explaining its purpose, the parameters that are passed to it, and when/why it is triggered.
- **prov_complete_handler**
- **config_complete_handler**
- **recv_message_handler**
- **recv_response_handler**
- **timeout_handler**
- **broadcast_handler**
- **connectivity_handler**

OPTIONAL:
Explain what defined can off, or how to change the app or net keIDid, or NetworkConfig, or even if they want to add another opcode or something

### Error Handling
>>>>>>> a0aaf3ecff9a12af5b1149bc8b3b4676bef58996

## Testing and Troubleshooting

## References
