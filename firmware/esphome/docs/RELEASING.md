# Preparing a public release

The repository currently identifies this as **0.1.0-dev**, a development preview.
Do not describe untested provider accounts as validated. The current README lists
live-test coverage. Before promoting a stable version, complete account-backed
Libre/Nightscout/SugarSense tests and the documented physical controls/provisioning
checks. Fixture tests do not prove compatibility with current live APIs.

## Source release

1. Confirm attribution/NOTICE/LICENSE and the quick start are present.
2. Run host tests, both JS syntax checks, icon embedding check, ESPHome validation
   and compilation against the pinned build requirements. Generate throwaway
   settings/certificates for clean-build validation; never put them in the archive.
3. Export source using the explicit allowlist:

```sh
python tools/package_public_source.py --output PATH_OUTSIDE_REPOSITORY/SucroClock-source.zip
```

The exporter excludes Git metadata/remotes, local environments, caches, private
settings, certificates/keys, compiled binaries, swap files and local milestone
notes. It writes a SHA-256 source manifest. Inspect the file list and scan for
personal settings before uploading. Do not upload a whole working-directory zip.
Device firmware contains the compiled Wi-Fi/API/OTA/web secrets and TLS key; it is
not a reusable public binary. Universal binary onboarding is a separate release
feature and is not implemented by this source package.

## Repository and version

Create a SucroClock repository under the intended publisher's account when ready.
The existing local `origin` is Adrian's upstream project: do not push SucroClock
changes there as if it were this project's publication destination. Keep the
upstream attribution and original license. Review the prepared source archive
and public README before creating a public repository, tag or release.

Advance the ESPHome project version deliberately after release validation. Keep
release notes with source/config changes, compatibility changes, tested providers,
known limitations and migration steps. The Clock-O-Rama→SucroClock hostname,
certificate and HA action changes are documented in MIGRATION.md.
