import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Define the namespace and class for the base component
nrf24l01_base_ns = cg.esphome_ns.namespace("nrf24l01_base")
nrf24l01 = nrf24l01_base_ns.class_("Nrf24l01_base", cg.Component)

# Add required libraries
cg.add_library("SPI", None)
cg.add_library("nrf24/RF24", None)

# Configuration schema for the base component
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(nrf24l01),
    cv.Required("ce_pin"): cv.int_range(min=0, max=40),
    cv.Required("cs_pin"): cv.int_range(min=0, max=40),
}).extend(cv.COMPONENT_SCHEMA)

# Generate C++ code for the component
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_ce_pin(config["ce_pin"]))
    cg.add(var.set_cs_pin(config["cs_pin"]))