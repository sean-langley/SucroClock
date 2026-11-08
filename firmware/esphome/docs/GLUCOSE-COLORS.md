# Glucose ranges and colors

Open the clock's HTTPS dashboard and expand **Glucose → Ranges & colors**.
Changes take effect only after **Save ranges & colors** confirms success. Settings
are saved on the clock and survive restarts and ordinary OTA updates.

The supplied Home Assistant automation uses solid bands, rather than a spatial
gradient. The defaults reproduce its glucose colors (mg/dL):

| Reading | Color |
| --- | --- |
| Below 70 | Red, #ff0000 |
| 70 through 180 inclusive | Green, #00c000 |
| Above 180 through 250 inclusive | Amber, #ffc000 |
| Above 250 | Red, #ff0000 |

Below the low limit, above the very-high limit, or on a Double up/Double down
trend, an alert episode begins. Low/high alerts use their editable range colors;
a rapid-trend alert overrides the range with the rapid-trend color. On entry,
it breathes from zero to full intensity and back over a four-second cycle for
15 seconds, then remains steady. New samples during the same alert episode do
not restart the breathing. Returning to a non-alert condition rearms it. Stale
data, Preview mode, disabling glucose colors, or changing providers ends the
episode. A restart also starts a new episode if the condition is still present.

All three boundaries, four band colors, rapid-trend color, rapid-trend override,
breathing duration and cycle are editable. **Smooth blending** optionally blends
between band colors as the value changes; it does not spread a gradient across
the digits. Blend width is in mg/dL: low-to-low+width, target-high ± width/2, and
very-high-width to very-high. Enabled rapid-trend alerts still
use the rapid-trend color. Limits must be strictly increasing between 20 and 600;
blending width must fit the adjacent ranges. Restore defaults requires confirmation.

The overlay applies only to a fresh direct Dexcom/LibreLinkUp/Nightscout reading
in Display mode. Home Assistant is optional: SNTP supplies time and the API
does not reboot the clock for lack of an HA connection. Preview leaves the clock
display unchanged. It retains brightness and the hardware current limit. Clock
base, digit and position colors and normal effects remain stored underneath;
they return for clock/HA displays. Lamp test and short HA number/seconds displays keep priority. In direct
Display mode, number requests longer than five seconds are ignored so an old
HA glucose automation cannot indefinitely take over. Selecting a direct source
clears an existing number hold. Home Assistant source mode retains the original
number display durations. Turning off **Use glucose colors** restores the existing
clock color/effect behavior for direct readings too.

Home Assistant source mode continues to use the HA automation's own colors.
Existing HA entities, API actions, buttons, glyphs and provider credentials are
retained. This change does not move the automation's brightness schedule,
age/delta/IOB button actions, or 20-minute stale handling into the firmware.
Direct-provider freshness handling remains as before.

Implementation: versioned, validated settings in NVS namespace `clock_colors`,
key `settings`, independent of provider credentials. Commands use the existing
authenticated HTTPS endpoint and CSRF protection. Invalid or failed writes retain
the previous settings; an invalid stored record falls back to the defaults.

## Update / recovery

Use `sucroclock.ota.bin` for an ordinary application OTA update. Preserve
the existing partition layout and private secrets/certificate files. Native
ESPHome OTA on the current clock IP remains the recovery path alongside browser
OTA. Keep the preceding known-good OTA image available. Do not upload a factory
image through OTA or erase flash: erasing would remove saved settings/accounts.

After updating, check the HTTPS page, fresh provider reading and selected mode;
check the physical color and your HA displays/buttons. Save a harmless setting
and refresh the page to confirm it is retained. Synthetic boundary/alert values
are tested on the host rather than sent to the live glucose display.
