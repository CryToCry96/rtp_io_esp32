/* FreeModbus Slave ESP32

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "mdns.h"
#include "esp_netif.h"

#include "mbcontroller.h"       // for mbcontroller defines and api
#include "../include/modbus_params.h"      // for modbus parameters structures
#include "../include/di_do.h"

#define MB_TCP_PORT_NUMBER      (502)
#define MB_MDNS_PORT            (502)

// Defines below are used to define register start address for each type of Modbus registers
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) >> 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) >> 1))
#define MB_REG_DISCRETE_INPUT_START         (0x0000)
#define MB_REG_COILS_START                  (0x64) //Coil start address >>>> Change me for start address of coils
#define MB_REG_INPUT_START_AREA0            (INPUT_OFFSET(input_data0)) // register offset input area 0
#define MB_REG_INPUT_START_AREA1            (INPUT_OFFSET(input_data4)) // register offset input area 1
#define MB_REG_HOLDING_START_AREA0          (HOLD_OFFSET(holding_data0))
#define MB_REG_HOLDING_START_AREA1          (HOLD_OFFSET(holding_data4))

#define MB_PAR_INFO_GET_TOUT                (10) // Timeout for get parameter info
#define MB_CHAN_DATA_MAX_VAL                (10)
#define MB_CHAN_DATA_OFFSET                 (1.1f)

#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD \
                                                | MB_EVENT_HOLDING_REG_RD \
                                                | MB_EVENT_DISCRETE_RD \
                                                | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR \
                                                | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)

#define SLAVE_TAG "MODBUS_SLAVE"

static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

#if CONFIG_MB_MDNS_IP_RESOLVER

#define MB_ID_BYTE0(id) ((uint8_t)(id))
#define MB_ID_BYTE1(id) ((uint8_t)(((uint16_t)(id) >> 8) & 0xFF))
#define MB_ID_BYTE2(id) ((uint8_t)(((uint32_t)(id) >> 16) & 0xFF))
#define MB_ID_BYTE3(id) ((uint8_t)(((uint32_t)(id) >> 24) & 0xFF))

#define MB_ID2STR(id) MB_ID_BYTE0(id), MB_ID_BYTE1(id), MB_ID_BYTE2(id), MB_ID_BYTE3(id)

#if CONFIG_FMB_CONTROLLER_SLAVE_ID_SUPPORT
#define MB_DEVICE_ID (uint32_t)CONFIG_FMB_CONTROLLER_SLAVE_ID
#endif

#define MB_SLAVE_ADDR (CONFIG_MB_SLAVE_ADDR)

#define MB_MDNS_INSTANCE(pref) pref"mb_slave_tcp"

// convert mac from binary format to string
static inline char* gen_mac_str(const uint8_t* mac, char* pref, char* mac_str)
{
    sprintf(mac_str, "%s%02X%02X%02X%02X%02X%02X", pref, MAC2STR(mac));
    return mac_str;
}

static inline char* gen_id_str(char* service_name, char* slave_id_str)
{
    sprintf(slave_id_str, "%s%02X%02X%02X%02X", service_name, MB_ID2STR(MB_DEVICE_ID));
    return slave_id_str;
}

static inline char* gen_host_name_str(char* service_name, char* name)
{
    sprintf(name, "%s_%02X", service_name, MB_SLAVE_ADDR);
    return name;
}

static void start_mdns_service()
{
    char temp_str[32] = {0};
    uint8_t sta_mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
    char* hostname = gen_host_name_str(MB_MDNS_INSTANCE(""), temp_str);
    //initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(SLAVE_TAG, "mdns hostname set to: [%s]", hostname);

    //set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(MB_MDNS_INSTANCE("esp32_")));

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[] = {
        {"board","esp32"}
    };

    //initialize service
    ESP_ERROR_CHECK(mdns_service_add(hostname, "_modbus", "_tcp", MB_MDNS_PORT, serviceTxtData, 1));
    //add mac key string text item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_modbus", "_tcp", "mac", gen_mac_str(sta_mac, "\0", temp_str)));
    //add slave id key txt item
    ESP_ERROR_CHECK( mdns_service_txt_item_set("_modbus", "_tcp", "mb_id", gen_id_str("\0", temp_str)));
}

#endif

// Set register values into known state
static void setup_reg_data(void)
{
    // Define initial state of parameters
    // discrete_reg_params.discrete_input0 = 1;
    // discrete_reg_params.discrete_input1 = 1;
    // discrete_reg_params.discrete_input2 = 1;
    // discrete_reg_params.discrete_input3 = 1;
    // discrete_reg_params.discrete_input4 = 1;
    // discrete_reg_params.discrete_input5 = 1;
    // discrete_reg_params.discrete_input6 = 1;
    // discrete_reg_params.discrete_input7 = 0;
    
    //Value of Holding register address 100 (change in uint16_t test_regs[100];)
    // holding_reg_params.holding_data0 = 1.23;
    // holding_reg_params.holding_data1 = 2.56;
    // holding_reg_params.holding_data2 = 3.78;
    // holding_reg_params.holding_data3 = 4.90;
    // holding_reg_params.holding_data4 = 5.67;
    // holding_reg_params.holding_data5 = 6.78;
    // holding_reg_params.holding_data6 = 7.79;
    // holding_reg_params.holding_data7 = 8.80;

    //Start data for DI DO
    coil_reg_params.coils_DO = 0x00;
    coil_reg_params.coils_DI = 0xff; //Normal high (Pull-up)

    // input_reg_params.input_data0 = 1.12;
    // input_reg_params.input_data1 = 2.34;
    // input_reg_params.input_data2 = 3.56;
    // input_reg_params.input_data3 = 4.78;
    // input_reg_params.input_data4 = 1.12;
    // input_reg_params.input_data5 = 2.34;
    // input_reg_params.input_data6 = 3.56;
    // input_reg_params.input_data7 = 4.78;
}

// See deviceparams.h file for more information about assigned Modbus parameters.
// These parameters can be accessed from main application and also can be changed
// by external Modbus master host.
void tcp_slave(void *pvParameters)
{
#if CONFIG_MB_MDNS_IP_RESOLVER
    start_mdns_service();
#endif

    // Set UART log level
    esp_log_level_set(SLAVE_TAG, ESP_LOG_INFO);
    void* mbc_slave_handler = NULL;

    ESP_ERROR_CHECK(mbc_slave_init_tcp(&mbc_slave_handler)); // Initialization of Modbus controller

    mb_param_info_t reg_info; // keeps the Modbus registers access information
    mb_register_area_descriptor_t reg_area; // Modbus register area descriptor structure

    mb_communication_info_t comm_info = { 0 };
    comm_info.ip_port = MB_TCP_PORT_NUMBER;
#if !CONFIG_EXAMPLE_CONNECT_IPV6
    comm_info.ip_addr_type = MB_IPV4;
#else
    comm_info.ip_addr_type = MB_IPV6;
#endif
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = NULL;
    esp_netif_t* netif=NULL;
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); //Get IP from netif key
    comm_info.ip_netif_ptr = netif;
    // Setup communication parameters and start stack
    ESP_ERROR_CHECK(mbc_slave_setup((void*)&comm_info));

    // The code below initializes Modbus register area descriptors
    // for Modbus Holding Registers, Input Registers, Coils and Discrete Inputs
    // Initialization should be done for each supported Modbus register area according to register map.
    // When external master trying to access the register in the area that is not initialized
    // by mbc_slave_set_descriptor() API call then Modbus stack
    // will send exception response for this register area.
    // reg_area.type = MB_PARAM_HOLDING; // Set type of register area
    // reg_area.start_offset = MB_REG_HOLDING_START_AREA0; // Offset of register area in Modbus protocol
    // reg_area.address = (void*)&holding_reg_params.holding_data0; // Set pointer to storage instance
    // reg_area.size = sizeof(float) << 2; // Set the size of register storage instance
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    // ESP_LOGI(SLAVE_TAG, "Holding Address 1: %x", reg_area.start_offset);

    // reg_area.type = MB_PARAM_HOLDING; // Set type of register area
    // reg_area.start_offset = MB_REG_HOLDING_START_AREA1; // Offset of register area in Modbus protocol
    // reg_area.address = (void*)&holding_reg_params.holding_data4; // Set pointer to storage instance
    // reg_area.size = sizeof(float) << 2; // Set the size of register storage instance
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    // ESP_LOGI(SLAVE_TAG, "Holding Address 2: %x", reg_area.start_offset);

    // // Initialization of Input Registers area
    // reg_area.type = MB_PARAM_INPUT;
    // reg_area.start_offset = MB_REG_INPUT_START_AREA0;
    // reg_area.address = (void*)&input_reg_params.input_data0;
    // reg_area.size = sizeof(float) << 2;
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    // // Initialization of Input Registers area
    // reg_area.type = MB_PARAM_INPUT;
    // reg_area.start_offset = MB_REG_INPUT_START_AREA1;
    // reg_area.address = (void*)&input_reg_params.input_data4;
    // reg_area.size = sizeof(float) << 2;
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    // Initialization of Coils register area
    reg_area.type = MB_PARAM_COIL;
    reg_area.start_offset = MB_REG_COILS_START;
    reg_area.address = (void*)&coil_reg_params;
    reg_area.size = sizeof(coil_reg_params);
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
    //We are using two coils address
    ESP_LOGI(SLAVE_TAG, "Coil Address 1: 0x%x", reg_area.start_offset);
    ESP_LOGI(SLAVE_TAG, "Coil Address 2: 0x%x", reg_area.start_offset + 8);

    // // Initialization of Discrete Inputs register area
    // reg_area.type = MB_PARAM_DISCRETE;
    // reg_area.start_offset = MB_REG_DISCRETE_INPUT_START;
    // reg_area.address = (void*)&discrete_reg_params;
    // reg_area.size = sizeof(discrete_reg_params);
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));

    setup_reg_data(); // Set values into known state

    // Starts of modbus controller and stack
    ESP_ERROR_CHECK(mbc_slave_start());

    ESP_LOGI(SLAVE_TAG, "Modbus slave stack initialized.");
    ESP_LOGI(SLAVE_TAG, "Start modbus slave...");
        // Check for read/write events of Modbus master for certain events
    while(1){
        mb_event_group_t event = mbc_slave_check_event(MB_READ_WRITE_MASK);
        const char* rw_str = (event & MB_READ_MASK) ? "READ" : "WRITE";
        // Filter events and process them accordingly
        /*if(event & (MB_EVENT_HOLDING_REG_WR | MB_EVENT_HOLDING_REG_RD)) {
            // Get parameter information from parameter queue
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "HOLDING %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                    rw_str,
                    (uint32_t)reg_info.time_stamp,
                    (uint32_t)reg_info.mb_offset,
                    (uint32_t)reg_info.type,
                    (uint32_t)reg_info.address,
                    (uint32_t)reg_info.size);
            // if (reg_info.address == (uint8_t*)&holding_reg_params.holding_data0)
            // {
            //     portENTER_CRITICAL(&param_lock);
            //     holding_reg_params.holding_data0 += MB_CHAN_DATA_OFFSET;
            //     if (holding_reg_params.holding_data0 >= (MB_CHAN_DATA_MAX_VAL - MB_CHAN_DATA_OFFSET)) {
            //         coil_reg_params.coils_port1 = 0xFF;
            //     }
            //     portEXIT_CRITICAL(&param_lock);
            // }
        } 
        else if (event & MB_EVENT_INPUT_REG_RD) {
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "INPUT READ (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                    (uint32_t)reg_info.time_stamp,
                    (uint32_t)reg_info.mb_offset,
                    (uint32_t)reg_info.type,
                    (uint32_t)reg_info.address,
                    (uint32_t)reg_info.size);
        } else if (event & MB_EVENT_DISCRETE_RD) {
            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "DISCRETE READ (%u us): ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                                (uint32_t)reg_info.time_stamp,
                                (uint32_t)reg_info.mb_offset,
                                (uint32_t)reg_info.type,
                                (uint32_t)reg_info.address,
                                (uint32_t)reg_info.size);
        } else */
        
        if (event & (MB_EVENT_COILS_RD | MB_EVENT_COILS_WR)) {

            if(event & MB_EVENT_COILS_WR) set_DO();//set output status on Coils Write

            ESP_ERROR_CHECK(mbc_slave_get_param_info(&reg_info, MB_PAR_INFO_GET_TOUT));
            ESP_LOGI(SLAVE_TAG, "COILS %s (%u us), ADDR:%u, TYPE:%u, INST_ADDR:0x%.4x, SIZE:%u",
                                rw_str,
                                (uint32_t)reg_info.time_stamp,
                                (uint32_t)reg_info.mb_offset,
                                (uint32_t)reg_info.type,
                                (uint32_t)reg_info.address,
                                (uint32_t)reg_info.size);
            //if (coil_reg_params.coils_port1 == 0xFF) break;
        }
    }
    // Destroy of Modbus controller on alarm
    ESP_LOGI(SLAVE_TAG,"Modbus controller destroyed.");
    vTaskDelay(100);
    ESP_ERROR_CHECK(mbc_slave_destroy());
#if CONFIG_MB_MDNS_IP_RESOLVER
    mdns_free();
#endif
}
