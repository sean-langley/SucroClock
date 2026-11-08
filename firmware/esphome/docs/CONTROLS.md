# Clock and Glucose controls

The primary secure control page is **https://sucroclock.local:8443/** (or the same `.local` hostname). Use the existing certificate trust and HTTPS setup login. It now has two sections:

- **Clock**: display on/off, brightness/color, 12-hour time, colon blink and effect selection. Expand Effect settings for speed/spread/angle/hue/fade. Expand Clock tools for seconds, lamp test, color demos, test number/unit and physical button states.
- **Glucose**: one view of the selected source's value, trend, freshness, age and measurement time; one source selector, one shared Glucose Display Mode, and one retry button. Home Assistant mode is labeled as managed by HA because it does not publish a normalized reading back into the connector.

Provider account settings are on **/setup**, reached through Provider setup on the Glucose card. That page retains separate Dexcom, LibreLinkUp and Nightscout records and Save & Test behavior. Root bookmarks now open the controls; the account form moved to /setup. The certificate, login and credential endpoints are unchanged.

## Shared display mode and compatibility

**Glucose Display Mode** controls all direct providers. Preview leaves HA in control of the panel; Display lets the selected direct source show its readings. No provider is selected automatically.

The older **Dexcom Display Mode** and **Libre/Nightscout Display Mode** HA entities remain available as compatibility aliases. Changes to any of the three mode selectors synchronize the others and both connectors, guarded against recursion. On the first boot after this upgrade, the previous mode of the selected provider is retained and copied into the shared selector. Existing source selection and provider credentials are retained. The canonical **Glucose Retry Connection** and dashboard retry button target the selected provider. Older retry entities remain available for existing HA uses.

The older provider-specific sensor entities also remain for HA compatibility. The main page presents a single reading/status for the selected source. Libre and Nightscout still have independent protocols, credentials, settings and linked-person behavior; their shared backend is not a reason to show combined controls on the everyday UI.

The standard ESPHome HTTP page remains as a fallback with the existing HTTP login and browser firmware uploader. It is also grouped into Clock and Glucose. Compatibility controls are sorted at the bottom of its Glucose group. The clean primary view is the HTTPS page.

## Request handling

The dashboard is self-contained, authenticated HTTPS and uses the existing CSP/CSRF/no-store protections. Its /api/control endpoint accepts only a fixed allowlist of clock/glucose controls with typed, bounded values. Hardware changes are queued to the ESPHome main loop and use the existing native light, switch, select, number and button actions. It never executes arbitrary actions or exposes stored credentials. Inputs start disabled until status has loaded, and changes wait for an acknowledgement from the clock.

The HA API/actions, native and browser OTA, fallback AP, provider clients, display driver, glyphs and scan timing are unchanged. This task did not add alarms, new glucose thresholds or an automatic fallback policy.
