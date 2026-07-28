import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID

from . import IohcGatewayComponent, iohomecontrol_ns

DEPENDENCIES = ["iohomecontrol"]

CONF_IOHOMECONTROL_ID = "iohomecontrol_id"
CONF_DEVICE_ID = "device_id"

IohcCover = iohomecontrol_ns.class_("IohcCover", cover.Cover)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(IohcCover),
        cv.GenerateID(CONF_IOHOMECONTROL_ID): cv.use_id(IohcGatewayComponent),
        cv.Required(CONF_DEVICE_ID): cv.string,
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_IOHOMECONTROL_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_DEVICE_ID], parent.get_gateway())
    await cover.register_cover(var, config)
    cg.add(parent.register_cover(var))
