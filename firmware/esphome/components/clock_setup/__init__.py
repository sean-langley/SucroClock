from pathlib import Path
import hashlib
import ssl
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import clock_controls, additional_cgm, clock_cgm, esp32, select, socket, text_sensor
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["esp32", "clock_cgm", "additional_cgm", "clock_controls", "network"]
AUTO_LOAD = ["json", "text_sensor", "select"]
ns = cg.esphome_ns.namespace("clock_setup")
ClockSetup = ns.class_("ClockSetup", cg.Component)
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(ClockSetup),
    cv.Required("controls_id"): cv.use_id(clock_controls.ClockControls),
    cv.Required("additional_id"): cv.use_id(additional_cgm.AdditionalCGM),
    cv.Required("source_id"): cv.use_id(select.Select),
    cv.Required("cgm_id"): cv.use_id(clock_cgm.ClockCGM),
    cv.Required("region_id"): cv.use_id(select.Select),
    cv.Required("certificate"): cv.file_,
    cv.Required("private_key"): cv.file_,
    cv.Required("auth_password"): cv.sensitive(cv.string_strict),
    cv.Required("fingerprint"): text_sensor.text_sensor_schema(),
    cv.Required("setup_url"): text_sensor.text_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA),
    socket.consume_sockets(2, "clock_setup"),
    socket.consume_sockets(1, "clock_setup", socket.SocketType.TCP_LISTEN),
    socket.consume_sockets(1, "clock_setup", socket.SocketType.UDP))

async def to_code(config):
    cert = Path(CORE.relative_config_path(config["certificate"])).read_text()
    key = Path(CORE.relative_config_path(config["private_key"])).read_text()
    # Check that the public certificate and private key match before compiling.
    from cryptography import x509
    from cryptography.hazmat.primitives import serialization
    certificate = x509.load_pem_x509_certificate(cert.encode())
    private = serialization.load_pem_private_key(key.encode(), password=None)
    encoding = serialization.Encoding.DER
    form = serialization.PublicFormat.SubjectPublicKeyInfo
    if certificate.public_key().public_bytes(encoding, form) != private.public_key().public_bytes(encoding, form):
        raise cv.Invalid("HTTPS certificate does not match its private key")
    fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert)).hexdigest().upper()
    fingerprint = ":".join(fingerprint[i:i+2] for i in range(0, len(fingerprint), 2))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_controls(await cg.get_variable(config["controls_id"])))
    cg.add(var.set_additional(await cg.get_variable(config["additional_id"])))
    cg.add(var.set_source_select(await cg.get_variable(config["source_id"])))
    cg.add(var.set_connector(await cg.get_variable(config["cgm_id"])))
    cg.add(var.set_region_select(await cg.get_variable(config["region_id"])))
    cg.add(var.set_tls(cert, key, fingerprint))
    cg.add(var.set_auth_password(config["auth_password"]))
    cg.add(var.set_fingerprint_sensor(await text_sensor.new_text_sensor(config["fingerprint"])))
    cg.add(var.set_url_sensor(await text_sensor.new_text_sensor(config["setup_url"])))
    esp32.include_builtin_idf_component("esp_https_server")
    esp32.include_builtin_idf_component("esp_http_server")
    esp32.include_builtin_idf_component("esp-tls")
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HTTPS_SERVER_ENABLE", True)
