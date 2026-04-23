import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover

# 1. Manually define the nrf24 namespace to match what you just showed me
nrf24_ns = cg.esphome_ns.namespace("nrf24")
NRF24Component = nrf24_ns.class_("NRF24Component", cg.Component)

# 2. Your cover namespace
directolor_cover_ns = cg.esphome_ns.namespace("directolor_cover")
DirectolorCover = directolor_cover_ns.class_("DirectolorCover", cover.Cover, cg.Component)

# 3. Configuration schema
CONFIG_SCHEMA = cover.cover_schema(DirectolorCover).extend({
    cv.GenerateID(): cv.declare_id(DirectolorCover),
    
    # Use the NRF24Component class we defined above
    cv.Required("nrf24_id"): cv.use_id(NRF24Component), 
    
    cv.Required("radio_code"): cv.All(
        cv.ensure_list(cv.hex_int_range(min=0, max=255)),
        cv.Length(min=4, max=4),
    ),
    cv.Optional("movement_duration", default="0s"): cv.positive_time_period_seconds,
    cv.Required("channel"): cv.int_range(min=1, max=63),
    cv.Optional("tilt_supported", default=False): cv.boolean,
    cv.Optional("directolor_code_attempts", default=2): cv.int_range(min=1, max=10),
    cv.Optional("message_send_repeats", default=256): cv.int_range(min=1, max=1000)
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    
    # 4. Link the nrf24 variable
    parent = await cg.get_variable(config["nrf24_id"])
    cg.add(var.set_nrf24(parent))
    
    cg.add(var.set_radio_code(config["radio_code"]))
    cg.add(var.set_movement_duration(config["movement_duration"]))
    cg.add(var.set_tilt_supported(config["tilt_supported"]))
    cg.add(var.set_channel(config["channel"]))
    cg.add(var.set_directolor_code_attempts(config["directolor_code_attempts"]))
    cg.add(var.set_message_send_repeats(config["message_send_repeats"]))
    