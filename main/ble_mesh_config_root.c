/* main.c - Application main entry point */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

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


#define TAG TAG_ROOT
#define TAG_W "Debug"
#define TAG_INFO "Net_Info"

//maybe i'll move it to networkConfig.h
// #define MSG_SEND_TTL        3
// #define MSG_TIMEOUT         0
#define MSG_ROLE            ROLE_PROVISIONER
#define COMP_DATA_1_OCTET(msg, offset)      (msg[offset])
#define COMP_DATA_2_OCTET(msg, offset)      (msg[offset + 1] << 8 | msg[offset])

static uint8_t remote_dev_uuid_match[2] = {0x55, 0x55};
static uint16_t cur_rpr_cli_opcode;
static uint16_t remote_rpr_srv_addr = 0;
uint8_t message_tid = 0;

typedef struct {
    uint8_t  uuid[16];
    uint16_t unicast;
    uint8_t  elem_num;
    uint8_t  onoff;
    uint8_t *sig_model_num;
    uint8_t *vnd_model_num;
    uint16_t **sig_models;
    uint32_t **vnd_models;
} esp_ble_mesh_node_info_t;

static uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN];
static struct esp_ble_mesh_key {
    uint16_t net_idx;
    uint16_t app_idx;
    uint8_t  app_key[ESP_BLE_MESH_OCTET16_LEN];
} ble_mesh_key;

static esp_ble_mesh_node_info_t nodes[CONFIG_BLE_MESH_MAX_PROV_NODES] = {
    [0 ... (CONFIG_BLE_MESH_MAX_PROV_NODES - 1)] = {
        .unicast = ESP_BLE_MESH_ADDR_UNASSIGNED,
        .elem_num = 0,
    }
};

// static nvs_handle_t NVS_HANDLE;
// static const char * NVS_KEY = NVS_KEY_ROOT;

#define MSG_ROLE MSG_ROLE_ROOT


static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_DISABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

#if CONFIG_BLE_MESH_RPR_CLI
static esp_ble_mesh_client_t remote_prov_client;
#endif
static esp_ble_mesh_client_t config_client;

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
#if CONFIG_BLE_MESH_RPR_CLI
    ESP_BLE_MESH_MODEL_RPR_CLI(&remote_prov_client),
#endif
};


static const esp_ble_mesh_client_op_pair_t client_op_pair[] = {
    { ECS_193_MODEL_OP_MESSAGE, ECS_193_MODEL_OP_RESPONSE },
    { ECS_193_MODEL_OP_BROADCAST, NULL },
};

static esp_ble_mesh_client_t ecs_193_client = {
    .op_pair_size = ARRAY_SIZE(client_op_pair),
    .op_pair = client_op_pair,
};


