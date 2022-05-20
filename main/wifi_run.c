#include "include/wifi_run.h"

static esp_err_t set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static void set_static_ip(esp_netif_t *netif)
{
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(WIFITAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(WIFITAG, "Failed to set ip info");
        return;
    }
    ESP_LOGD(WIFITAG, "Success to set static ip: %s, netmask: %s, gw: %s", STATIC_IP_ADDR, STATIC_NETMASK_ADDR, STATIC_GW_ADDR);
    //ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
    //ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        xTaskCreate(&wifi_scan, "wifi_scan", 2048, NULL, 10, NULL);//Create wifi scan task when STA started

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        set_static_ip(arg);

        ad_da_init();
        esp_log_level_set("I2S", ESP_LOG_INFO);
        xTaskCreate(i2s_adc_dac_fn, "i2s_adc_dac", 1024 * 2, NULL, 5, NULL);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFITAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        esp_restart();
        ESP_LOGI(WIFITAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFITAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFITAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFITAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFITAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else {
        ESP_LOGE(WIFITAG, "UNEXPECTED EVENT");
    }
    /* The event will not be processed after unregister */
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void *arg)
{
     ESP_LOGI(WIFITAG, "WIFI Scan task is running!");
    for(;;) {
        uint16_t number = DEFAULT_SCAN_LIST_SIZE;
        uint16_t ap_count = 0;
        memset(ap_info, 0, sizeof(ap_info));
        esp_wifi_scan_start(NULL, true);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGI(WIFITAG, "Total APs scanned = %u", ap_count);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
            ESP_LOGI(WIFITAG, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGI(WIFITAG, "RSSI \t\t%d", ap_info[i].rssi);
        }
        vTaskDelay(scan_delay / portTICK_PERIOD_MS);
    }
}