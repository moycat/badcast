# badcast

POC of spamming people using ESP32 with a captive portal.

Demo video: https://t.me/moycat_official/1192

## How it works

Modern devices scan and connect to known wireless networks automatically by default. And a [captive portal](https://en.wikipedia.org/wiki/Captive_portal) pops up if the device finds that the network requires authentication.

With this project, you can exploit these features to spam others with ESP32, an MCU with Wi-Fi capability.

You can broadcast multiple widely-used SSIDs (like `Starbucks` and `KFC FREE WIFI`) without passwords. Nearby devices that have connected to the networks of the same name before will connect to you automatically. Then the portal page shows up. No user operation is required.

If deauther is enabled, it also performs [deauth attacks](https://en.wikipedia.org/wiki/Wi-Fi_deauthentication_attack) against nearby APs to force the devices to connect to you.

## How to use it

This project is developed with [PlatformIO](https://platformio.org/) and tested on an ESP32-WROOM-32E development board.

Specify options like SSIDs in `src/config.h`.

Customize the portal page by editing `src/web.html`.

## Limitations & potential improvements

1. Deauth attacks are against 2.4 GHz APs only, and the clients disconnect during attacks.
2. ESP32 limits the number of clients and TCP connections to around 10. We may be able to modify the firmware to raise the limitations.
3. Devices with proxy or VPN on may ignore captive portals.

## Credits

- Broadcast multiple SSIDs with ESP32: https://github.com/spacehuhn/esp8266_beaconSpam
- Captive portal & DNS server: https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/captive_portal
- Deauth with ESP32: https://github.com/GANESH-ICMC/esp32-deauther
- Penetration test with ESP32: https://github.com/risinek/esp32-wifi-penetration-tool
