#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "config.h"

static const char *TAG = "badcast_ap";

void ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void ap_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "ap init finished");
}

void ap_set_mac(void)
{
    uint8_t mac[6];
    esp_fill_random(mac, 6);
    mac[0] &= 254;
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, mac));
    ESP_LOGI(TAG, "mac set to "MACSTR, MAC2STR(mac));
}

void ap_start(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = DUMB_SSID_LEN,
            .channel = AP_CHANNEL,
            .max_connection = AP_MAX_STATION,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 1,
        },
    };
    esp_fill_random(wifi_config.ap.ssid, DUMB_SSID_LEN);
    for (int i = 0; i < DUMB_SSID_LEN; i++) {
        wifi_config.ap.ssid[i] = wifi_config.ap.ssid[i] % 10 + 0x30;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "ap started, dump ssid: %s, channel: %d", wifi_config.ap.ssid, AP_CHANNEL);
}

uint8_t beaconPacket1[] = {
  0x80, 0x00, 0x00, 0x00,             // Type/Subtype: managment beacon frame
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source MAC
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source MAC

  // Fixed parameters
  0x00, 0x00,                         // Fragment & sequence number (will be done by the SDK)
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  0x64, 0x00,                         // Interval: 0x64, 0x00 => every 100ms
  0x21, 0x04,                         // Capabilities

  // Tagged parameters
  0x00, 0x20,                         // Tag: SSID, length: 32 (will be changed)
};

const uint8_t beaconPacket2[] = {
  0x01, 0x08,                         // Tag: Supported Rates, length: 8
  0x82,                               // 1(B)
  0x84,                               // 2(B)
  0x8b,                               // 5.5(B)
  0x0c,                               // 6
  0x96,                               // 11(B)
  0x18,                               // 12
  0x30,                               // 24
  0x60,                               // 48

  0x03, 0x01,                         // Tag: DS Parameter, length: 1
  AP_CHANNEL,                         // Current channel
};

void ap_broadcast(void *arg)
{
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, mac));
    memcpy(beaconPacket1 + 10, mac, 6);
    memcpy(beaconPacket1 + 16, mac, 6);

    char *ssids[] = {AP_SSIDS};
    size_t n = sizeof(ssids)/sizeof(ssids[0]);
    char **beacons = (char **)malloc(sizeof(char *) * n);
    int *beaconLens = (int *)malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) {
        int ssidLen = strlen(ssids[i]);
        char *beacon = (char *)malloc(sizeof(beaconPacket1) + sizeof(beaconPacket2) + ssidLen);
        memcpy(beacon, beaconPacket1, sizeof(beaconPacket1));
        strcpy(beacon + sizeof(beaconPacket1), ssids[i]);
        memcpy(beacon + sizeof(beaconPacket1) + ssidLen, beaconPacket2, sizeof(beaconPacket2));
        beacon[sizeof(beaconPacket1) - 1] = ssidLen;
        beacons[i] = beacon;
        beaconLens[i] = sizeof(beaconPacket1) + ssidLen + sizeof(beaconPacket2);
    }
    ESP_LOGI(TAG, "constructed %d beacon(s)", n);

    while (1) {
        for (int i = 0; i < n; i++) {
            esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_AP, beacons[i], beaconLens[i], true);
            if (ret) {
                ESP_LOGW(TAG, "failed to broadcast ssid %d, err: %d", i, ret);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void start_ap_task(void)
{
    ap_init();
    if (AP_RANDOMIZE_MAC) {
        ap_set_mac();
    }
    ap_start();
    xTaskCreate(ap_broadcast, "ap_broadcast", TASK_STACK_SIZE, NULL, 10, NULL);
}