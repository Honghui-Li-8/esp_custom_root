#include "board.h"
#include "ble_mesh_config_root.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h> // for host byte endianess <--> network byte endianess convert
#include "esp_log.h"

#define TAG_M "MAIN"
#define TAG_ALL "*"
#define OPCODE_LEN 1
#define NODE_ADDR_LEN 2  // can't change bc is base on esp
#define NODE_UUID_LEN 16 // can't change bc is base on esp
#define CMD_LEN 5 // network command length - 5 byte
#define CMD_GET_NET_INFO "NINFO"
#define CMD_SEND_MSG "SEND-"
#define CMD_BROADCAST_MSG "BCAST"
#define CMD_RESET_ROOT "RST-R"
#define CMD_CLEAN_NETWORK_CONFIG "CLEAN"

/***************** Event Handler *****************/
static void prov_complete_handler(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t node_addr, uint8_t element_num, uint16_t net_idx) {
    ESP_LOGI(TAG_M, " ----------- prov_complete handler trigered -----------");
    uart_sendMsg(node_addr, " ----------- prov_complete -----------\n");
}

static void config_complete_handler(uint16_t node_addr) {
    ESP_LOGI(TAG_M,  " ----------- Node-0x%04x config_complete -----------", node_addr);
    uart_sendMsg(node_addr, " ----------- config_complete -----------\n");
    // 16 byte uuid on node
    uint8_t node_data_size = NODE_ADDR_LEN + NODE_UUID_LEN; // node_addr + node_uuid size
    uint8_t buffer_size = OPCODE_LEN + node_data_size; // 1 byte opcode, 16 byte node_uuid
    uint8_t* buffer = (uint8_t*) malloc(buffer_size * sizeof(uint8_t));

    buffer[0] = 0x02; // node info
    uint8_t* buffer_itr = buffer + OPCODE_LEN;
    esp_ble_mesh_node_t *node_ptr = esp_ble_mesh_provisioner_get_node_with_addr(node_addr);
    if (node_ptr == NULL) {
        uart_sendMsg(node_addr, "Error, can get node that's just configed");
        free(buffer);
        return;
    }

    // load node data
    memcpy(buffer_itr, node_ptr->dev_uuid, NODE_UUID_LEN);
    buffer_itr += NODE_UUID_LEN;

    uart_sendData(node_addr, buffer, buffer_itr-buffer);
    free(buffer);
    printNetworkInfo(); // esp log for debug
}

static void recv_message_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr, uint32_t opcode) {
    // ESP_LOGI(TAG_M, " ----------- recv_message handler trigered -----------");
    uint16_t node_addr = ctx->addr;
    ESP_LOGW(TAG_M, "-> Received Message \'%s\' from node-%d", (char*)msg_ptr, node_addr);

    // recived a ble-message from edge ndoe
    // ========== potential special case ==========
    if (strncmp((char*)msg_ptr, "Special Case", 12) == 0) {
        // place holder for special case that need to be handled in esp-root module
        // handle locally
        char response[5] = "S";
        uint16_t response_length = strlen(response);
        send_response(ctx, response_length, (uint8_t*) response);
        ESP_LOGW(TAG_M, "<- Sended Response \'%s\'", (char*) response);
    }
    
    // ========== General case, pass up to APP level ==========
    // pass node_addr & data to network server through uart
    else {
        uart_sendData(node_addr, msg_ptr, length);
    }

    // check if needs an response to confirm recived
    if (opcode != ECS_193_MODEL_OP_MESSAGE_R) {
        return;
    }
    // send response
    char response[5] = "S";
    uint16_t response_length = strlen(response);
    send_response(ctx, response_length, (uint8_t *)response);
    ESP_LOGW(TAG_M, "<- Sended Response %d bytes \'%*s\'", response_length, response_length, (char *)response);
}

static void recv_response_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    // ESP_LOGI(TAG_M, " ----------- recv_response handler trigered -----------");
    ESP_LOGW(TAG_M, "-> Recived Response \'%s\'", (char*)msg_ptr);

}

static void timeout_handler(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode) {
    ESP_LOGI(TAG_M, " ----------- timeout handler trigered -----------");
    
}