static esp_ble_mesh_model_op_t client_op[] = { // operation client will "RECEIVED"
    ESP_BLE_MESH_MODEL_OP(ECS_193_MODEL_OP_RESPONSE, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_op_t server_op[] = { // operation server will "RECEIVED"
    ESP_BLE_MESH_MODEL_OP(ECS_193_MODEL_OP_MESSAGE, 2),
    ESP_BLE_MESH_MODEL_OP(ECS_193_MODEL_OP_BROADCAST, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models[] = { // custom models
    ESP_BLE_MESH_VENDOR_MODEL(ECS_193_CID, ECS_193_MODEL_ID_CLIENT, client_op, NULL, &ecs_193_client), 
    ESP_BLE_MESH_VENDOR_MODEL(ECS_193_CID, ECS_193_MODEL_ID_SERVER, server_op, NULL, NULL),
};

static esp_ble_mesh_model_t *client_model = &vnd_models[0];
static esp_ble_mesh_model_t *server_model = &vnd_models[1];

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = { // composition of current module
    .cid = ECS_193_CID,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = {
    .prov_uuid          = dev_uuid,
    .prov_unicast_addr  = PROV_OWN_ADDR,
    .prov_start_address = PROV_START_ADDR,
    .prov_attention      = 0x00,
    .prov_algorithm      = 0x00,
    .prov_pub_key_oob    = 0x00,
    .prov_static_oob_val = NULL,
    .prov_static_oob_len = 0x00,
    .flags               = 0x00,
    .iv_index            = 0x00,
};


// -------------------- application level callback functions ------------------
static void (*prov_complete_handler_cb)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx) = NULL;
static void (*recv_message_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) = NULL;
static void (*recv_response_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) = NULL;
static void (*timeout_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode) = NULL;
static void (*broadcast_handler_cb)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr) = NULL;

//-------------------- Network Functions ----------------
void printNetworkInfo()
{
    ESP_LOGW(TAG, "----------- Current Network Info--------------");
    uint16_t node_count = esp_ble_mesh_provisioner_get_prov_node_count();

    ESP_LOGI(TAG_INFO, "Node Count: %d\n\n", node_count);
    const esp_ble_mesh_node_t **nodeTableEntry = esp_ble_mesh_provisioner_get_node_table_entry();

    // Iterate over each node in the table
    for (int i = 0; i < node_count; i++)
    {
        const esp_ble_mesh_node_t *node = nodeTableEntry[i];

        char uuid_str[(16 * 2) + 1]; // Static buffer to hold the string representation
        for (int i = 0; i < 16; i++)
        {
            sprintf(&uuid_str[i * 2], "%02X", node->dev_uuid[i]); // Convert each byte of the UUID to hexadecimal and store it in the string
        }
        uuid_str[16 * 2] = '\0';

        ESP_LOGI(TAG_INFO, "Node Name: %s", node->name);
        ESP_LOGI(TAG_INFO, "     Address: %hu", node->unicast_addr);
        ESP_LOGI(TAG_INFO, "     uuid: %s", uuid_str);
    }

    ESP_LOGW(TAG, "----------- End of Network Info--------------");
}

static esp_err_t example_ble_mesh_store_node_info(const uint8_t uuid[16], uint16_t unicast,
                                                  uint8_t elem_num)
{
    int i;

    if (!uuid || !ESP_BLE_MESH_ADDR_IS_UNICAST(unicast)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Judge if the device has been provisioned before */
    for (i = 0; i < ARRAY_SIZE(nodes); i++) {
        if (!memcmp(nodes[i].uuid, uuid, 16)) {
            ESP_LOGW(TAG, "%s: reprovisioned device 0x%04x", __func__, unicast);
            nodes[i].unicast = unicast;
            nodes[i].elem_num = elem_num;
            return ESP_OK;
        }
    }

    for (i = 0; i < ARRAY_SIZE(nodes); i++) {
        if (nodes[i].unicast == ESP_BLE_MESH_ADDR_UNASSIGNED) {
            memcpy(nodes[i].uuid, uuid, 16);
            nodes[i].unicast = unicast;
            nodes[i].elem_num = elem_num;
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

static esp_ble_mesh_node_info_t *example_ble_mesh_get_node_info(uint16_t unicast)
{
    int i;

    if (!ESP_BLE_MESH_ADDR_IS_UNICAST(unicast)) {
        return NULL;
    }

    for (i = 0; i < ARRAY_SIZE(nodes); i++) {
        if (nodes[i].unicast <= unicast &&
                nodes[i].unicast + nodes[i].elem_num > unicast) {
            return &nodes[i];
        }
    }

    return NULL;
}


static void ble_mesh_set_msg_common(esp_ble_mesh_client_common_param_t *common,
                                            uint16_t unicast,
                                            esp_ble_mesh_model_t *model, uint32_t opcode)
{
    if (!common || !unicast || !model) {
        return ESP_ERR_INVALID_ARG;
    }

    common->opcode = opcode;
    common->model = model;
    common->ctx.net_idx = ble_mesh_key.net_idx;
    common->ctx.app_idx = ble_mesh_key.app_idx;
    common->ctx.addr = unicast;
    common->ctx.send_ttl = MSG_SEND_TTL;
    common->msg_timeout = MSG_TIMEOUT;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common->msg_role = MSG_ROLE_ROOT;
#endif
}

static void prov_link_open(esp_ble_mesh_prov_bearer_t bearer)
{
    ESP_LOGI(TAG, "%s link open", bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
}

static void prov_link_close(esp_ble_mesh_prov_bearer_t bearer, uint8_t reason)
{
    ESP_LOGI(TAG, "%s link close, reason 0x%02x",
             bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT", reason);
}

void example_ble_mesh_send_remote_provisioning_scan_start(void)
{

    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    if (!remote_rpr_srv_addr) {
        ESP_LOGE(TAG, "No valid remote provisioning server address");
        return;
    }

    /* Send a ESP_BLE_MESH_MODEL_OP_RPR_SCAN_GET to get the scan status of remote provisioning server */
    example_ble_mesh_set_msg_common(&common, remote_rpr_srv_addr, remote_prov_client.model, ESP_BLE_MESH_MODEL_OP_RPR_SCAN_GET);
    err = esp_ble_mesh_rpr_client_send(&common, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Remote Provisioning Client msg: Scan Get");
    }
    cur_rpr_cli_opcode = ESP_BLE_MESH_MODEL_OP_RPR_SCAN_GET;
}

static void example_ble_mesh_parse_node_comp_data(esp_ble_mesh_node_info_t* node, const uint8_t *data, uint16_t length)
{
    uint16_t cid, pid, vid, crpl, feat;
    uint16_t loc, model_id, company_id;
    uint8_t nums, numv;
    uint16_t offset;
    uint8_t seq = 0;
    int i;

    if (!node || !data) {
        ESP_LOGE(TAG, "Invalid Argument");
        return;
    }

    cid = COMP_DATA_2_OCTET(data, 0);
    pid = COMP_DATA_2_OCTET(data, 2);
    vid = COMP_DATA_2_OCTET(data, 4);
    crpl = COMP_DATA_2_OCTET(data, 6);
    feat = COMP_DATA_2_OCTET(data, 8);
    offset = 10;

    node->sig_model_num = (uint8_t *)calloc(node->elem_num, sizeof(uint8_t));
    if (!node->sig_model_num) {
        ESP_LOGW(TAG, "No Free memory to store composition data");
        return;
    }

    node->vnd_model_num = (uint8_t *)calloc(node->elem_num, sizeof(uint8_t));
    if (!node->vnd_model_num) {
        ESP_LOGW(TAG, "No Free memory to store composition data");
        return;
    }

    node->sig_models = (uint16_t **)calloc(node->elem_num, sizeof(uint16_t*));
    if (!node->sig_models) {
        ESP_LOGW(TAG, "No Free memory to store composition data");
        return;
    }

    node->vnd_models = (uint32_t **)calloc(node->elem_num, sizeof(uint32_t*));
    if (!node->sig_models) {
        ESP_LOGW(TAG, "No Free memory to store composition data");
        return;
    }

    ESP_LOGI(TAG, "********************** Composition Data Start **********************");
    ESP_LOGI(TAG, "* CID 0x%04x, PID 0x%04x, VID 0x%04x, CRPL 0x%04x, Features 0x%04x *", cid, pid, vid, crpl, feat);
    for (; offset < length; ) {
        loc = COMP_DATA_2_OCTET(data, offset);
        nums = COMP_DATA_1_OCTET(data, offset + 2);
        numv = COMP_DATA_1_OCTET(data, offset + 3);
        node->sig_model_num[seq] = nums;
        node->vnd_model_num[seq] = numv;

        if (nums) {
            node->sig_models[seq] = (uint16_t *)calloc(nums, sizeof(uint16_t));
            if (!(node->sig_models[seq])) {
                ESP_LOGW(TAG, "No Free memory to store composition data");
                return;
            }
        } else {
            node->sig_models[seq] = NULL;
        }

        if (numv) {
            node->vnd_models[seq] = (uint32_t *)calloc(numv, sizeof(uint32_t));
            if (!(node->vnd_models[seq])) {
                ESP_LOGW(TAG, "No Free memory to store composition data");
                return;
            }
        } else {
            node->vnd_models[seq] = NULL;
        }

        offset += 4;
        ESP_LOGI(TAG, "* Loc 0x%04x, NumS 0x%02x, NumV 0x%02x *", loc, nums, numv);
        for (i = 0; i < nums; i++) {
            model_id = COMP_DATA_2_OCTET(data, offset);
            node->sig_models[seq][i] = model_id;
            ESP_LOGI(TAG, "* SIG Model ID 0x%04x *", model_id);
            offset += 2;
        }
        for (i = 0; i < numv; i++) {
            company_id = COMP_DATA_2_OCTET(data, offset);
            model_id = COMP_DATA_2_OCTET(data, offset + 2);
            node->vnd_models[seq][i] = company_id << 16 | model_id;
            ESP_LOGI(TAG, "* Vendor Model ID 0x%04x, Company ID 0x%04x *", model_id, company_id);
            offset += 4;
        }
        seq++;
    }
    ESP_LOGI(TAG, "*********************** Composition Data End ***********************");
}

static void example_ble_mesh_config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
                                              esp_ble_mesh_cfg_client_cb_param_t *param)
{
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_cfg_client_set_state_t set = {0};
    esp_ble_mesh_node_t *node = NULL;
    esp_err_t err;

    ESP_LOGI(TAG, "Config client, err_code %d, event %u, addr 0x%04x, opcode 0x%04" PRIx32,
        param->error_code, event, param->params->ctx.addr, param->params->opcode);

    if (param->error_code) {
        ESP_LOGE(TAG, "Send config client message failed, opcode 0x%04" PRIx32, param->params->opcode);
        return;
    }

    node = example_ble_mesh_get_node_info(param->params->ctx.addr);
    if (!node) {
        ESP_LOGE(TAG, "%s: Get node info failed", __func__);
        return;
    }

    switch (event) {
    case ESP_BLE_MESH_CFG_CLIENT_GET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET) {
            ESP_LOGI(TAG, "composition data %s", bt_hex(param->status_cb.comp_data_status.composition_data->data,
                    param->status_cb.comp_data_status.composition_data->len));
            example_ble_mesh_parse_node_comp_data(node, param->status_cb.comp_data_status.composition_data->data,
                                                        param->status_cb.comp_data_status.composition_data->len);
            // err = esp_ble_mesh_provisioner_store_node_comp_data(param->params->ctx.addr,
            //     param->status_cb.comp_data_status.composition_data->data,
            //     param->status_cb.comp_data_status.composition_data->len);
            // if (err != ESP_OK) {
            //     ESP_LOGE(TAG, "Failed to store node composition data");
            //     break;
            // }

            ble_mesh_set_msg_common(&common, param->params->ctx.addr, config_client.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
            set.app_key_add.net_idx = ble_mesh_key.net_idx;
            set.app_key_add.app_idx = ble_mesh_key.app_idx;
            memcpy(set.app_key_add.app_key, ble_mesh_key.app_key, ESP_BLE_MESH_OCTET16_LEN);
            err = esp_ble_mesh_config_client_set_state(&common, &set);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Config AppKey Add");
            }
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD) {
            ble_mesh_set_msg_common(&common, node, config_client.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
            set.model_app_bind.element_addr = node->unicast_addr;
            set.model_app_bind.model_app_idx = ble_mesh_key.app_idx;
            set.model_app_bind.model_id = ECS_193_MODEL_ID_SERVER;
            set.model_app_bind.company_id = ECS_193_CID;
            err = esp_ble_mesh_config_client_set_state(&common, &set);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Config Model App Bind");
            }
        } else if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND) {
            ESP_LOGW(TAG, "%s, Provision and config successfully", __func__);
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_PUBLISH_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_STATUS) {
            ESP_LOG_BUFFER_HEX("Composition data", param->status_cb.comp_data_status.composition_data->data,
                param->status_cb.comp_data_status.composition_data->len);
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
        switch (param->params->opcode) {
        case ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET: {
            esp_ble_mesh_cfg_client_get_state_t get = {0};
            ble_mesh_set_msg_common(&common, param->params->ctx.addr, config_client.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
            get.comp_data_get.page = COMP_DATA_PAGE_0;
            err = esp_ble_mesh_config_client_get_state(&common, &get);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Config Composition Data Get");
            }
            break;
        }
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ble_mesh_set_msg_common(&common, param->params->ctx.addr, config_client.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
            set.app_key_add.net_idx = ble_mesh_key.net_idx;
            set.app_key_add.app_idx = ble_mesh_key.app_idx;
            memcpy(set.app_key_add.app_key, ble_mesh_key.app_key, ESP_BLE_MESH_OCTET16_LEN);
            err = esp_ble_mesh_config_client_set_state(&common, &set);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Config AppKey Add");
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ble_mesh_set_msg_common(&common, param->params->ctx.addr, config_client.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
            set.model_app_bind.element_addr = param->params->ctx.addr;
            set.model_app_bind.model_app_idx = ble_mesh_key.app_idx;
            set.model_app_bind.model_id = ECS_193_MODEL_ID_SERVER;
            set.model_app_bind.company_id = ECS_193_CID;
            err = esp_ble_mesh_config_client_set_state(&common, &set);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Config Model App Bind");
            }
            break;
        default:
            break;
        }
        break;
    default:
        ESP_LOGE(TAG, "Invalid config client event %u", event);
        break;
    }
}

static esp_err_t prov_complete(uint16_t node_index, const esp_ble_mesh_octet16_t uuid,
                               uint16_t primary_addr, uint8_t element_num, uint16_t net_idx)
{
    // Root Module only, intiate configuration of edge node
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_cfg_client_get_state_t get = {0};
    esp_ble_mesh_node_t *node = NULL;
    char name[10] = {'\0'};
    esp_err_t err;

    ESP_LOGI(TAG, "node_index %u, primary_addr 0x%04x, element_num %u, net_idx 0x%03x",
        node_index, primary_addr, element_num, net_idx);
    ESP_LOG_BUFFER_HEX("uuid", uuid, ESP_BLE_MESH_OCTET16_LEN);

    sprintf(name, "%s%02x", "NODE-", node_index);
    err = esp_ble_mesh_provisioner_set_node_name(node_index, name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set node name");
        return ESP_FAIL;
    }

    err = example_ble_mesh_store_node_info(uuid, primary_addr, element_num);
    if (err) {
        ESP_LOGE(TAG, "%s: Store node info failed", __func__);
        return ESP_FAIL;
    }

    node = example_ble_mesh_get_node_info(primary_addr);
    if (!node) {
        ESP_LOGE(TAG, "%s: Get node info failed", __func__);
        return ESP_FAIL;
    }

    ble_mesh_set_msg_common(&common, primary_addr, config_client.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
    get.comp_data_get.page = COMP_DATA_PAGE_0;
    err = esp_ble_mesh_config_client_get_state(&common, &get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Config Composition Data Get");
        return ESP_FAIL;
    }

    // return ESP_OK;
    // // End of Root Module intiate configuration fo edge node


    // application level callback, let main() know provision is completed
    prov_complete_handler_cb(node_index, uuid, primary_addr, element_num, net_idx);  //==================== app level callback

    return ESP_OK;
}

static void recv_unprov_adv_pkt(uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN], uint8_t addr[BD_ADDR_LEN],
                                esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{
    esp_ble_mesh_unprov_dev_add_t add_dev = {0};
    esp_err_t err;

    /* Due to the API esp_ble_mesh_provisioner_set_dev_uuid_match, Provisioner will only
     * use this callback to report the devices, whose device UUID starts with 0xdd & 0xdd,
     * to the application layer.
     */

    ESP_LOG_BUFFER_HEX("Device address", addr, BD_ADDR_LEN);
    ESP_LOGI(TAG, "Address type 0x%02x, adv type 0x%02x", addr_type, adv_type);
    ESP_LOG_BUFFER_HEX("Device UUID", dev_uuid, ESP_BLE_MESH_OCTET16_LEN);
    ESP_LOGI(TAG, "oob info 0x%04x, bearer %s", oob_info, (bearer & ESP_BLE_MESH_PROV_ADV) ? "PB-ADV" : "PB-GATT");

    memcpy(add_dev.addr, addr, BD_ADDR_LEN);
    add_dev.addr_type = (uint8_t)addr_type;
    memcpy(add_dev.uuid, dev_uuid, ESP_BLE_MESH_OCTET16_LEN);
    add_dev.oob_info = oob_info;
    add_dev.bearer = (uint8_t)bearer;
    /* Note: If unprovisioned device adv packets have not been received, we should not add
             device with ADD_DEV_START_PROV_NOW_FLAG set. */
    err = esp_ble_mesh_provisioner_add_unprov_dev(&add_dev,
            ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning device");
    }
}

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{ 
    switch (event) {
    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
        prov_complete(param->provisioner_prov_complete.node_idx, param->provisioner_prov_complete.device_uuid,
                      param->provisioner_prov_complete.unicast_addr, param->provisioner_prov_complete.element_num,
                      param->provisioner_prov_complete.netkey_idx);
        break;
    case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT");
        recv_unprov_adv_pkt(param->provisioner_recv_unprov_adv_pkt.dev_uuid, param->provisioner_recv_unprov_adv_pkt.addr,
                            param->provisioner_recv_unprov_adv_pkt.addr_type, param->provisioner_recv_unprov_adv_pkt.oob_info,
                            param->provisioner_recv_unprov_adv_pkt.adv_type, param->provisioner_recv_unprov_adv_pkt.bearer);
        break;
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        // mesh_example_info_restore(); /* Restore proper mesh example info */
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT, err_code %d", param->provisioner_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT, err_code %d", param->provisioner_prov_disable_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT, bearer %s",
            param->provisioner_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT, bearer %s, reason 0x%02x",
            param->provisioner_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT", param->provisioner_prov_link_close.reason);
        break;
    case ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT, err_code %d", param->provisioner_add_unprov_dev_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT, err_code %d", param->provisioner_set_dev_uuid_match_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT, err_code %d", param->provisioner_set_node_name_comp.err_code);
        if (param->provisioner_set_node_name_comp.err_code == 0) {
            const char *name = esp_ble_mesh_provisioner_get_node_name(param->provisioner_set_node_name_comp.node_index);
            if (name) {
                ESP_LOGI(TAG, "Node %d name %s", param->provisioner_set_node_name_comp.node_index, name);
            }
        }
        break;
    case ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT, err_code %d", param->provisioner_add_app_key_comp.err_code);
        if (param->provisioner_add_app_key_comp.err_code == 0) {
            ble_mesh_key.app_idx = param->provisioner_add_app_key_comp.app_idx;
            esp_err_t err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(PROV_OWN_ADDR, ble_mesh_key.app_idx,
                    ECS_193_MODEL_ID_CLIENT, ECS_193_CID);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to bind AppKey to custom client");
            }
            err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(PROV_OWN_ADDR, ble_mesh_key.app_idx,
                    ECS_193_MODEL_ID_SERVER, ECS_193_CID);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to bind AppKey to custom server");
                return;
            }
        }
        break;
    case ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT, err_code %d", param->provisioner_bind_app_key_to_model_comp.err_code);
        break;
    case ESP_BLE_MESH_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT, err_code %d", param->provisioner_store_node_comp_data_comp.err_code);
        break;
    default:
        break;
    }
}

static void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param)
{
    // static int64_t start_time;

    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        if (param->model_operation.opcode == ECS_193_MODEL_OP_MESSAGE) {
            recv_message_handler_cb(param->model_operation.ctx, param->model_operation.length, param->model_operation.msg);
        } else if (param->model_operation.opcode == ECS_193_MODEL_OP_RESPONSE) {
            recv_response_handler_cb(param->model_operation.ctx, param->model_operation.length, param->model_operation.msg);
        } else if (param->model_operation.opcode == ECS_193_MODEL_OP_BROADCAST) {
            broadcast_handler_cb(param->model_operation.ctx, param->model_operation.length, param->model_operation.msg);
        }
        break;
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code) {
            ESP_LOGE(TAG, "Failed to send message 0x%06" PRIx32, param->model_send_comp.opcode);
            break;
        }
        // start_time = esp_timer_get_time();
        ESP_LOGI(TAG, "Send opcode [0x%06" PRIx32 "] completed", param->model_send_comp.opcode);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:

        ESP_LOGI(TAG, "Receive publish message 0x%06" PRIx32, param->client_recv_publish_msg.opcode);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
        ESP_LOGW(TAG, "Client message 0x%06" PRIx32 " timeout", param->client_send_timeout.opcode);
        timeout_handler_cb(param->client_send_timeout.ctx, param->client_send_timeout. opcode);
        break;
    default:
        ESP_LOGE(TAG, "Uncaught Event");
        break;
    }
}

static void example_ble_mesh_remote_prov_client_callback(esp_ble_mesh_rpr_client_cb_event_t event,
                                        esp_ble_mesh_rpr_client_cb_param_t *param)
{
    static uint8_t remote_dev_uuid[16] = {0};
    esp_ble_mesh_rpr_client_msg_t msg = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;
    uint16_t addr = 0;

    switch (event) {
    case ESP_BLE_MESH_RPR_CLIENT_SEND_COMP_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Send Comp, err_code %d", param->send.err_code);
        break;
    case ESP_BLE_MESH_RPR_CLIENT_SEND_TIMEOUT_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Send Timeout, opcode 0x%04x, to 0x%04x",
                 param->send.params->opcode, param->send.params->ctx.addr);
        break;
    case ESP_BLE_MESH_RPR_CLIENT_RECV_PUB_EVT:
    case ESP_BLE_MESH_RPR_CLIENT_RECV_RSP_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Recv RSP, opcode 0x%04x, from 0x%04x",
                 param->recv.params->ctx.recv_op, param->recv.params->ctx.addr);
        switch (param->recv.params->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_RPR_SCAN_CAPS_STATUS:
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_SCAN_STATUS:
            addr = param->recv.params->ctx.addr;
            ESP_LOGI(TAG, "scan_status, status 0x%02x", param->recv.val.scan_status.status);
            ESP_LOGI(TAG, "scan_status, rpr_scanning 0x%02x", param->recv.val.scan_status.rpr_scanning);
            ESP_LOGI(TAG, "scan_status, scan_items_limit 0x%02x", param->recv.val.scan_status.scan_items_limit);
            ESP_LOGI(TAG, "scan_status, timeout 0x%02x", param->recv.val.scan_status.timeout);
            switch (cur_rpr_cli_opcode) {
            case ESP_BLE_MESH_MODEL_OP_RPR_SCAN_GET: {
                if (param->recv.val.scan_status.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    switch (param->recv.val.scan_status.rpr_scanning) {
                        /**
                         *  If the remote provisioning server's scan state is idle,
                         *  that state indicates that remote provisioning server could
                         *  start scan process.
                         */
                        case ESP_BLE_MESH_RPR_SCAN_IDLE: {
                            err = example_ble_mesh_set_msg_common(&common, addr, remote_prov_client.model,
                                                                    ESP_BLE_MESH_MODEL_OP_RPR_SCAN_START);
                            if (err != ESP_OK) {
                                ESP_LOGE(TAG, "Set message common fail:%d", __LINE__);
                                return ;
                            }

                            msg.scan_start.scan_items_limit = 0; /* 0 indicates there is no limit for scan items' count */
                            msg.scan_start.timeout = 0x0A;       /* 0x0A is the default timeout */
                            msg.scan_start.uuid_en = 0;          /* If uuid enabled, a specify device which have the same uuid will be report */
                                                                 /* If uuid disable, any unprovision device all will be report */

                            err = esp_ble_mesh_rpr_client_send(&common, &msg);
                            if (err != ESP_OK) {
                                ESP_LOGE(TAG, "Failed to send Remote Provisioning Client msg: Scan start");
                            }
                            cur_rpr_cli_opcode = ESP_BLE_MESH_MODEL_OP_RPR_SCAN_START;
                            break;
                        }
                        default:
                            ESP_LOGW(TAG, "Remote Provisioning Server(addr: 0x%04x) Busy", addr);
                            break;
                    }
                } else {
                    ESP_LOGE(TAG, "Remote Provisioning Client Scan Get Fail");
                }
            }
                break;
            case ESP_BLE_MESH_MODEL_OP_RPR_SCAN_START: {
                if (param->recv.val.scan_status.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    ESP_LOGI(TAG, "Start Remote Provisioning Server(addr: 0x%04x) Scan Success", addr);
                } else {
                    ESP_LOGE(TAG, "Remote Provisioning Client Scan Start Fail");
                }
                break;
            }
            default:
                ESP_LOGW(TAG, "Unknown Process opcode 0x%04x:%d", cur_rpr_cli_opcode,__LINE__);
                break;
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_SCAN_REPORT:
            addr = param->recv.params->ctx.addr;
            ESP_LOGI(TAG, "scan_report, rssi %ddBm", param->recv.val.scan_report.rssi);
            ESP_LOG_BUFFER_HEX(TAG": scan_report, uuid", param->recv.val.scan_report.uuid, 16);
            ESP_LOGI(TAG, "scan_report, oob_info 0x%04x", param->recv.val.scan_report.oob_info);
            ESP_LOGI(TAG, "scan_report, uri_hash 0x%08x", param->recv.val.scan_report.uri_hash);

            if (param->recv.val.scan_report.uuid[0] != remote_dev_uuid_match[0] ||
                param->recv.val.scan_report.uuid[1] != remote_dev_uuid_match[1]) {
                ESP_LOGI(TAG, "This device is not expect device");
                return;
            }

            memcpy(remote_dev_uuid, param->recv.val.scan_report.uuid, 16);

            /* Send ESP_BLE_MESH_MODEL_OP_RPR_LINK_GET to remote provisioning server get link status */
            err = example_ble_mesh_set_msg_common(&common, addr, remote_prov_client.model
                                        , ESP_BLE_MESH_MODEL_OP_RPR_LINK_GET);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Set message common fail:%d", __LINE__);
                return;
            }

            err = esp_ble_mesh_rpr_client_send(&common, NULL);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send Remote Provisioning Client msg:Link Get");
            }

            cur_rpr_cli_opcode = ESP_BLE_MESH_MODEL_OP_RPR_LINK_GET;
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_EXT_SCAN_REPORT:
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_LINK_STATUS:
            addr = param->recv.params->ctx.addr;
            ESP_LOGI(TAG, "link_status, status 0x%02x", param->recv.val.link_status.status);
            ESP_LOGI(TAG, "link_status, rpr_state 0x%02x", param->recv.val.link_status.rpr_state);
            switch (cur_rpr_cli_opcode) {
            case ESP_BLE_MESH_MODEL_OP_RPR_LINK_GET: {
                if (param->recv.val.link_status.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    switch (param->recv.val.link_status.rpr_state) {
                    case ESP_BLE_MESH_RPR_LINK_IDLE:
                        /**
                         *  Link status is idle, send ESP_BLE_MESH_MODEL_OP_RPR_LINK_OPEN
                         *  to remote provisioning server to open prov link
                         */
                        err = example_ble_mesh_set_msg_common(&common, addr, remote_prov_client.model
                                                    , ESP_BLE_MESH_MODEL_OP_RPR_LINK_OPEN);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "Set message common fail:%d", __LINE__);
                            return;
                        }

                        msg.link_open.uuid_en = 1;
                        memcpy(msg.link_open.uuid, remote_dev_uuid, 16);
                        msg.link_open.timeout_en = 0;

                        err = esp_ble_mesh_rpr_client_send(&common, &msg);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to send Remote Provisioning Client msg:Link open");
                        }
                        cur_rpr_cli_opcode = ESP_BLE_MESH_MODEL_OP_RPR_LINK_OPEN;
                        break;
                    default:
                        ESP_LOGW(TAG, "Remote Provisioning Server(addr: 0x%04x) Busy", addr);
                        break;
                    }
                }
                break;
            }
            case ESP_BLE_MESH_MODEL_OP_RPR_LINK_OPEN: {
                if (param->recv.val.link_status.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Recv Link Open Success", addr);
                } else {
                    ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Recv Link Open Fail", addr);
                }
            }
                break;
            case ESP_BLE_MESH_MODEL_OP_RPR_LINK_CLOSE: {
                if (param->recv.val.link_status.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Recv Link Close Success", addr);
                } else {
                    ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Recv Link Close Fail", addr);
                }
            }
                break;
            default:
                ESP_LOGW(TAG, "Unknown Process opcode 0x%04x:%d", cur_rpr_cli_opcode,__LINE__);
                break;
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_LINK_REPORT:
            addr = param->recv.params->ctx.addr;
            ESP_LOGI(TAG, "link_report, status 0x%02x", param->recv.val.link_report.status);
            ESP_LOGI(TAG, "link_report, rpr_state 0x%02x", param->recv.val.link_report.rpr_state);
            if (param->recv.val.link_report.reason_en) {
                ESP_LOGI(TAG, "link_report, reason 0x%02x", param->recv.val.link_report.reason);
            }
            switch (cur_rpr_cli_opcode) {
            case ESP_BLE_MESH_MODEL_OP_RPR_LINK_OPEN:
                if (param->recv.val.link_report.status == ESP_BLE_MESH_RPR_STATUS_SUCCESS) {
                    switch (param->recv.val.link_report.rpr_state)
                    {
                    case ESP_BLE_MESH_RPR_LINK_ACTIVE:
                        ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Link Open Success", addr);
                        esp_ble_mesh_rpr_client_act_param_t param = {0};
                        param.start_rpr.model = remote_prov_client.model;
                        param.start_rpr.rpr_srv_addr = addr;

                        /* Let remote provisioning server start provisioning */
                        err = esp_ble_mesh_rpr_client_action(ESP_BLE_MESH_RPR_CLIENT_ACT_START_RPR,
                                                                        &param);
                        if (err) {
                            ESP_LOGE(TAG, "Failed to perform Remote Provisioning Client action: Start Prov");
                        }
                        break;
                    default:
                        ESP_LOGI(TAG, "Remote Provisioning Server(addr: 0x%04x) Status error", addr);
                        break;
                    }
                } else {
                    ESP_LOGW(TAG, "Remote Provisioning Server(addr: 0x%04x) Link open fail");
                }
                break;
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_RPR_LINK_CLOSE:
            switch (param->recv.val.link_report.status)
            {
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_BY_CLIENT:
                ESP_LOGI(TAG, "Link closed by client");
                break;
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_BY_DEVICE:
                ESP_LOGI(TAG, "Link closed by device");
                break;
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_BY_SERVER:
                ESP_LOGI(TAG, "Link closed by server");
                break;
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_AS_CANNOT_RECEIVE_PDU:
                ESP_LOGI(TAG, "Link closed as cannot receive pdu");
                break;
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_AS_CANNOT_SEND_PDU:
                ESP_LOGI(TAG, "Link closed as cannot send pdu");
                break;
            case ESP_BLE_MESH_RPR_STATUS_LINK_CLOSED_AS_CANNOT_DELIVER_PDU_REPORT:
                ESP_LOGI(TAG, "Link closed as cannot send pdu report");
                break;
            default:
                ESP_LOGW(TAG, "Unknown link close status, %d", param->recv.val.link_report.status);
                break;
            }
            break;
        default:
            ESP_LOGW(TAG, "Unknown Process opcode 0x%04x:%d", cur_rpr_cli_opcode,__LINE__);
            break;
        }
        break;
    case ESP_BLE_MESH_RPR_CLIENT_ACT_COMP_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Act Comp, sub_evt 0x%02x", param->act.sub_evt);
        switch (param->act.sub_evt) {
        case ESP_BLE_MESH_START_RPR_COMP_SUB_EVT:
            ESP_LOGI(TAG, "Start Remote Prov Comp, err_code %d, rpr_srv_addr 0x%04x",
                     param->act.start_rpr_comp.err_code,
                     param->act.start_rpr_comp.rpr_srv_addr);
            break;
        default:
            ESP_LOGE(TAG, "Unknown Remote Provisioning Client sub event");
            break;
        }
        break;
    case ESP_BLE_MESH_RPR_CLIENT_LINK_OPEN_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Link Open");
        break;
    case ESP_BLE_MESH_RPR_CLIENT_LINK_CLOSE_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Link Close");
        break;
    case ESP_BLE_MESH_RPR_CLIENT_PROV_COMP_EVT:
        ESP_LOGW(TAG, "Remote Prov Client Prov Complete");
        ESP_LOGI(TAG, "Net Idx: 0x%04x", param->prov.net_idx);
        ESP_LOGI(TAG, "Node addr: 0x%04x", param->prov.unicast_addr);
        ESP_LOGI(TAG, "Node element num: 0x%04x", param->prov.element_num);
        ESP_LOG_BUFFER_HEX(TAG": Node UUID: ", param->prov.uuid, 16);
        err = example_ble_mesh_set_msg_common(&common, param->prov.rpr_srv_addr, remote_prov_client.model,
                                              ESP_BLE_MESH_MODEL_OP_RPR_LINK_CLOSE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set message common fail:%d", __LINE__);
            return;
        }
        msg.link_close.reason = ESP_BLE_MESH_RPR_REASON_SUCCESS;

        err = esp_ble_mesh_rpr_client_send(&common, &msg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send Remote Provisioning Client msg:Link open");
        }
        cur_rpr_cli_opcode = ESP_BLE_MESH_MODEL_OP_RPR_LINK_CLOSE;

        prov_complete(param->prov.net_idx,  param->prov.uuid,
            param->prov.unicast_addr, param->prov.element_num, param->prov.net_idx);
        break;
    default:
        break;
    }
}

