#include "board.h"
#include "ble_mesh.c"

void app_main(void)
{
    esp_err_t err;

    err = esp_module_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ROOT, "Network Module Initialization failed (err %d)", err);
        return;
    }

    
    board_init();
}