# SucroClock firmware

Tested build tool: **ESPHome 2026.9.1**, with Python 3.12 and `cryptography`.
The target is the existing ESP32/Arduino HU-058D wiring. Keep the hardware current
limit and pin mapping appropriate for your board; see [wiring](../../docs/wiring.md).

## First build

From this directory:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-build.txt
cp secrets.yaml.example secrets.yaml
```

Fill in Wi-Fi, a 32-byte base64 API encryption key, unique web/setup/OTA/fallback
passwords, and your timezone. Provider accounts can be entered later on the HTTPS
setup page. Generate the API key locally:

```sh
python -c "import base64,secrets;print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

Create a unique certificate for this device. Include its reserved IP if you want
HTTPS access by IP too; otherwise the hostname alone is sufficient:

```sh
python tools/create_setup_certificate.py --hostname sucroclock.local
# Or: python tools/create_setup_certificate.py --hostname sucroclock.local --ip DEVICE_IP
esphome config clock.yaml
esphome compile clock.yaml
```

For a new board, upload using its serial connection with `esphome upload clock.yaml`.
For an already-running compatible ESPHome board, use
`esphome upload clock.yaml --device DEVICE_IP --ota-platform esphome`.
Existing Clock-O-Rama users must follow the [migration guide](docs/MIGRATION.md).
Keep the existing partition layout; an application OTA image is not a factory image.

## Open the clock

The fallback AP is **SucroClock Setup**, with your `ap_password`. Normal operation
uses your Wi-Fi. The local HTTPS interface is **https://sucroclock.local:8443/**;
username **admin**, password `setup_password`. Trust and verify the device's
unique [certificate](docs/HTTPS-SETUP.md) on your own browser/device.

Choose **Provider setup**, save/test a provider (or pair SugarSense), then select
that source and **Display**. Preview tests readings while leaving the clock display
unchanged. HA is optional; SNTP supplies time without an HA connection.

The ESPHome controls/browser OTA page is **http://sucroclock.local/** with
`admin` / `web_password`. Use the HTTPS page for provider credentials. Native
ESPHome OTA remains available with `ota_password` if browser OTA is unavailable.

## Develop and validate

```sh
python tools/embed_brand_icon.py --check
python tests/run_tests.py --arduinojson PATH_TO_ARDUINOJSON_7_SRC
esphome config clock.yaml
esphome compile clock.yaml
```

The ArduinoJson source is fetched by the first ESPHome build. Pass its actual
`src` directory from the ESPHome build tree. Host tests require `clang++` with
AddressSanitizer/UndefinedBehaviorSanitizer. They use fixtures and do not contact
provider accounts.

- [Migration](docs/MIGRATION.md) · [Controls](docs/CONTROLS.md) · [Providers](docs/PROVIDERS.md)
- [Colors](docs/GLUCOSE-COLORS.md) · [SugarSense](docs/SUGARSENSE.md)
- [Branding](docs/BRANDING.md) · [Releasing](docs/RELEASING.md)

Keep `secrets.yaml`, `.clock-private/`, local builds and environments private.
A compiled device image embeds credentials and its HTTPS key: **do not publish it**.
The public distribution is source plus example settings, with per-device builds.

### Display fixes in 0.1.1-dev

Direct-provider Display mode shows a single white `-` while a reading is missing
or stale, and automatically restores glucose when a fresh reading arrives.
Provider-owned readings no longer expire on a short display timer during busy
updates. Preview, Home Assistant, and temporary button displays keep their
existing behavior. See the root changelog for validation and remaining limits.
