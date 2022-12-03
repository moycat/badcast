# badcast

POC of spamming people using ESP32 with a captive portal.

## How it works

Modern devices scan and connect to known wireless networks automatically by default. And a [captive portal](https://en.wikipedia.org/wiki/Captive_portal) pops up if the device finds that the network requires authentication.

With this project, you can exploit these features to spam others with ESP32, an MCU with Wi-Fi capability.

You can broadcast multiple widely-used SSIDs (like `Starbucks` and `KFC FREE WIFI`) without passwords. Nearby devices that have connected to the networks of the same name will connect to you automatically. Then the portal page shows up. No user operation is required.

## How to use it

This project is developed with [PlatformIO](https://platformio.org/) and tested on an ESP32-WROOM-32E development board.

Specify options like SSIDs in `src/config.h`.

Customize the portal page by editing `src/web.html`.

## Limitations & potential improvements

1. Only devices not connected to a wireless network will connect to ESP32. We can perform deauth attacks to force other devices to connect to us.
2. ESP32 limits the number of clients and TCP connections to around 10. We may be able to modify the firmware to raise the limitations.

## Credits

- Broadcast multiple SSIDs with ESP32: https://github.com/spacehuhn/esp8266_beaconSpam
- Captive portal & DNS server: https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/captive_portal