static void broadcast_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    if (ctx->addr == PROV_OWN_ADDR) {
        // uart_sendMsg(0, "Broadcasted");
        return; // is root's own broadcast
    }

    uint16_t node_addr = ctx->addr;
    ESP_LOGW(TAG_M, "-> Received Broadcast Message \'%s\' from node-%d", (char *)msg_ptr, node_addr);

    // ========== General case, pass up to APP level ==========
    // pass node_addr & data to to edge device using uart
    uart_sendData(node_addr, msg_ptr, length);
}

static void connectivity_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG_M, "----- Connectivity Handler Triggered -----\n");

    char response[3] = "S";
    uint16_t response_length = strlen(response);
    send_response(ctx, response_length, (uint8_t *)response);
}

/***************** Other Functions *****************/
static void send_network_info() {
    // craft bytes of network info and send to uart
    uint16_t node_count = esp_ble_mesh_provisioner_get_prov_node_count();
    uint16_t node_left = node_count;
    const esp_ble_mesh_node_t **nodeTableEntry = esp_ble_mesh_provisioner_get_node_table_entry();

    // 18 byte per node, send up to 40 node everytime
    uint8_t node_data_size = NODE_ADDR_LEN + NODE_UUID_LEN; // node_addr + node_uuid size
    uint8_t buffer_size = OPCODE_LEN + 1 + 40 * node_data_size; // 3 byte opcode, 1 byte node amount, up to 40 node
    uint8_t* buffer = (uint8_t*) malloc(buffer_size * sizeof(uint8_t));

    buffer[0] = 0x01; //network info

    int node_index = 0;
    while (node_left > 0)
    {
        const esp_ble_mesh_node_t *node_itr = nodeTableEntry[node_index];
        // compute current ctach, max 40
        uint8_t batch_size = (node_left < 40 ? node_left : 40);
        uint8_t* buffer_itr = buffer + OPCODE_LEN;

        // load batch size (1 byte node amount)
        *buffer_itr = batch_size;
        ++buffer_itr;

        // load all node data in this batch
        for (int i = 0; i < batch_size; ++i) {
            // load current node
            uint16_t node_addr = node_itr->unicast_addr;
            uint16_t node_addr_network_endian = htons(node_addr);

            memcpy(buffer_itr, &node_addr_network_endian, NODE_ADDR_LEN);
            buffer_itr += NODE_ADDR_LEN;
            memcpy(buffer_itr, node_itr->dev_uuid, NODE_UUID_LEN);
            buffer_itr += NODE_UUID_LEN;

            // move to next node
            node_index += 1;
        }

        uart_sendData(0, buffer, buffer_itr-buffer);
        node_left -= batch_size;
    }
    free(buffer);
}

