import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_ICON
from esphome.components import nrf24

# Namespace and classes
directolor_radio_ns = cg.esphome_ns.namespace("directolor_radio")
DirectolorRadio = directolor_radio_ns.class_("DirectolorRadio", cg.Component)

CONF_NRF24_ID = "nrf24_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DirectolorRadio),
        cv.Required(CONF_NRF24_ID): cv.use_id(nrf24.NRF24Component),
        cv.Optional("directolor_code_attempts", default=3): cv.int_range(min=1, max=10),
        cv.Optional("message_send_repeats", default=513): cv.int_range(min=1, max=1000),
        cv.Optional("intermessage_cooldown", default=30): cv.int_range(min=0, max=255),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    nrf24_var = await cg.get_variable(config[CONF_NRF24_ID])
    cg.add(var.set_nrf24(nrf24_var))
    cg.add(var.set_directolor_code_attempts(config["directolor_code_attempts"]))
    cg.add(var.set_message_send_repeats(config["message_send_repeats"]))
    cg.add(var.set_cooldown(config["intermessage_cooldown"]))