void send_message(uint16_t dst_address, uint16_t length, uint8_t *data_ptr)
{
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode = ECS_193_MODEL_OP_MESSAGE;
    esp_ble_mesh_dev_role_t message_role = MSG_ROLE;
    esp_err_t err;

    // ESP_LOGW(TAG, "net_idx: %" PRIu16, ble_mesh_key.net_idx);
    // ESP_LOGW(TAG, "app_idx: %" PRIu16, ble_mesh_key.app_idx);
    // ESP_LOGW(TAG, "dst_address: %" PRIu16, dst_address);

    // check if node is in network
    esp_ble_mesh_node_t *node = NULL;
    node = esp_ble_mesh_provisioner_get_node_with_addr(dst_address);
    if (node == NULL)
    {
        ESP_LOGE(TAG, "Node 0x%04x not exists in network", dst_address);
        return;
    }

    ctx.net_idx = ble_mesh_key.net_idx;
    ctx.app_idx = ble_mesh_key.app_idx;
    ctx.addr = dst_address;
    ctx.send_ttl = MSG_SEND_TTL;
    

    err = esp_ble_mesh_client_model_send_msg(client_model, &ctx, opcode, length, data_ptr, MSG_TIMEOUT, true, message_role);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send message to node addr 0x%04x", dst_address);
        return;
    }

    // ESP_LOGW(TAG, "Message [%s] sended to [0x%04x]", (char*) data_ptr, dst_address);
}

