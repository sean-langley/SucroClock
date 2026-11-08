# SucroClock

**Your glucose, at a glance.**

SucroClock is the project and visible product name. The HTTPS dashboard, provider
setup page, browser title/favicon, ESPHome friendly name, fallback setup AP,
SugarSense client model/User-Agent and project README use this name.

The public device identity is `sucroclock`, with hostname `sucroclock.local` and
HTTPS dashboard `https://sucroclock.local:8443/`. The fallback AP is SucroClock
Setup. The native API actions use the `esphome.sucroclock_` prefix in HA.
Each device generates and retains its own TLS certificate/key. See
[MIGRATION.md](MIGRATION.md) for the intentional change from Clock-O-Rama.
Provider storage namespaces remain stable so a hostname change does not erase
accounts, SugarSense pairing or custom ranges. Historical milestone notes may
still use the old name. Adrian Black's attribution remains intact.

## Icon assets

The original icon was generated with the built-in image_gen tool. The prompt is
in `assets/generation-prompt.txt`. The public `assets/` folder supplies 32, 64, 192 and 512px PNG versions,
with 512px as the public master. The full generated original is retained locally
and excluded from the public package. Derivatives only resize the artwork.
The clock embeds the 192px PNG (about 36 KiB) and serves it locally at `/icon.png`;
no external asset service, tracking request or additional library is required.
The image is a public brand asset; all account/control endpoints retain their
existing HTTPS authentication and CSRF protection. The CSP permits images from
the clock's own origin only.

Run `python tools/embed_brand_icon.py` after changing the 192px PNG, then compile
normally. `--check` verifies the committed firmware header matches the PNG. The
192px icon appears in the main dashboard and provider setup page and supplies
the browser favicon and saved-home-screen icon.

Use `sucroclock.ota.bin` for application OTA with the existing partitions. Keep
the previous known-good image for rollback. No flash erase or certificate change
is part of the rename. Current provider credentials, source/mode and custom
color ranges must remain intact after updating.
