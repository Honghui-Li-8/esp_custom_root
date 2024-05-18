#include "board.h"
#include "ble_mesh_config_root.c"

#define TAG_M "MAIN"
#define TAG_ALL "*"

static void prov_complete_handler(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx) {
    ESP_LOGI(TAG_M, " ----------- prov_complete handler trigered -----------");

}

static void recv_message_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    // ESP_LOGI(TAG_M, " ----------- recv_message handler trigered -----------");
    ESP_LOGW(TAG_M, "-> Received Message [%s]", (char*)msg_ptr);

    static uint8_t *data_buffer = NULL;
    if (data_buffer == NULL) {
        data_buffer = (uint8_t*)malloc(128);
        if (data_buffer == NULL) {
            printf("Memory allocation failed.\n");
            return;
        }
    }
    
    strcpy((char*)data_buffer, "hello Edge, my code seems working fine");
    uint16_t response_length = strlen("hello Edge, my code seems working fine") + 1;

    send_response(ctx, response_length, data_buffer);
    ESP_LOGW(TAG_M, "<- Sended Response [%s]", (char*)data_buffer);
}

static void recv_response_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    // ESP_LOGI(TAG_M, " ----------- recv_response handler trigered -----------");
    ESP_LOGW(TAG_M, "-> Received Response [%s]", (char*)msg_ptr);

}

static void timeout_handler(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode) {
    ESP_LOGI(TAG_M, " ----------- timeout handler trigered -----------");

}

static void broadcast_handler(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) {
    ESP_LOGI(TAG_M, "Receive Broadcast from Root\n");
    
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

static void execute_command(char* command) {
    ESP_LOGI(TAG_M, "execute_command called");
    static const char *TAG_E = "EXE";

    if (strlen(command) < 6) {
        ESP_LOGE(TAG_E, "Command [%s] too short", command);
        return;
    }

    if (strncmp(command, "INFO", 4) == 0) {
        printNetworkInfo();
    }
    else if (strncmp(command, "LOGOF", 5) == 0) {
        esp_log_level_set(TAG_ALL, ESP_LOG_NONE);
        uart_sendData(TAG_M, "[UART] Turning off all Log's from esp_log\n");
    }
    else if (strncmp(command, "LOGON", 5) == 0) {
        esp_log_level_set(TAG_ALL, ESP_LOG_ERROR);
        esp_log_level_set(TAG_ALL, ESP_LOG_WARN);
        esp_log_level_set(TAG_ALL, ESP_LOG_INFO);
        esp_log_level_set(TAG_ALL, ESP_LOG_DEBUG);
        esp_log_level_set(TAG_ALL, ESP_LOG_VERBOSE);
        uart_sendData(TAG_M, "[UART] Turning on all Log's from esp_log\n");
    }
    else if (strncmp(command, "SEND", 4) == 0) {
        ESP_LOGI(TAG_E, "executing [SEND]");
        char spliter[] = "-";
        char *address_start = command + 4 + strlen(spliter);
        char *data_start = strstr(address_start, spliter);
        if (data_start == NULL) {
            ESP_LOGE(TAG_E, "No send data found");
            return;
        }
        data_start = data_start + strlen(spliter); // pass the spliter

        int max_addr_len = 3;
        int addr_len = data_start - address_start - 1;
        if (addr_len > max_addr_len) {
            // address too long
            ESP_LOGE(TAG_E, "Address is too long");
            return;
        }

        char addr_str[max_addr_len + 1]; // Ensure enough space for the string and null terminator
        strncpy(addr_str, address_start, addr_len);
        addr_str[addr_len] = '\0';

        int addr = atoi(addr_str);
        ESP_LOGI(TAG_E, "Sending message to address-%d ...", addr);
        send_message(addr, strlen(data_start), (uint8_t *) data_start);
        ESP_LOGW(TAG_M, "<- Sended Message [%s]", (char*)data_start);
    }
    else {
        ESP_LOGE(TAG_E, "Command not Vaild");
    }

    
    ESP_LOGI(TAG_E, "Command [%s] executed", command);
}

static void uart_task_handler(char *data) {
    ESP_LOGW(TAG_M, "uart_task_handler called ------------------");

    const char delimiter[] = "\n";
    char *command;
    
    command = strtok(data, delimiter);
    while (command != NULL) {
        // printf("Executing: [%s]\n", command);
        execute_command(command);

        // get next command
        command = strtok(NULL, delimiter);
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
            data[rxBytes] = 0; // mark end of string
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);

            uart_task_handler((char*) data);
        }
    }
    free(data);
}

void app_main(void)
{
    esp_err_t err;

#if defined(ROOT_MODULE)
    err = esp_module_root_init(prov_complete_handler, recv_message_handler, recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
#else
    err = esp_module_edge_init(prov_complete_handler, recv_message_handler, recv_response_handler, timeout_handler, broadcast_handler, connectivity_handler);
#endif

    if (err != ESP_OK) {
        ESP_LOGE(TAG_M, "Network Module Initialization failed (err %d)", err);
        return;
    }

    
    board_init();

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    ESP_LOGI(TAG, "done uart_rx_task");

    ESP_LOGI(TAG, " ----------- app_main done -----------");
}