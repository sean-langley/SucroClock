# SugarSense source

SugarSense is an optional source alongside direct Dexcom Share, LibreLinkUp,
Nightscout and Home Assistant. It requires a SugarSense account with its own
glucose source connected. Existing direct sources do not depend on SugarSense.

## Pair when ready

1. Open the clock's authenticated HTTPS dashboard, then **Provider setup**.
2. Choose **SugarSense** and press **Start pairing**.
3. Follow **Open SugarSense pairing** and sign in to SugarSense yourself. Approve
   the displayed six-character code for this clock.
4. Wait for the clock to report **Paired**. Then, on the main dashboard, choose
   **SugarSense** as the glucose source and **Display** as the mode.

Pairing and its one-shot connection test do not change the selected source or
mode. Preview checks data without putting it on the clock. A code expires after
at most ten minutes; start again if it expires. No Dexcom/Libre passwords are
entered into the clock's SugarSense form. SugarSense issues a read-only display
session after approval. The app controls which sensor/account supplies data.

Only the paired account's own latest reading is supported in this version;
Care Circle person selection and multiple readings are not included. The value
travels in mg/dL, even if the account prefers mmol/L. Five SugarSense arrow
positions map to the existing clock trend glyphs; rapid movement is not inferred
from a plain up/down arrow. The clock's existing configurable ranges, colors and
breathing rules apply. Account thresholds, timezone offset, units, graph and
remote SugarSense firmware suggestions do not overwrite the clock settings.

## Connection and disconnect

The clock calls SugarSense directly over certificate-verified HTTPS at
`https://app.sugarsense.io/api/v1`. It ordinarily polls once a minute and respects
the server's snapshot pull interval, bounded to 30–900 seconds. Readings retain
their measurement time. The existing 15-minute freshness rule, monotonic age cap,
no older-sample replacement, retry/backoff and rate-limit handling apply. Invalid
or revoked sessions stop automatic requests until you disconnect/re-pair or Retry.
There is no automatic fallback to another provider or to Home Assistant.

**Disconnect / cancel pairing** removes the local session and requests server
revocation. If offline or revocation fails, remove that display yourself at
[Your SugarSense displays](https://app.sugarsense.io/display). A local storage
failure keeps the previous session and reports the failure. Cancellation also
invalidates in-flight pairing results; a late token is not adopted and is queued
for best-effort revocation. Pairing codes and pending poll secrets do not survive
restarts; a saved approved session does survive ordinary OTA updates.

Session storage is versioned in NVS namespace `clock_sugar`, key `session`,
independent of all other accounts and the color settings. The token is not
returned by the local status API or rendered in the UI. The local HTTPS login and
CSRF protection guard pairing commands. The device ID uses `disp-` plus the
board's MAC, matching the public display API's identity format; it identifies its
model as SucroClock HU-058D. Outbound provider requests share the existing TLS
worker lock and additional-provider task, rather than adding another worker.

## Protocol reference and validation

Implemented against SugarSense's public display client:
[net.h](https://github.com/sugarsense/sugarsense-display/blob/main/firmware/sugarsense_display/net.h),
[display rules](https://github.com/sugarsense/sugarsense-display/blob/main/firmware/sugarsense_display/ss_logic.h).
The four endpoints are displayPairStart, displayPairPoll, getDisplaySnapshot and
displayUnpair. No server endpoint is configurable and redirects are not followed.

Host tests cover the protocol, bounded/malformed responses, authentication,
timestamps, units, all arrow values, code expiry, cancellation and late results,
failed local saves, revocation, active-source isolation and stale data. At the
user's request SugarSense is installed unpaired: no live account approval or live
SugarSense reading has been tested yet. Dexcom and current custom settings remain
selected and preserved. The setup-page preview uses a sample pairing code only.

Use the included application OTA image with the existing partitions. Keep the
previous known-good image for rollback. Do not erase flash or upload a factory
image through OTA.
