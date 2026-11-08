# Dexcom Share connector

This adds a direct Dexcom Share provider alongside Home Assistant. It does not use Sugar Sense. LibreLinkUp and Nightscout are available as separate providers; see [docs/PROVIDERS.md](docs/PROVIDERS.md). Device name, existing HA API/actions/entities, display wiring/current, glyph shapes, effects, web UI, fallback provisioning and both OTA paths remain in place.

## Browser credential setup

The preferred path is now the [HTTPS setup page](docs/HTTPS-SETUP.md). It saves credentials on the clock without a rebuild. The compiled-secret instructions below remain an optional fallback; saved credentials (including an explicit Forget record) take precedence. Save & Test is a one-shot login check and does not change source/display selection.

## First setup using compiled secrets

1. Enter the **publishing account's** Dexcom username/password in your private `secrets.yaml` under `dexcom_username` and `dexcom_password`. These are the credentials for the account uploading readings, not a follower's login. Empty values disable requests. Credentials are compiled into the firmware; they are not exposed as HA text entities, printed in connector logs or included in source bundles. Changing them currently requires rebuilding and OTA uploading. This is private local storage, not encrypted storage against physical extraction. Keep the firmware image private too.
2. Use the existing ESPHome 2026.9.1 build environment (`requirements-build.txt`). Validate and compile `clock.yaml`, then upload its OTA application image using the existing native OTA path or browser uploader. Preserve `secrets.yaml` and use the unchanged device IP. No serial access or erase is required.
3. Keep `Glucose Source` at **Home Assistant** initially. The default `Dexcom Display Mode` is **Preview**, and region defaults to **US**. All three selectors persist across restart and appear in HA and the clock web UI.
4. To test, select **US**, **Preview**, then set `Glucose Source` to **Dexcom Share**. This enables fetching and the Dexcom diagnostic entities, while existing HA automations still control the display. Check glucose, trend, measurement time, age and `Dexcom Reading Fresh` against the phone/HA for at least two measurement cycles.
5. Only after comparison, change `Dexcom Display Mode` to **Display** to let fresh direct readings drive the digits. HA temporary number/seconds/lamp actions still take priority until their existing duration expires. Frequent HA glucose actions can therefore keep taking the display; pause that particular automation if you want Dexcom to own it continuously. Color/brightness/effect controls remain HA/web controllable. This first provider build leaves existing colors/effects unchanged; it does not recreate unknown HA threshold/color automations inside the clock.
6. Switch `Glucose Source` back to **Home Assistant** at any time to stop new Dexcom polls and release provider-owned display content. An already-running request may finish, but its result is discarded. Existing HA temporary displays are not cleared by that switch.

## Regions

The verified Share reference supports **US**, **Outside US**, and **Japan**, with distinct service endpoints (and a different application ID for Japan). Changing region invalidates cached credentials/session results and requires a new login. China was mentioned in the planning chat in error; no unverified China endpoint is offered. Only US is relevant to this clock's first live test; other regions remain fixture-tested, not account-tested.

## Data behavior and priority

A provider-independent `Reading` holds mg/dL, normalized trend and the actual UTC measurement timestamp. Fetching the same sample again never changes its timestamp or resets its monotonic freshness timer. Readings older than 15 minutes, invalid values/timestamps, or readings implausibly in the future are not presented as fresh. Invalid/stale data produces unavailable glucose and a false freshness entity; direct display releases to time. A recent cached reading may remain available through a temporary network error, with the connection problem shown in status.

The connector polls at most once a minute, reuses its in-memory session and performs HTTPS in a separate task. Certificates/hostnames are verified against the ESPHome CA bundle; redirects are disabled for credential-bearing requests. It waits for clock sync before TLS. Response bodies are capped at 8 KiB. Errors retry at 1, 2, 4, 8 and then 16 minutes; HTTP 429 waits 15 minutes. A rejected login stops attempts until **Dexcom Retry Connection**, a source/region change, or restart. Session expiry clears the session and retries a login on the next scheduled attempt. A region/source generation counter prevents old requests from publishing into a new selection.

Display glyphs use the existing shapes: `>` rapid rise, `^` rise/slight rise, `-` flat, `_` fall/slight fall, `<` rapid fall. Unknown/noncomputable trend has no glyph, rather than a false flat indication. Live display is mg/dL only for this US-first milestone. No sensor alarms or new threshold behaviors are added.

## Verification before switching the display

- Defaults: HA source, Preview mode and US region; existing clock behavior and HA buttons/actions still work.
- Compare measurement timestamps as well as the numerical values. A successfully completed request is not evidence of a new measurement.
- Check wrong credentials once: status should report rejection, then attempts must stop. Correct locally/rebuild, then retry. Do not deliberately trigger account lockout.
- Disconnect the clock's network temporarily: the UI may become unreachable, but the current reading must age out; recovery must preserve settings. Repeat the clock's normal OTA and fallback-AP checks.
- In Display mode, invoke show_number/show_seconds/lamp_test from HA. Each must finish normally before the provider resumes. Test HA source switch during a temporary action, and Preview switch while live digits are showing.
- Confirm stable behavior during cloud delay/failure, no reboots, usable web UI/HA and acceptable display flicker/heap. Host tests cannot establish these hardware/runtime properties.

## Host tests

After source generation/compilation has installed ArduinoJson 7, run:

```sh
python3 tests/run_tests.py --arduinojson /absolute/path/to/managed_components/bblanchon__arduinojson/src
```

Uses `clang++` (or `CXX`) with address/undefined-behavior sanitizers. The actual protocol code is tested using a fake transport. The actual coordinator uses small platform/sensor/queue stubs; its worker is not started on the host. Tests cover wire parsing, string escaping, session reuse/expiry, regional application IDs, missing/malformed readings, future/stale timestamps, preview isolation, invalidated in-flight results, repeated-sample aging, login rejection, retries and source release. Physical display ownership, real TLS and live authentication need device testing.

## Protocol references

This is an independent ESPHome implementation. The service interface was checked against the primary [pydexcom implementation](https://github.com/gagebenne/pydexcom/tree/main/pydexcom), especially `const.py`, `dexcom.py` and `glucose_reading.py`. Dexcom Share is an unofficial interface and may change. No Sugar Sense service/client code is included.
