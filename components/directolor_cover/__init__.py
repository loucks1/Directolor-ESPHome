import esphome.codegen as cg
import esphome.components.button as button
from esphome.const import ICON_RESTART

dummy_button = button.button.Schema(
    {
        cg.CONF_ID: "dummy_join_button",
        button.CONF_ICON: ICON_RESTART,
        button.CONF_NAME: "Dummy Join Button",
    }
)