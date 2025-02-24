import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID

directolor_ns = cg.esphome_ns.namespace("directolor")
Directolor = directolor_ns.class_("Directolor", cover.Cover, cg.Component)

cg.add_library("SPI", None)
cg.add_library("nrf24/RF24", None)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(Directolor),
    cv.Required("led_pin"): cv.int_range(min=0, max=40)  # Require an LED pin (0-40)
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    
    cg.add(var.set_pin(config["led_pin"]))