void send_broadcast(uint16_t length, uint8_t *data_ptr)
{
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode = ECS_193_MODEL_OP_BROADCAST;
    esp_ble_mesh_dev_role_t message_role = MSG_ROLE;
    esp_err_t err = ESP_OK;

    // ESP_LOGW(TAG, "net_idx: %" PRIu16, ble_mesh_key.net_idx);
    // ESP_LOGW(TAG, "app_idx: %" PRIu16, ble_mesh_key.app_idx);
    // ESP_LOGW(TAG, "dst_address: %" PRIu16, dst_address);

    ctx.net_idx = ble_mesh_key.net_idx;
    ctx.app_idx = ble_mesh_key.app_idx;
    ctx.addr = 0xFFFF;
    ctx.send_ttl = MSG_SEND_TTL;
    

    err = esp_ble_mesh_client_model_send_msg(client_model, &ctx, opcode, length, data_ptr, MSG_TIMEOUT, true, message_role);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send message to node addr 0xFFFF, err_code %d", err);
        return;
    }
}


void send_response(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *data_ptr)
{
    uint32_t opcode = ECS_193_MODEL_OP_RESPONSE;
    esp_err_t err;

    // ESP_LOGW(TAG, "response net_idx: %" PRIu16, ctx->net_idx);
    // ESP_LOGW(TAG, "response app_idx: %" PRIu16, ctx->app_idx);
    // ESP_LOGW(TAG, "response addr: %" PRIu16, ctx->addr);
    // ESP_LOGW(TAG, "response recv_dst: %" PRIu16, ctx->recv_dst);

    err = esp_ble_mesh_server_model_send_msg(server_model, ctx, opcode, length, data_ptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send message to node addr 0x%04x", ctx->addr);
        return;
    }
}

