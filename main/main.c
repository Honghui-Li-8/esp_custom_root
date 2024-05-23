#include "board.h"
#include "ble_mesh_config_root.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

#define TAG_M "MAIN"
#define TAG_ALL "*"

static void prov_complete_handler(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx) {
    ESP_LOGI(TAG_M, " ----------- prov_complete handler trigered -----------");
    uart_sendMsg(0,  " ----------- prov_complete -----------");

}

static void config_complete_handler(uint16_t addr) {
    ESP_LOGI(TAG_M,  " ----------- Node-0x%04x config_complete -----------", addr);
    uart_sendMsg(0,  " ----------- config_complete -----------");
}

static void recv_message_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    // ESP_LOGI(TAG_M, " ----------- recv_message handler trigered -----------");
    uint16_t node_addr = ctx->addr;
    ESP_LOGW(TAG_M, "-> Received Message \'%s\' from node-%d", (char*)msg_ptr, node_addr);

    static uint8_t *data_buffer = NULL;
    if (data_buffer == NULL) {
        data_buffer = (uint8_t*)malloc(128);
        if (data_buffer == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
    }

    // recived a ble-message from edge ndoe - TB Finish
    //  - leave all the task hadling logic to app level (python server)
    //  - when recived ble-message, pass it directly to net-server
    //  - if there is sepcial case in future, add if/switch case here
    // ========== potential special case ==========
    if (strncmp((char*)msg_ptr, "Special Case", 1) == 0) {
        // Data update on node
        // hadle locally
        return;
    }
    
    // cases for testing
    if (strncmp((char*)msg_ptr, "Ehco Test", 1) == 0){
        
        strcpy((char*)data_buffer, "hello Edge, my code seems working fine");
        uint16_t response_length = strlen("hello Edge, my code seems working fine") + 1;

        send_response(ctx, response_length, data_buffer);
        ESP_LOGW(TAG_M, "<- Sended Response \'%s\'", (char*)data_buffer);
        return;
    }

    // ========== Data update cases ==========
    // ========== Edge Request cases ==========
    // pass node_addr & data to network server through uart
    uart_sendData(node_addr, msg_ptr, length);
    // TB Finish, any other thing need to be done in network level?

    // send response to conirm recive
    strcpy((char*)data_buffer, "ACK");
    uint16_t response_length = strlen("ACK");

    send_response(ctx, response_length, data_buffer);
    ESP_LOGW(TAG_M, "<- Sended Response \'%s\'", (char*)data_buffer);
}

static void recv_response_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    // ESP_LOGI(TAG_M, " ----------- recv_response handler trigered -----------");
    ESP_LOGW(TAG_M, "-> Recived Response \'%s\'", (char*)msg_ptr);

}

static void timeout_handler(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode) {
    ESP_LOGI(TAG_M, " ----------- timeout handler trigered -----------");
    
}

static void broadcast_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG_M, "Receive Broadcast from Root\n");	    ESP_LOGI(TAG_M, "Receive Broadcast from Root\n");
    	    
    return ;
}

static void connectivity_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG_M, "----- Connectivity Handler Triggered -----\n");

    static uint8_t *data_buffer = NULL;
    if (data_buffer == NULL) {
        data_buffer = (uint8_t*)malloc(128);
        if (data_buffer == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
    }

    strcpy((char*)data_buffer, "hello Edge, you're connected still");
    uint16_t response_length = strlen("hello Edge, you're connected still") + 1;

    send_response(ctx, response_length, data_buffer);

    return ;
}

static void execute_uart_command(char* command, size_t cmd_len) {
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
    if (strlen(command) < 5) {
        ESP_LOGE(TAG_E, "Command [%s] too short", command);
        return;
    }
    const size_t CMD_LEN = 5;
    const size_t ADDR_LEN = 2;
    const size_t MSG_SIZE_NUM_LEN = 1;

    // ====== core commands ====== 
    if (strncmp(command, "INFO-", 5) == 0) {
        printNetworkInfo();
    } else if (strncmp(command, "SEND-", 5) == 0) {
        ESP_LOGI(TAG_E, "executing \'SEND-\'");
        char *address_start = command + CMD_LEN;
        char *msg_len_start = address_start + ADDR_LEN;
        char *msg_start = msg_len_start + MSG_SIZE_NUM_LEN;

        uint16_t node_addr = (uint16_t)((address_start[0] << 8) | address_start[1]);
        size_t msg_length = (size_t)msg_len_start[0];

        ESP_LOGI(TAG_E, "Sending message to address-%d ...", node_addr);
        send_message(node_addr, msg_length, (uint8_t *) msg_start);
        ESP_LOGW(TAG_M, "<- Sended Message [%s]", (char*) msg_start);
    }


    // ====== other dev/debug use command ====== 
    else if (strncmp(command, "ECHO-", 5) == 0) {
        // echo test
        ESP_LOGW(TAG_M, "recived \'ECHO-\' command");
        strcpy((char*) data_buffer, command);
        strcpy(((char*) data_buffer) + strlen(command), "; [ESP] confirm recived from uart; \n");
        uart_sendMsg(0, (char*)data_buffer);
    } else {
        ESP_LOGE(TAG_E, "Command not Vaild");
    }

    
    ESP_LOGI(TAG_E, "Command [%.*s] executed", cmd_len, command);
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
            cmd_end = i;  // 0xFE byte
        }

        if (cmd_end > cmd_start) {
            // located a message, message at least 1 byte
            uint8_t* command = (uint8_t *) (data + cmd_start);
            cmd_len = cmd_end - cmd_start;
            cmd_len = uart_decoded_bytes(command, cmd_len, command); // decoded cmd will be put back to command pointer
            ESP_LOGE("Decoded Data", "i:%d, cmd_start:%d, cmd_len:%d", i, cmd_start, cmd_len);

            execute_uart_command(data + cmd_start, cmd_len); //TB Finish, don't execute at the moment
        }
    }

    if (cmd_start > cmd_end) {
        // one message is only been read half into buffer, edge case. Not consider at the moment
        ESP_LOGE("E", "Buffer might have remaining half message!! cmd_start:%d, cmd_end:%d", cmd_start, cmd_end);
        uart_sendMsg(0, "[Error] Buffer might have remaining half message!!\n");
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(UART_BUF_SIZE + 1);
    ESP_LOGW(RX_TASK_TAG, "rx_task called ------------------");

    while (1) {
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
    // turn off log - important, bc the server counting on '[E]' as end of message instaed of '\0'
    //              - since the message from uart carries data
    //              - use uart_sendMsg or uart_sendData for message, the esp_log for dev debug
    // esp_log_level_set(TAG_ALL, ESP_LOG_NONE);
    // uart_sendMsg(0, "[UART] Turning off all Log's from esp_log\n");
    
    esp_err_t err = esp_module_root_init(prov_complete_handler, config_complete_handler, recv_message_handler, recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_M, "Network Module Initialization failed (err %d)", err);
        return;
    }

    board_init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    
    uart_sendMsg(0, "[UART] ----------- app_main done -----------\n");
}