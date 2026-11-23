# Buying hardware for SucroClock

## Required parts

- A HU-058D RGB clock kit with enclosure and the AiP33628-based display/PCB.
- **A newly purchased, separate ESP32 development board. The included ESP8266 / ESP-01S cannot run SucroClock.** The tested configuration targets a classic ESP32-WROOM-32 (`esp32dev`), with GPIO18/19/21/22/32/33 available.
- Hookup wire, suitable 5 V power, and soldering tools; follow the [wiring guide](wiring.md).

The stock STC controller and ESP-01S are replaced/omitted during the conversion.
Do not flash SucroClock to the included ESP8266. ESP32-C3/S2/S3 require a separate
port/configuration and are not covered by the supplied wiring.

## Clock-kit purchase links

Checked 2026-11-22. These are direct retailer links without affiliate parameters;
they identify base clock kits, not prebuilt SucroClock devices.

- [Amazon US: Segrehy ESP8266 WiFi Clock Kit, white — B0FCCG11XB](https://www.amazon.com/dp/B0FCCG11XB). The live listing described a soldering/assembly kit with adjustable RGB display and Wi-Fi time sync, and advertised stock during the check. This seller's exact board revision has not been physically tested by this project.
- [AliExpress: original HU-058D / ESP8266 IoT Colorful WiFi Clock Kit listing](https://www.aliexpress.us/item/3256807371597845.html). This is the listing identified in [Adrian Black's upstream hardware table](https://github.com/misterblack1/HU-058_ESPHome#the-board-variants). The live page reported unavailable in the check location; it is retained as a product reference for other destinations, not as a claim of current stock.

The Amazon product was located through the links in [Adrian's build video](https://www.youtube.com/watch?v=vgfV_IGmhGA); its shortened link was resolved to the direct product URL above. The AliExpress alternate from that video did not provide a usable product page during this check.

Before ordering, compare the board photos with [hardware documentation](hardware.md)
and confirm the model with the seller. White/blue/purple describe the enclosure,
not proof of electronic compatibility. Stock, revisions, price and shipping vary.

## Similar board variants

Adrian's upstream project also documents HU-058 / HU-058SE kits with a DS1302 RTC.
They use a different driver-clock connection: GPIO22 goes to socket pin 16 rather
than HU-058D pin 14. Confirm your board and follow the variant-specific wiring;
do not assume a similar-looking RTC kit is wired exactly like HU-058D.
