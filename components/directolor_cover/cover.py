import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID
from ..nrf24l01_base import nrf24l01  # This imports esphome::nrf24l01_base::Nrf24l01_base

DEPENDENCIES = ["nrf24l01_base"]

AUTO_LOAD = ["button"]

# Define the namespace and class for DirectolorCover
directolor_cover_ns = cg.esphome_ns.namespace("directolor_cover")
DirectolorCover = directolor_cover_ns.class_("DirectolorCover", cover.Cover, cg.Component)

cg.add_library("robtillaart/CRC", None)

# Configuration schema
CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(DirectolorCover),
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
    cv.Optional("movement_duration", default="0s"): cv.positive_time_period_seconds,  # Duration in seconds
    cv.Required("channel"): cv.int_range(min=1, max=63),  # Required channel 1-6
    cv.Optional("tilt_supported", default=False): cv.boolean,  # Optional tilt support, defaults to false
    cv.Optional("favorite_support", default=False): cv.boolean,
    cv.Optional("program_support", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

# Generate C++ code
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    base = await cg.get_variable(config["base"])  # Retrieves the Nrf24l01_base object
    cg.add(var.set_base(base))  # Pass the base object directly
    cg.add(var.set_radio_code(config["radio_code"][0], config["radio_code"][1], config["radio_code"][2], config["radio_code"][3]))   
    cg.add(var.set_movement_duration(config["movement_duration"]))
    cg.add(var.set_tilt_supported(config["tilt_supported"]))
    cg.add(var.set_channel(config["channel"]))
    cg.add(var.set_favorite_support(config["favorite_support"]))
    cg.add(var.set_program_function_support(config["program_support"]))