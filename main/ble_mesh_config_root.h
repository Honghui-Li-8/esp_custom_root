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

/**
 * @brief Print Network Nodes in dev logs (esp log function)
 *
 * This funcrtion is used for debuging and will be turn off when debug (dev) logs are turn off in main()
 */
void printNetworkInfo();

/**
 * @brief set the message ttl for current module.
 */
void set_message_ttl(uint8_t new_ttl);

/**
 * @brief Send Message (bytes) to another node in network
 *
 * @param dst_address  Dstination node's unicast address
 * @param length Length of message (bytes)
 * @param data_ptr pointer to data buffer that holds message
 * @param require_response flag that indicate if this message expecting response, timeout will get triger if response not recived
 */
void send_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr, bool require_response);

/**
 * @brief Broadcast Message (bytes) to all node in network
 *
 * @param length Length of message (bytes)
 * @param data_ptr pointer to data buffer that holds message
 */
void broadcast_message(uint16_t length, uint8_t *data_ptr);

/**
 * @brief Send Response (bytes) to an recived message
 *
 * @param ctx message context of incoming message, will be used for sending response to that message to src node
 * @param length Length of message (bytes)
 * @param data_ptr pointer to data buffer that holds message
 * @param message_opcode opcode of incoming message, used to determine corresponding response opcode
 */
void send_response(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *data_ptr, uint32_t message_opcode);

/**
 * @brief Send an Important Message (bytes) to an node
 * 
 *  This function send and tracks an important message, it will retransmit the important message with higher ttl after timeout for 2 times.
 *
 * @param dst_address  Dstination node's unicast address
 * @param length Length of message (bytes)
 * @param data_ptr pointer to data buffer that holds message
 */
void send_important_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr);

/**
 * @brief Get the on-tracking Important Message's index
 *
 * @param opcode Message or Response opcode of an important message
 */
int8_t get_important_message_index(uint32_t opcode);

/**
 * @brief Retransmit an Important Message (bytes) to an node
 * 
 *  This function retransmit the important message with higher ttl after timeout for 2 times.
 *
 * @param ctx_ptr poninter to message context of timeout important message
 * @param opcode Message opcode of an important message
 * @param index Index  of on-tracking Important Message
 */
void retransmit_important_message(esp_ble_mesh_msg_ctx_t* ctx_ptr, uint32_t opcode, int8_t index);

/**
 * @brief Clear the tracking on an Important Message
 * 
 *  This function clears the tracking on 1) successful transmission wwith response 2) faild all transmission
 * 
 * @param index Index  of on-tracking Important Message
 */
void clear_important_message(int8_t index);

/**
 * @brief Reset the module and Erase persistent memeory if persistent memeory is enabled
 */
void reset_esp32();

/**
 * @brief Initialize Root module and attach event handler callback functions
 * 
 * @param prov_complete_handler Callback function triggered on edge node provisioning completion
 * @param config_complete_handler Callback function triggered on edge node configuration completion
 * @param recv_message_handler Callback function triggered on reciving incoming message
 * @param recv_response_handler Callback function triggered on receiving incoming response to previously sent message
 * @param timeout_handler Callback function triggered on timeout on previously sent message without expected response
 * @param broadcast_handler_cb Callback function triggered on reciving incoming broadcase message
 * @param connectivity_handler_cb Callback function triggered on reciving incoming connectivity message (heartbeat connection check)
 */
esp_err_t esp_module_root_init(
    void (*prov_complete_handler)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx),
    void (*config_complete_handler)(uint16_t addr),
    void (*recv_message_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr, uint32_t opcode),
    void (*recv_response_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr, uint32_t opcode),
    void (*timeout_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode),
    void (*broadcast_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*connectivity_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr));

#endif /* _BLE_ROOT_H_ */