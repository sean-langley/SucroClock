# LibreLinkUp and Nightscout

The clock now has four choices under **Glucose Source**: Home Assistant, Dexcom Share, LibreLinkUp and Nightscout. Selection is explicit: there is no automatic fallback between providers or people. Existing Dexcom credentials, selected source, Dexcom Display Mode, certificate/login, Home Assistant entities/actions, display glyphs/effects/current settings and OTA paths are retained.

The secure provider setup page at **https://sucroclock.local:8443/setup** now has a **Configure provider** selector. Use this device’s configured HTTPS login and certificate. Credentials are stored separately per provider. Save & Test and Forget affect the selected configuration only. A test never selects that provider for display, and a test for another provider cannot replace a fresh active reading. Passwords/tokens are never returned by the page; blank retains the saved secret. Nightscout Public mode is allowed without a secret. Its saved secret, if any, is not transmitted in Public mode.

## LibreLinkUp

Use the **LibreLinkUp follower account** that receives shared readings, not the LibreView publishing-account login. Ensure sharing and account setup/consents are completed in the official app.

1. Choose LibreLinkUp in the secure setup page, enter follower email/password, and leave Region at Auto unless you need an explicit supported region.
2. Save & Test. Login follows only JSON region redirects to the fixed verified Abbott host list; arbitrary HTTP redirects are not followed. Tokens are reused in RAM and refreshed after session expiry. The account-id header is the SHA-256 of the returned user ID.
3. With one shared person, Automatic chooses that person. With multiple shared people, the test returns a linked-person list and requires a selection. Choose the person and Save & Test again. If that person stops sharing, selection must be resolved again; another person is not silently substituted.
4. The client-version field defaults to 4.16.0, matching the reference client checked for this implementation. Abbott may require a later client version; update this field to the appropriate LibreLinkUp version if needed. Account consent/verification requirements must be completed in the official app; the clock does not accept them for you.

Measurements use `ValueInMgPerDl`, regardless of the app's local unit, and `FactoryTimestamp` as UTC. The local wall-clock `Timestamp` is not used as a UTC fallback. Missing/invalid timestamps or mg/dL fields do not become fresh data. Five Libre trend arrows map into the existing display trend vocabulary.

Supported regional hosts are Auto/global, US, EU, EU2, AE, AP, AU, CA, DE, FR, JP, LA and RU. They are protocol/fixture tested, not account tested on every region.

## Nightscout

Enter a **HTTPS base URL**, optionally with a path prefix, such as `https://your-nightscout.example` or `https://host.example/nightscout`. Do not append an API path, query token or user/password. DNS names/IPv4 and optional ports are accepted; IPv6 URL literals are not currently supported. TLS certificates must validate using the ESPHome CA bundle; an untrusted private/self-signed Nightscout certificate needs a future custom-CA configuration rather than disabled validation.

Choose one authentication mode:

- **Token**: enter a Nightscout access token, preferably with a read-only role. Sent in the `api-secret` header; it is not put in a URL.
- **API Secret**: enter the raw API_SECRET. The clock sends its SHA-1 digest in the `api-secret` header.
- **Public**: no authentication header. Only useful when that instance allows public reads.

The clock makes only a read request: `GET /api/v1/entries/sgv.json?count=1`. It does not upload, edit or delete data. `sgv` is treated as mg/dL. Measurement time comes from the numeric millisecond `date`, with explicit-time-zone ISO `dateString` supported when `date` is absent. Unknown trends remain unknown. Network/server/authentication failures are distinct from an empty readings array.

## Display and Home Assistant

After a successful test, select the desired provider under **Glucose Source** on the existing control page or in HA. Use the shared **Glucose Display Mode**: choose Preview while comparing value, trend and measurement time for at least two new readings, then choose Display to put it on the clock. Existing provider-specific mode entities remain synchronized compatibility aliases. New other-provider sensors are grouped under Libre/Nightscout; existing Dexcom sensors are unchanged.

Existing temporary HA number/seconds/lamp actions still take precedence for their configured duration. If your HA automation repeatedly writes glucose, pause that automation when using direct display. Existing color, brightness and effect controls remain available; no new threshold/color policy is imposed by these providers.

Fifteen-minute stale checks and implausible-future timestamp checks are shared with Dexcom. Re-fetching the same measurement never refreshes its measurement timestamp or monotonic freshness timer. An old or missing reading releases provider-owned display content. Source/config changes invalidate old in-flight results. A background test does not overwrite another provider's reading, retry state or display ownership.

Cloud requests run in worker tasks, with a shared lock across provider fetches to avoid simultaneous outbound TLS handshakes. HTTPS setup remains available separately. Requests verify certificates, refuse automatic HTTP redirects, bound response sizes, reuse Libre sessions, back off on failures and stop repeated rejected logins. A provider selection change is deliberate user action; data is not mixed between providers.

## Validation limits

Host tests exercise the actual parsers and coordinators with synthetic replies: region/session handling, patient selection, authentication headers, UTC/ISO timestamps, units/trends, malformed data, source/config races, retry limits, Preview and display/test isolation. Device tests preserve and re-check the working Dexcom path, HTTPS setup, provider-specific settings and memory. Actual LibreLinkUp and Nightscout accounts have not been supplied, so those live-service checks remain for setup by the user.

Protocol references (independent implementations; no third-party provider client source copied):

- https://github.com/robberwick/pylibrelinkup/tree/main/src/pylibrelinkup
- https://github.com/nightscout/nightscout-connect/blob/main/lib/sources/librelinkup.js
- https://github.com/nightscout/cgm-remote-monitor/blob/master/lib/authorization/index.js
- https://github.com/nightscout/cgm-remote-monitor/blob/master/lib/api/entries/index.js
