import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import aip33628,light,switch,select,number,button,binary_sensor,sensor,text_sensor,clock_cgm,additional_cgm
from esphome.const import CONF_ID
DEPENDENCIES=["clock_cgm","additional_cgm"]
ns=cg.esphome_ns.namespace("clock_controls")
ClockControls=ns.class_("ClockControls",cg.Component)
CONFIG_SCHEMA=cv.Schema({
 cv.GenerateID():cv.declare_id(ClockControls),
 cv.Required("panel_id"):cv.use_id(aip33628.Aip33628Panel),
 cv.Required("light_id"):cv.use_id(light.LightState),
 cv.Required("cgm_id"):cv.use_id(clock_cgm.ClockCGM),
 cv.Required("additional_id"):cv.use_id(additional_cgm.AdditionalCGM),
 cv.Required("switches"):cv.Schema({cv.Required("twelve_hour"):cv.use_id(switch.Switch),cv.Required("blink"):cv.use_id(switch.Switch)}),
 cv.Required("selects"):cv.Schema({cv.Required("source"):cv.use_id(select.Select),cv.Required("mode"):cv.use_id(select.Select),cv.Required("region"):cv.use_id(select.Select),cv.Required("effect"):cv.use_id(select.Select),cv.Required("spread"):cv.use_id(select.Select),cv.Required("unit"):cv.use_id(select.Select)}),
 cv.Required("numbers"):cv.Schema({cv.Required("speed"):cv.use_id(number.Number),cv.Required("angle"):cv.use_id(number.Number),cv.Required("hue_span"):cv.use_id(number.Number),cv.Required("flash_fade"):cv.use_id(number.Number),cv.Required("test_number"):cv.use_id(number.Number)}),
 cv.Required("buttons"):cv.Schema({cv.Required("lamp_test"):cv.use_id(button.Button),cv.Required("seconds"):cv.use_id(button.Button),cv.Required("digit_demo"):cv.use_id(button.Button),cv.Required("clear_colors"):cv.use_id(button.Button),cv.Required("gradient"):cv.use_id(button.Button)}),
 cv.Required("binary_sensors"):cv.Schema({cv.Required("top"):cv.use_id(binary_sensor.BinarySensor),cv.Required("bottom"):cv.use_id(binary_sensor.BinarySensor),cv.Required("dx_fresh"):cv.use_id(binary_sensor.BinarySensor),cv.Required("other_fresh"):cv.use_id(binary_sensor.BinarySensor)}),
 cv.Required("sensors"):cv.Schema({cv.Required("dx_glucose"):cv.use_id(sensor.Sensor),cv.Required("dx_age"):cv.use_id(sensor.Sensor),cv.Required("other_glucose"):cv.use_id(sensor.Sensor),cv.Required("other_age"):cv.use_id(sensor.Sensor)}),
 cv.Required("text_sensors"):cv.Schema({cv.Required("dx_trend"):cv.use_id(text_sensor.TextSensor),cv.Required("dx_time"):cv.use_id(text_sensor.TextSensor),cv.Required("other_trend"):cv.use_id(text_sensor.TextSensor),cv.Required("other_time"):cv.use_id(text_sensor.TextSensor)}),
}).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
 var=cg.new_Pvariable(config[CONF_ID]);await cg.register_component(var,config)
 cg.add(var.set_panel(await cg.get_variable(config["panel_id"])))
 cg.add(var.set_light(await cg.get_variable(config["light_id"])))
 cg.add(var.set_connector(await cg.get_variable(config["cgm_id"])))
 cg.add(var.set_additional(await cg.get_variable(config["additional_id"])))
 for group,method in [("switches","add_switch"),("selects","add_select"),("numbers","add_number"),("buttons","add_button"),("binary_sensors","add_binary_sensor"),("sensors","add_sensor"),("text_sensors","add_text_sensor")]:
  for key,ref in config[group].items():cg.add(getattr(var,method)(key,await cg.get_variable(ref)))
