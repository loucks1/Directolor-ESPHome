import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

empty_button_ns = cg.esphome_ns.namespace("empty_button")
EmptyButton = empty_button_ns.class_("EmptyButton", button.Button, cg.Component)

CONFIG_SCHEMA = button.button_schema(EmptyButton).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)