static esp_err_t ble_mesh_init(void)
{
    uint8_t match[2] = INIT_UUID_MATCH;
    esp_err_t err;

    ble_mesh_key.net_idx = ESP_BLE_MESH_KEY_PRIMARY;
    ble_mesh_key.app_idx = APP_KEY_IDX;
    memset(ble_mesh_key.app_key, APP_KEY_OCTET, sizeof(ble_mesh_key.app_key));

    esp_ble_mesh_register_config_client_callback(example_ble_mesh_config_client_cb);
    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);
    esp_ble_mesh_register_rpr_client_callback(example_ble_mesh_remote_prov_client_callback);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack");
        return err;
    }

    err = esp_ble_mesh_client_model_init(&vnd_models[0]);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize vendor client");
        return err;
    }

    err = esp_ble_mesh_provisioner_set_dev_uuid_match(match, sizeof(match), 0x0, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set matching device uuid");
        return err;
    }

    err = esp_ble_mesh_provisioner_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh provisioner");
        return err;
    }

    err = esp_ble_mesh_provisioner_add_local_app_key(ble_mesh_key.app_key, ble_mesh_key.net_idx, ble_mesh_key.app_idx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add local AppKey");
        return err;
    }

    ESP_LOGI(TAG, "ESP BLE Mesh Provisioner initialized");

    return ESP_OK;
}



