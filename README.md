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
    - [1. Downloading ESP-IDF Extension on VSCode (*Recommended*)](#1-downloading-esp-idf-extension-on-vscode-recommended)
    - [2. Using Docker Images on ESP-IDF](#2-using-docker-images-on-esp-idf)
  - [Communication Protocols](#communication-protocols)
  - [Code Structure](#code-structure)
  - [Code Flow](#code-flow)
    - [1) Initialization](#1-initialization)
    - [2) UART Channel Logic Flow](#2-uart-channel-logic-flow)
    - [3) Network Commands - UART incoming](#3-network-commands---uart-incoming)
    - [4) Module to App level - UART outgoing](#4-module-to-app-level---uart-outgoing)
    - [5) Event Handler](#5-event-handler)
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
- ESP-IDF version 5.2.0 (Espressif IoT Development Framework)
  - Description: Official development framework for ESP32
  - Function: Provides libraries and tools for developing applications on the ESP32
    - Build, Flash, Monitor, etc.
  - Instalation: [link to ESP's website](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

## Setup and Configuration
In this section, we will be explaining 2 ways on using our program, specifically ESP-IDF.

### 1. Downloading ESP-IDF Extension on VSCode (*Recommended*)
- Make sure you have [VS Code](https://code.visualstudio.com/download), it can be any operating system, or any version of VS Code.
- The next step is to download ESP-IDF Extension on VSCode. There are steps
on using ESP-IDF Extension in this [link](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md)
- **P.S. Make sure the ESP-IDF version is 5.2.0, without this version, our code would not be able to run.**
- Once you have follow the steps on installing ESP-IDF, you are ready to `build`, `flash`, and `monitor`.
- **P.S. If you are on windows, you need to install a driver to establish a serial connection with the ESP32 Board. You can find it on this [website](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html). The author use the `CP210x USB to UART Bridge Drivers` to connect the windows port to the ESP32.**

### 2. Using Docker Images on ESP-IDF
- If you don't want to download ESP-IDF Extension, you can also use a Docker Image to `build` and `flash` the program. However, this only works in `Linux` system since you need a port number that's connected to the ESP32 when `flashing`
- The steps are as follows:
  1. Make sure you have [Docker Desktop](https://www.docker.com/products/docker-desktop/) downloaded and running in the background 
  2. Go to the terminal, and go to the project directory. (If you want to make sure you are in your project directory, you can write `${PWD}`, if this returns the project directory, that means you're in the right palce)
  3. Run `docker run --rm -v ${PWD}:/project -w /project -e HOME=/tmp espressif/idf:v5.2 idf.py build`
  4. Once it's done building, then you can run `docker run --rm -v ${PWD}:/project -w /project -e HOME=/tmp espressif/idf:v5.2 idf.py -p -PORT flash`. to flash to a ESP32 Board

  The `-PORT` you can change it to the port you're ESP32 is connected to, for example `/dev/ttyS5`. For more information on the docker image ESP32, click [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-docker-image.html)

  Also, after you're done flashing, you could also write `idf.py monitor --no-reset -p -PORT`. For more information, you can check [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-monitor.html)
  
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

In the code below, `line 3`, `esp_log_level_set(TAG_ALL, ESP_LOG_NONE);` disables esp logs that's used for develpment debug logging but will pollute uart channle on root module.

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

### 2) UART Channel Logic Flow
The module communicate with central pc via usb-uart port.

1. `UART byte encoding` - To ensure the message bytes' integrity, message encoding was applied to add `\0xFF` and `\0xFE` speical bytes at the begining and end of an uart message. Also the messagebyte encoding was applied to encode all bytes >= `\0xFA` into 2 byte with xor gate to reserve all bytes > `\0xFA` as speical bytes. Uart encoding, decoding, and write functions is defined in `board.h` file with detield explainatiion.

2. `UART channle listening thread` - The function `rx_task()` on main.c defines the uart signal handling logic. It create an infniate scaning loop to check uart buffer's data avalibility. Once the scaner read in datas, it passes to `uart_task_handler()` to scan for message start byte `\0xFF` and message end byte `\0xFE` to locate the message then decode the messsage and invoke `execute_uart_command()` to parse and execute the message recived.

3. `execute_uart_command()` - This function responsible for executing commands from application level such as `BCAST`, `SEND-`, and etc. The module able to be extended for custom command by adding a case in this function.

### 3) Network Commands - UART incoming
The formate of network commands send to esp module is defined to consist `5_byte_network_command | payload` where the payloadi's format varys based on the command and detils on current commands is documented here. `[Add link later]------------------------`

### 4) Module to App level - UART outgoing
The formate of esp module to app level message is defined as `2_byte_node_addr | payload`. The first part is `netword endian` encoding of address of the node associated with the payload. For instance, the main use case is when module recived and message from src node `5`; the uart message will be `0x00 0x05 | message from node 5` (the uart escape byte endoing still get applied on top of this). 

Other use cases are module's own address for module status message or debug use to pass 2 byte critical informations.

### 5) Event Handler
The network module exercised callback based event handlers to abstract away lower level logics in `ble_mesh_config_root/edge.c` and keep higher level event handling logic in `main.c`. The event handlers are following:
- `prov_complete_handler` - Invoked when a node is provisioned and ready to join the network.
- `config_complete_handler` - Invoked when a node is configed and joined the network.
- `recv_message_handler` - Invoked when there is message from other node.
- `recv_response_handler` - Invoked when recived response to previously sent response-expected message.
- `timeout_handler` - Invoked when no response recived on previously sent response-expected message.
- `broadcast_handler` - Invoked when recived broadcast message from any node.
- `connectivity_handler` - Invoked when recived connectivity check (heartbeat) message from other node.

OPTIONAL:
Explain what defined can off, or how to change the app or net keIDid, or NetworkConfig, or even if they want to add another opcode or something

### Error Handling
- Message bounce issue (ttl)
- Buffer under read message (uart left over)
- Provisioned edge bug when root reset (root reset, edge missed reset signal)
- Power drain too small (might happen)
- 


OPTIONAL:
potencial error and warning and current fix.

## Testing and Troubleshooting

## References
