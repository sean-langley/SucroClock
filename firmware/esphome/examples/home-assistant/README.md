# Example Home Assistant automation

`dexcom-display.yaml` is an optional HA-owned display automation. Select **Home
Assistant** as SucroClock's glucose source when using it for ongoing glucose.
Direct providers in Display mode retain priority over long HA number holds.

Adapt the entity IDs to your HA registry before enabling. The example assumes:

- `sensor.dexcom_g7_glucose_value` and `sensor.dexcom_g7_glucose_trend`.
- `binary_sensor.sucroclock_button_1`, `binary_sensor.sucroclock_button_2`, and
  `light.sucroclock_display`.
- `input_number.dexcom_previous_glucose` and
  `input_boolean.dexcom_display_alert_active`, created as HA helpers.
- Optional `sensor.nightscout_iob` for the both-buttons display; missing data
  uses the automation's unavailable indicator.

Use a previous-glucose number helper with a range covering your sensor values
and a step of one. The alert helper is an ordinary toggle. The example includes
its own color boundaries and day/night brightness schedule; edit those explicitly
if desired. It uses HA trigger/action syntax and belongs in the automation editor,
not in the ESPHome device YAML. Install one automation, not duplicate copies.

After a node rename, reload the ESPHome integration and check that the
`esphome.sucroclock_…` actions are registered. Check automation traces and physical
buttons. Running actions manually does not simulate every trigger branch.