static void execute_uart_command(char* command, size_t cmd_total_len) {
    ESP_LOGI(TAG_M, "execute_command called");
    static const char *TAG_E = "EXE";
    static uint8_t *data_buffer = NULL;
    if (data_buffer == NULL) {
        data_buffer = (uint8_t*)malloc(128);
        if (data_buffer == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
    }

    // ============= process and execute commands from net server (from uart) ==================
    // uart command format
    // TB Finish, TB Complete
    if (cmd_total_len < 5) {
        ESP_LOGE(TAG_E, "Command [%s] with %d byte too short", command, cmd_total_len);
        uart_sendMsg(0, "Error: Command Too Short\n");
        return;
    }

    // ====== core commands ====== 
    if (strncmp(command, CMD_GET_NET_INFO, CMD_LEN) == 0) {
        send_network_info();
    } 
    else if (strncmp(command, CMD_SEND_MSG, CMD_LEN) == 0) {
        ESP_LOGI(TAG_E, "executing \'SEND-\'");
        char *address_start = command + CMD_LEN;
        char *msg_start = address_start + NODE_ADDR_LEN;
        size_t msg_length = cmd_total_len - CMD_LEN - NODE_ADDR_LEN;

        if (cmd_total_len < CMD_LEN + NODE_ADDR_LEN) {
            uart_sendMsg(0, "Error: No Dst Address Attached\n");
            return;
        } else if (msg_length <= 0) {
            uart_sendMsg(0, "Error: No Message Attached\n");
            return;
        }

        uint16_t node_addr_network_order = 0;
        memcpy(&node_addr_network_order, address_start, 2);
        uint16_t node_addr = ntohs(node_addr_network_order);
        if (node_addr == 0)
        {
            node_addr = PROV_OWN_ADDR; // root addr
        }
        
        ESP_LOGI(TAG_E, "Sending message to address-%d ...", node_addr);
        send_message(node_addr, msg_length, (uint8_t *) msg_start, false);
        ESP_LOGW(TAG_M, "<- Sended Message [%s]", (char *)msg_start);
    } 
    else if (strncmp(command, CMD_BROADCAST_MSG, CMD_LEN) == 0) {
        ESP_LOGI(TAG_E, "executing \'BCAST\'");
        char *msg_start = command + CMD_LEN + NODE_ADDR_LEN;
        size_t msg_length = cmd_total_len - CMD_LEN - NODE_ADDR_LEN;

        broadcast_message(msg_length, (uint8_t *)msg_start);
    }
    else if (strncmp(command, CMD_RESET_ROOT, 5) == 0) {
        ESP_LOGI(TAG_E, "executing \'RST-R\'");
        esp_restart();
    }
    else if (strncmp(command, CMD_CLEAN_NETWORK_CONFIG, 5) == 0)
    {
        ESP_LOGI(TAG_E, "executing \'CLEAN\'");
        uart_sendMsg(0, " - Reseting Root Module\n");
        reset_esp32();
    }

    // ====== other dev/debug use command ====== 
    // else if (strncmp(command, "ECHO-", 5) == 0) {
    //     // echo test
    //     ESP_LOGW(TAG_M, "recived \'ECHO-\' command");
    //     strcpy((char*) data_buffer, command);
    //     strcpy(((char*) data_buffer) + strlen(command), "; [ESP] confirm recived from uart; \n");
    //     uart_sendData(0, data_buffer, strlen((char *)data_buffer) + 1);
    // }

    // ====== Not Supported  command ======
    else {
        ESP_LOGE(TAG_E, "Command not Vaild");
    }

    
    ESP_LOGI(TAG_E, "Command [%.*s] executed", cmd_total_len, command);
}

static void uart_task_handler(char *data) {
    ESP_LOGW(TAG_M, "uart_task_handler called ------------------");

    int cmd_start = 0;
    int cmd_end = 0;
    int cmd_len = 0;

    for (int i = 0; i < UART_BUF_SIZE; i++) {
        if (data[i] == 0xFF) {
            // located start of message
            cmd_start = i + 1; // start byte of actual message
        }else if (data[i] == 0xFE) {
            // located end of message
            cmd_end = i; // 0xFE byte
        }

        if (cmd_end > cmd_start) {
            // located a message, message at least 1 byte
            uint8_t* command = (uint8_t *) (data + cmd_start);
            cmd_len = cmd_end - cmd_start;
            cmd_len = uart_decoded_bytes(command, cmd_len, command); // decoded cmd will be put back to command pointer
            ESP_LOGE("Decoded Data", "i:%d, cmd_start:%d, cmd_len:%d", i, cmd_start, cmd_len);

            execute_uart_command(data + cmd_start, cmd_len); //TB Finish, don't execute at the moment
            cmd_start = cmd_end;
        }
    }

    if (cmd_start > cmd_end) {
        // one message is only been read half into buffer, edge case. Not consider at the moment
        ESP_LOGE("E", "Buffer might have remaining half message!! cmd_start:%d, cmd_end:%d", cmd_start, cmd_end);
        uart_sendMsg(0, "[Warning] Buffer might have remaining half message!!\n");
    }
}

static void rx_task(void *arg)
{
    // esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    // esp_log_level_set(RX_TASK_TAG, ESP_LOG_NONE);

    static const char *RX_TASK_TAG = "RX";
    uint8_t* data = (uint8_t*) malloc(UART_BUF_SIZE + 1);
    ESP_LOGW(RX_TASK_TAG, "rx_task called ------------------");

    while (1)
    {
        memset(data, 0, UART_BUF_SIZE);
        const int rxBytes = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            // uart_sendMsg(rxBytes, " readed from RX\n");

            uart_task_handler((char*) data);
        }
    }
    free(data);
}

void app_main(void)
{
    // turn off log - Important, bc the server counting on uart escape byte 0xff and 0xfe
    //              - So need to enforce all uart traffic
    //              - use uart_sendMsg or uart_sendData for message, the esp_log for dev debug
    esp_log_level_set(TAG_ALL, ESP_LOG_NONE);
    
    esp_err_t err = esp_module_root_init(prov_complete_handler, config_complete_handler, recv_message_handler, recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
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