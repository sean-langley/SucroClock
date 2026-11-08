# Migrate Clock-O-Rama to SucroClock

This is an intentional device-identity change for the public SucroClock project.
The default node and mDNS names become `sucroclock` / `sucroclock.local`. The
friendly name is SucroClock, and the setup AP is SucroClock Setup. Your Wi-Fi,
provider credentials, source/mode, color settings and API/OTA passwords remain.
The former Secure Dexcom Setup URL diagnostic becomes Web Interface URL.
The shared Libre/Nightscout diagnostic entities and compatibility display mode
are now named Direct Provider, since SugarSense uses that same connector.
Update references to those entities in HA as well.

## Before updating

- Keep the previous known-good application OTA image and private files as a
  rollback checkpoint. Record the clock's current IP and your working HA setup.
- Back up `.clock-private/`. Generate a new certificate into a different directory:

```sh
python tools/create_setup_certificate.py --hostname sucroclock.local --ip DEVICE_IP --output-dir NEW_PRIVATE_DIRECTORY
```

- Replace the two files in `.clock-private/` only after keeping your backup.
  Do not reuse another person's key or ship your own key in a public release.
- Validate and compile, then upload over native ESPHome OTA to the **current IP**.
  The name change does not require opening the clock or erasing flash.

## After updating

1. Open `https://sucroclock.local:8443/` (or a certificate-covered reserved IP).
   Verify the new certificate fingerprint over the encrypted ESPHome API/logs,
   then trust that public certificate on your own device. The old certificate
   no longer identifies this firmware. Remove the old trust entry when no longer
   needed. Passwords stay the same; a browser may ask you to sign in again.
2. In Home Assistant, open **Settings → Devices & services → ESPHome**, open
   the clock integration entry's menu, and choose **Reload**. Renaming entities
   alone does not refresh the ESPHome action registrations after a node rename.
   Verify `esphome.sucroclock_set_effect` and the other actions appear under
   Developer Tools → Actions. An `Action ... not found` error is a reason to
   check/reload the integration, not to keep changing valid YAML action names.
   Home Assistant may discover the new name or ask for reconfiguration. Keep the
   API encryption key. Verify the new device/connection is available before
   removing any stale entry. Existing entity IDs may remain under their old names;
   inspect the entity registry rather than assuming HA renames them automatically.
3. Replace action names such as `esphome.clock_o_rama_show_number` with
   `esphome.sucroclock_show_number` in automations. The eight action suffixes and
   arguments remain: show_seconds, show_number, lamp_test, set_digit_color,
   clear_digit_colors, set_position_color, set_effect and set_gradient.
4. Update any old `clock_o_rama`/`wifi_clock` entity references to the IDs actually
   assigned by your HA registry. Check brightness/effects, buttons and temporary
   displays. Select a direct provider in Display mode to use its local colors.
5. Check source/mode, a fresh reading, saved color limits, HTTPS controls and both
   OTA paths. SugarSense pairing state remains unchanged.

Without Home Assistant, steps 2–4 do not apply. Direct providers and SNTP work
independently. The API has no HA-client reboot timeout.

## Rollback

Use the previous application OTA image through native OTA at the current IP.
That image restores its embedded old certificate and node name. Keep both public
certificates/backups until migration is verified. Do not erase NVS or change the
partition table as part of a normal rollback.
