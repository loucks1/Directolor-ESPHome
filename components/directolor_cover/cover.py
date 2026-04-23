import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID

# 1. Import the DirectolorRadio namespace and class
# This assumes your radio component is in a folder named 'directolor_radio'
directolor_radio_ns = cg.esphome_ns.namespace("directolor_radio")
DirectolorRadio = directolor_radio_ns.class_("DirectolorRadio", cg.Component)

# 2. Your cover namespace
directolor_cover_ns = cg.esphome_ns.namespace("directolor_cover")
DirectolorCover = directolor_cover_ns.class_("DirectolorCover", cover.Cover, cg.Component)

# 3. Configuration schema
CONFIG_SCHEMA = cover.cover_schema(DirectolorCover).extend({
    cv.GenerateID(): cv.declare_id(DirectolorCover),
    
    # Change dependency from nrf24 to your new directolor_radio hub
    cv.Required("directolor_radio_id"): cv.use_id(DirectolorRadio), 
    
    cv.Required("radio_code"): cv.All(
        cv.ensure_list(cv.hex_int_range(min=0, max=255)),
        cv.Length(min=4, max=4),
    ),
    cv.Optional("movement_duration", default="0s"): cv.positive_time_period_seconds,
    cv.Required("channel"): cv.int_range(min=1, max=63),
    cv.Optional("tilt_supported", default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    
    # 4. Link to the DirectolorRadio hub instead of raw NRF24
    parent = await cg.get_variable(config["directolor_radio_id"])
    # You will need a method in your Cover C++ class: void set_radio_hub(DirectolorRadio *hub)
    cg.add(var.set_radio_hub(parent))
    
    cg.add(var.set_radio_code(config["radio_code"]))
    cg.add(var.set_movement_duration(config["movement_duration"].total_seconds))
    cg.add(var.set_tilt_supported(config["tilt_supported"]))
    cg.add(var.set_channel(config["channel"]))
