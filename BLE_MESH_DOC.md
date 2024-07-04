ESP-IDF BLE MESH Root Documentation
==================================

- [ESP-IDF BLE MESH Root Documentation](#esp-idf-ble-mesh-root-documentation)
  - [Overview](#overview)
  - [Module Initialization](#module-initialization)
  - [BLE Client/Server Model](#ble-clientserver-model)
  - [Custom Opcodes](#custom-opcodes)
  - [Provisioning Process](#provisioning-process)
  - [Message Types](#message-types)
  - [Key Components](#key-components)
    - [`example_ble_mesh_config_client_cb`](#example_ble_mesh_config_client_cb)
    - [`ble_mesh_provisioning_cb`](#ble_mesh_provisioning_cb)
    - [`ble_mesh_custom_model_cb`](#ble_mesh_custom_model_cb)
    - [`example_ble_mesh_remote_prov_client_callback`](#example_ble_mesh_remote_prov_client_callback)
  - [References](#references)

## Overview
This markdown file specifically explain the `ble_mesh_config_root.c`. 

## Module Initialization
- Coming from `main.c`, we first start `esp_module_root_init`, attachign all the application level callback. We then check if all the callback function are passed on without any fails
  ```c
  if (prov_complete_handler_cb == NULL || recv_message_handler_cb == NULL || recv_response_handler_cb == NULL || timeout_handler_cb == NULL || broadcast_handler_cb == NULL || connectivity_handler_cb == NULL || config_complete_handler_cb == NULL) {
      ESP_LOGE(TAG, "Application Level Callback functin is NULL");
      return ESP_FAIL;
  }
  ```
- We then intialize the non-volatile storage flash, Bluetooth, and BLE Mesh.
  ```c
  esp_err_t err;

  err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return ESP_FAIL;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
        return ESP_FAIL;
    }
  ```

- In `ble_mesh_init()`, the device begins setting up most of the key features. The first step involves initializing the ble_mesh_key structure, which is defined at the top of the `ble_mesh_config_root.c` file:
  ```c
  static struct esp_ble_mesh_key {
    uint16_t net_idx;
    uint16_t app_idx;
    uint8_t  app_key[ESP_BLE_MESH_OCTET16_LEN];
  } ble_mesh_key;
  ```
  The ble_mesh_key structure holds essential information for BLE mesh networking, including network and application keys. We then assign values to its members using predefined variables from the header file:
  ```c
  ble_mesh_key.net_idx = ESP_BLE_MESH_KEY_PRIMARY;
  ble_mesh_key.app_idx = APP_KEY_IDX;
  ```
  Here, `ESP_BLE_MESH_KEY_PRIMARY` is used to set the primary network key index, and `APP_KEY_IDX` assigns the application key index. These assignments are crucial for the BLE mesh functionality to ensure secure and proper communication within the mesh network.
- Afterwards, four callback functions are registered to handle various aspects of BLE mesh functionality:
  ```c
  esp_ble_mesh_register_config_client_callback(example_ble_mesh_config_client_cb);
  esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
  esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);
  esp_ble_mesh_register_rpr_client_callback(example_ble_mesh_remote_prov_client_callback);
  ```
  - `example_ble_mesh_config_client_cb`: Handles configuration client events, such as receiving configuration status messages.
  - `ble_mesh_provisioning_cb`: Manages provisioning events, including provisioning start, complete, and failure events.
  - `ble_mesh_custom_model_cb`: Processes events related to custom BLE mesh models.
  - `example_ble_mesh_remote_prov_client_callback`: Deals with remote provisioning client events, enabling remote devices to be provisioned into the mesh network.
  
  For more details about each function, please go to the [Key Components](#key-components) which will explain the inside scope of each function.

## BLE Client/Server Model

## Custom Opcodes

## Provisioning Process

## Message Types

## Key Components
### `example_ble_mesh_config_client_cb`

**Overview**

This function (`example_ble_mesh_config_client_cb`) is a callback handler for handling events and parameters related to the Bluetooth Low Energy (BLE) Mesh configuration client. It manages various configuration operations such as retrieving composition data, adding application keys, and binding models.

**Functionality**

The function is designed to handle different events (`ESP_BLE_MESH_CFG_CLIENT_*_EVT`) and perform corresponding actions based on the received parameters (`esp_ble_mesh_cfg_client_cb_param_t`). Key functionalities include:

- Retrieving and parsing composition data.
- Storing node composition data if successful.
- Adding application keys and binding models as part of configuration.

**Integration**

To integrate this function into your BLE Mesh application, follow these steps:

1. **Initialization**: Ensure the BLE Mesh configuration client is properly initialized and registered with the appropriate models.

2. **Callback Registration**: Register this function as the callback handler for configuration client events (`ESP_BLE_MESH_CFG_CLIENT_EVT`).

3. **Handling Events**: Handle events (`ESP_BLE_MESH_CFG_CLIENT_*_EVT`) within this function by checking the event type (`event`) and executing corresponding logic based on `param`.

### `ble_mesh_provisioning_cb`

**Overview**

This function (`ble_mesh_provisioning_cb`) is a callback handler for handling events and parameters related to the Bluetooth Low Energy (BLE) Mesh provisioning process. It manages various provisioning events such as receiving unprovisioned device advertisements, provisioning completion, and link management.

**Functionality**

The function is designed to handle different provisioning events (`ESP_BLE_MESH_PROVISIONER_*_EVT`) and perform corresponding actions based on the received parameters (`esp_ble_mesh_prov_cb_param_t`). Key functionalities include:

- Handling the completion of the provisioning process.
- Receiving unprovisioned device advertisements.
- Managing provisioning links (open and close).
- Adding and binding local application keys to models.
- Storing node composition data.

**Integration**

To integrate this function into your BLE Mesh application, follow these steps:

1. **Initialization**: Ensure the BLE Mesh provisioning functionality is properly initialized and the provisioning is enabled (`provision_enable`).

2. **Callback Registration**: Register this function as the callback handler for provisioning events (`ESP_BLE_MESH_PROV_EVT`).

3. **Handling Events**: Handle events (`ESP_BLE_MESH_PROVISIONER_*_EVT`) within this function by checking the event type (`event`) and executing corresponding logic based on `param`.

### `ble_mesh_custom_model_cb`


### `example_ble_mesh_remote_prov_client_callback`

**Overview**

This function (`example_ble_mesh_remote_prov_client_callback`) is a callback handler for handling events and parameters related to the Bluetooth Low Energy (BLE) Mesh Remote Provisioning Client. It manages various remote provisioning events such as sending and receiving messages, handling link operations, and completing provisioning.

**Functionality**

The function handles different remote provisioning client events (`ESP_BLE_MESH_RPR_CLIENT_*_EVT`) and performs corresponding actions based on the received parameters (`esp_ble_mesh_rpr_client_cb_param_t`). Key functionalities include:

- Handling the completion of remote provisioning client message sending and timeouts.
- Managing received publication and response messages from the remote provisioning server.
- Handling remote provisioning scan and link operations.
- Completing the provisioning process and closing links.

**Integration**

To integrate this function into your BLE Mesh application, follow these steps:

1. **Initialization**: Ensure the BLE Mesh Remote Provisioning Client functionality is properly initialized and the callback function is registered.

2. **Callback Registration**: Register this function as the callback handler for remote provisioning client events (`ESP_BLE_MESH_RPR_CLIENT_EVT`).

3. **Handling Events**: Handle events (`ESP_BLE_MESH_RPR_CLIENT_*_EVT`) within this function by checking the event type (`event`) and executing corresponding logic based on `param`.

## References
