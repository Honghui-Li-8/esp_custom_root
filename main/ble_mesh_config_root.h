#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_timer.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_rpr_model_api.h"


#include "mesh/adapter.h"
#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"

#include "../Secret/NetworkConfig.h"

#ifndef _BLE_ROOT_H_
#define _BLE_ROOT_H_

void printNetworkInfo();
void set_message_ttl(uint8_t new_ttl);
void send_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr, bool require_response);
void broadcast_message(uint16_t length, uint8_t *data_ptr);
void send_response(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *data_ptr, uint32_t message_opcode);
// vvv important message functions
void send_important_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr);
int8_t get_important_message_index(uint32_t opcode);
void retransmit_important_message(esp_ble_mesh_msg_ctx_t* ctx_ptr, uint32_t opcode, int8_t index);
void clear_important_message(int8_t index);
// ^^^ important message functions
void reset_esp32();
esp_err_t esp_module_root_init(
    void (*prov_complete_handler)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx),
    void (*config_complete_handler)(uint16_t addr),
    void (*recv_message_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr, uint32_t opcode),
    void (*recv_response_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr, uint32_t opcode),
    void (*timeout_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode),
    void (*broadcast_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*connectivity_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr));

#endif /* _BLE_ROOT_H_ */