# Changelog

## 0.1.1-dev — display fixes (2026-11-09)

- Show a single dash when a direct provider has no usable reading, including
  startup, stale data, and unconfigured sources, instead of falling back to time.

- Keep direct-provider glucose visible across delayed updates, avoiding a brief
  return to the time. Freshness/source checks still clear the provider display;
  temporary button displays retain their normal timeout.
- Avoid redrawing unchanged provider values and trend glyphs every second.

Validation: firmware compiled and all eight host suites passed. Both fixes were
installed over Wi-Fi. The owner confirmed the white dash for unavailable Dexcom
data; the brief-time-flash fix still awaits explicit visual confirmation across
a reading update. LibreLinkUp, Nightscout and SugarSense live tests remain pending.

## 0.1.0-dev — initial development preview

SucroClock turns the HU-058D RGB clock into a local, provider-independent glucose
display built with ESPHome on an ESP32.

### Included

- Adrian Black's AiP33628 display driver/hardware foundation and custom trend glyphs.
- Direct Dexcom Share, LibreLinkUp, Nightscout and SugarSense sources, plus optional HA control.
- Local HTTPS dashboard with separate Clock and Glucose controls and provider setup.
- SugarSense code-based pairing; separate local credential storage for each provider.
- Configurable glucose ranges/colors, smooth blending and one-shot breathing alerts.
- Freshness checks, explicit source selection and Preview/Display modes.
- Native ESPHome OTA, browser OTA and fallback Wi-Fi provisioning.
- SucroClock name/icon, per-device certificate generation and source-only release tooling.

### Validation and limits

Dexcom and Home Assistant display automation have been exercised on the physical
clock. Display/buttons were confirmed by its owner. Eight host test suites cover
protocols, source/display isolation, freshness, credential/settings validation,
color rules and pairing failure paths. The ESPHome build and extracted source
configuration were validated with the pinned tools.

Live LibreLinkUp, Nightscout and SugarSense accounts remain unverified. SugarSense
supports the paired account's own reading only; Care Circle selection is not
included. The clock displays integer mg/dL. Provider APIs may change. This is a
secondary display and does not replace provider apps, alarms or treatment tools.

### Migration

Older Clock-O-Rama users need the new `sucroclock.local` hostname/certificate and
`esphome.sucroclock_…` action prefix. Reload the clock's ESPHome integration in HA
after the rename; renaming entities alone does not register the new actions.
Shared Libre/Nightscout diagnostic entities are now named Direct Provider.
See the migration guide for entity checks and rollback.

### Distribution

This release is source-only. Each device generates its own settings/certificate
and builds locally. A device-specific compiled image embeds credentials and a
private key and must not be uploaded as a public release asset.
