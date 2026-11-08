# HTTPS setup and controls

The primary interface is **https://sucroclock.local:8443/**; provider setup is
`/setup`. Each device needs its own certificate/key, generated locally with
`tools/create_setup_certificate.py`. The default certificate covers
`sucroclock.local`; pass `--ip DEVICE_IP` to include a reserved IP as well.
There is no project-wide private key and no personal IP baked into the helper.

The helper refuses to overwrite an existing key/certificate. Keep `.clock-private/`
across ordinary updates. For an intentional hostname/key rotation, generate into a
separate `--output-dir`, keep a backup, and follow [migration](MIGRATION.md).

This is a self-signed certificate. Compare its SHA-256 fingerprint with the one
reported through the encrypted native ESPHome API before trusting it in your
browser/computer. The public file `https-cert.pem` can be imported as a trusted
certificate on your own devices; never share `https-key.pem`. Browser/security
warnings require your own trust decision. Use the certificate's hostname or an
IP explicitly included in its SAN. DHCP changes do not alter the hostname SAN.

HTTPS login: **admin** and `setup_password`. The HTTP ESPHome controls/browser OTA
page at **http://sucroclock.local/** uses **admin** and `web_password`. Use the
HTTPS interface for account credentials. Both passwords should be unique.
Authentication and CSRF protection guard setup/control changes. Secrets and
SugarSense session/poll tokens are not returned by the status API.

Provider accounts are stored independently in local NVS. Empty password fields
retain a saved secret; Forget removes the selected account only. Firmware embeds
its own Wi-Fi/API/OTA/web secrets and TLS key: keep per-device binaries private.
The public release contains source and placeholder settings only.
