#ifndef WIFI_RUN_H
#define WIFI_RUN_H

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

#include "ad_da.h"
#include "rtp_rx.h"
#include "utils.h"

#define WIFI_SSID "QH"
#define WIFI_PASSWORD "QH042022"

#define DEFAULT_SCAN_LIST_SIZE 5 //Max 5 SSID
#define EXAMPLE_MAXIMUM_RETRY 5
static int s_retry_num = 0;

static int16_t scan_delay = 2000; //2s
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE]; // WIFI scan array

static const char *WIFITAG = "WIFI";

#define STATIC_IP_ADDR "176.176.176.201"
#define STATIC_NETMASK_ADDR "255.255.255.0"
#define STATIC_GW_ADDR "176.176.176.1"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

void wifi_init_sta(void);
static void wifi_scan(void *arg);

#endif //IFI_RUN_H