static esp_err_t esp_module_root_init(
    void (*prov_complete_handler)(uint16_t node_index, const esp_ble_mesh_octet16_t uuid, uint16_t addr, uint8_t element_num, uint16_t net_idx),
    void (*recv_message_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*recv_response_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr),
    void (*timeout_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint32_t opcode),
    void (*broadcast_handler)(esp_ble_mesh_msg_ctx_t *ctx, uint16_t length, uint8_t *msg_ptr)
) {
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing Root Module...");

    // attach application level callback
    prov_complete_handler_cb = prov_complete_handler;
    recv_message_handler_cb = recv_message_handler;
    recv_response_handler_cb = recv_response_handler;
    timeout_handler_cb = timeout_handler;
    broadcast_handler_cb = broadcast_handler;
    
    if (prov_complete_handler_cb == NULL || recv_message_handler_cb == NULL || recv_response_handler_cb == NULL || timeout_handler_cb == NULL || broadcast_handler_cb == NULL) {
        ESP_LOGE(TAG, "Appliocation Level Callback functin is NULL");
        return ESP_FAIL;
    }

    
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

    // /* Open nvs namespace for storing/restoring mesh example info */
    // err = ble_mesh_nvs_open(&NVS_HANDLE);
    // if (err) {
    //     return ESP_FAIL;
    // }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
        return ESP_FAIL;
    }

    return ESP_OK;
}