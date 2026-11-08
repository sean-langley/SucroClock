import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import aip33628, binary_sensor, esp32, sensor, socket, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "network", "aip33628"]
AUTO_LOAD = ["json", "sensor", "text_sensor", "binary_sensor"]
ns = cg.esphome_ns.namespace("clock_cgm")
ClockCGM = ns.class_("ClockCGM", cg.Component)
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(ClockCGM),
    cv.Required("panel_id"): cv.use_id(aip33628.Aip33628Panel),
    cv.Required("username"): cv.string_strict,
    cv.Required("password"): cv.sensitive(cv.string_strict),
    cv.Required("glucose"): sensor.sensor_schema(unit_of_measurement="mg/dL", accuracy_decimals=0),
    cv.Required("age"): sensor.sensor_schema(unit_of_measurement="min", accuracy_decimals=1),
    cv.Required("status"): text_sensor.text_sensor_schema(),
    cv.Required("trend"): text_sensor.text_sensor_schema(),
    cv.Required("measurement_time"): text_sensor.text_sensor_schema(),
    cv.Required("valid"): binary_sensor.binary_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA), socket.consume_sockets(1, "clock_cgm"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_panel(await cg.get_variable(config["panel_id"])))
    cg.add(var.set_credentials(config["username"], config["password"]))
    for name in ["glucose", "age"]:
        cg.add(getattr(var, f"set_{name}_sensor")(await sensor.new_sensor(config[name])))
    for name in ["status", "trend", "measurement_time"]:
        cg.add(getattr(var, f"set_{name}_sensor")(await text_sensor.new_text_sensor(config[name])))
    cg.add(var.set_valid_sensor(await binary_sensor.new_binary_sensor(config["valid"])))
    esp32.include_builtin_idf_component("esp_http_client")
    esp32.include_builtin_idf_component("esp-tls")
    esp32.require_certificate_bundle()
