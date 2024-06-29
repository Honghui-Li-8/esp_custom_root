| Supported Targets | ESP32-H2 | 
| ----------------- | -------- | 

ESP32 Root / Client
==================================
## Table of Contents
- [ESP32 Root / Client](#esp32-root--client)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Hardware Components](#hardware-components)
  - [Software Components](#software-components)
  - [Communication Protocols](#communication-protocols)
  - [Setup and Configuration](#setup-and-configuration)
  - [Code Explanation](#code-explanation)
    - [Code Structure](#code-structure)
    - [Code Flow](#code-flow)
    - [Key Components](#key-components)
    - [Error Handling](#error-handling)
  - [Testing and Troubleshooting](#testing-and-troubleshooting)
  - [References](#references)

## Overview
Our ESP32 Root, also called ESP32 Client served as our central network. It is responsible for managing and coordinating the edge devices and also ensuring efficient communication and data processing. The ESP32 Root is equipped with WiFi and Bluetooth, but we're focusing more on the BLE Mesh Features. Features that are included are as follows:
- Acts as a `main provisioner` for our mesh topology network
- Manage and handle communication between edge devices (data aggregation)
- Ability to do `Remote Provision` (provision nodes outside the range of the root)
- Can act as Non-Volatile or Volatile (Persistent or Non-Persistent Memory)
      
## Hardware Components
[not sure if the Custom PCB and the Antenna will be included here or something].

## Software Components
- ESP-IDF (Espressif IoT Development Framework)
  - Description: Official development framework for ESP32
  - Function: Provides libraries and tools for developing applciations on the ESP32
  
## Communication Protocols
[Opcode and other stuff maybe]?

## Setup and Configuration
[I (Axel) would put how to setup docker in here later.]

## Code Explanation
### Code Structure
This repo contained several files and directories, but the important ones will be listed below:
- **/main:** Contains the main source code files.
  - **ble_meshconfig_edge.c**
  - **ble_meshconfig_edge.h**
  - **board.c**
  - **board.h**
  - **CMakeList.txt**
  - **idf_componennt.yml**
  - **main.c**
- **/Secret:** Contains our Network Configuration for the Mesh Network and Headers
- **CMakeList.txt:** Header files and definitions.
- **sdkconfig.defaults:** Contain ESP Configurations as a default config if no `sdkconfig` exist

### Code Flow
The program starts with an initialization phase where hardware and software components are set up. It then enters the main loop where it continuously monitors and processes data. The program will keep running until it is shut off (via power source), or when it is resetted using the reset button or through a command

- **Initialization** starts in the `main.c` file
  ```c
  void app_main(void)
    {
    esp_log_level_set(TAG_ALL, ESP_LOG_NONE);
    
    esp_err_t err = esp_module_root_init(prov_complete_handler, config_complete_handler, recv_message_handler, recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_M, "Network Module Initialization failed (err %d)", err);
        uart_sendMsg(0, "Error: Network Module Initialization failed\n");
        return;
    }
    board_init();
  ```
  In `line 5`, `esp_module_root_init` is called to initialize the ESP Module which includes multiple functions that are used as callback functions.

- After initializing in `main.c`, it will move to the `ble_mesh_config_root.c`, located in `line 1169`, you can found the `esp_module_root_init` function
  ```c
  esp_err_t esp_module_root_init(
      void (*prov_complete_handler)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx),
      void (*config_complete_handler)(uint16_t addr),
      void (*recv_message_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
      void (*recv_response_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
      void (*timeout_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode),
      void (*broadcast_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
      void (*connectivity_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr)
  ) { ... }
  ```
  Each handler function would be explain in the [Key Components](#key-components) section

  ```
  This function will return a defined variable called `ESP_OK` every time it is successfully finished. A lot of ESP32 Functions will be returning the same thing, so assume it will return `ESP_OK` unless say otherwise.
- `ble_mesh_init()` will be continued [here], and this concludes the code flow in `main.c`

### Key Components
Explain what defined can off, or how to change the app or net keIDid, or NetworkConfig, or even if they want to add another opcode or something

### Error Handling

## Testing and Troubleshooting

## References
