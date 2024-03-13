#include "board.h"
#include "ble_mesh_config_root.c"

#define TAG "MAIN"

static void prov_complete_handler(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx) {
    ESP_LOGI(TAG, " ----------- prov_complete handler trigered -----------");

}

static void recv_message_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG, " ----------- recv_message handler trigered -----------");
    
}

static void recv_response_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG, " ----------- recv_response handler trigered -----------");
    
}

static void timeout_handler(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode) {
    ESP_LOGI(TAG, " ----------- timeout handler trigered -----------");
    
}


void app_main(void)
{
    esp_err_t err;

#if defined(ROOT_MODULE)
    err = esp_module_root_init(prov_complete_handler, recv_message_handler, recv_response_handler, timeout_handler);
#else
    err = esp_module_edge_init(prov_complete_handler, recv_message_handler, recv_response_handler, timeout_handler);
#endif

    if (err != ESP_OK) {
        ESP_LOGE(TAG_ROOT, "Network Module Initialization failed (err %d)", err);
        return;
    }


    
    board_init();
    ESP_LOGI(TAG, " ----------- app_main done -----------");
}