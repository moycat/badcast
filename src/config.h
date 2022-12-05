#ifndef BADCAST_HEADER_CONFIG
#define BADCAST_HEADER_CONFIG

/* AP */
#define AP_RANDOMIZE_MAC 1
#define AP_CHANNEL 6
#define AP_MAX_STATION 10
#define AP_SSIDS "CMCC", "KFC FREE WIFI", "Starbucks", "UESTC-WiFi"

/* Captive Portal */
#define CAPTIVE_PORTAL_DOMAIN "bad.cast"

/* Internal */
#define TASK_STACK_SIZE 4096
#define DUMB_SSID_LEN 10

/* Deauther */
#define DEAUTHER_ENABLED 1

#endif