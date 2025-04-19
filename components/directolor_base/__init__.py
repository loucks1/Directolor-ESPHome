import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from ..nrf24l01_base import nrf24l01  # This imports esphome::nrf24l01_base::Nrf24l01_base

DEPENDENCIES = ["nrf24l01_base"]

directolor_base_ns = cg.esphome_ns.namespace("directolor_base")
directolor = directolor_base_ns.class_("Directolor_base", cg.Component)

cg.add_library("robtillaart/CRC", None)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(directolor),
    cv.Required("base"): cv.use_id(nrf24l01),  # Correct type: esphome::nrf24l01_base::Nrf24l01_base
    cv.Required("radio_code"): cv.All(
        cv.Schema([
            cv.hex_int_range(min=0, max=255),  # Accepts hex (e.g., 0x02) or decimal (e.g., 2)
            cv.hex_int_range(min=0, max=255),
            cv.hex_int_range(min=0, max=255),
            cv.hex_int_range(min=0, max=255),
        ]),
        cv.Length(min=4, max=4),
    ),
    cv.Required("channel"): cv.int_range(min=1, max=6),  # Required channel 1-6
}).extend(cv.COMPONENT_SCHEMA)

# Generate C++ code for the component
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_base(base))  # Pass the base object directly
    cg.add(var.set_radio_code(config["radio_code"][0], config["radio_code"][1], config["radio_code"][2], config["radio_code"][3]))   
    cg.add(var.set_channel(config["channel"]))