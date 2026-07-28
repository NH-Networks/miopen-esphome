"""ESPHome iohomecontrol cover sub-platform.

Allows statically declaring io-homecontrol cover entities in YAML.
Each entry requires a device_id (the 6-hex-char node address, e.g. a1b2c3)
and an optional human-readable name.

Covers declared here are registered at compile time. Covers discovered at
runtime via pairing are handled automatically by the gateway component
without needing an entry here.

Example YAML usage:
  iohomecontrol:
    ...
    covers:
      - device_id: a1b2c3
        name: Living Room Blind
      - device_id: d4e5f6
        name: Bedroom Blind
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID

# Re-use the namespace declared in __init__.py
iohomecontrol_ns = cg.global_ns.namespace("iohomecontrol")
IohcGatewayComponent = iohomecontrol_ns.class_("IohcGatewayComponent", cg.Component)
IohcCover            = iohomecontrol_ns.class_("IohcCover", cover.Cover)

CONF_DEVICE_ID  = "device_id"
CONF_GATEWAY_ID = "gateway_id"

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(IohcCover),
    cv.Required(CONF_DEVICE_ID):  cv.string,
    cv.Required(CONF_GATEWAY_ID): cv.use_id(IohcGatewayComponent),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    gateway = await cg.get_variable(config[CONF_GATEWAY_ID])
    device_id = config[CONF_DEVICE_ID]

    cov = cg.new_Pvariable(config[CONF_ID], device_id,
                           cg.RawExpression(gateway + ".get_gateway()"))
    await cg.register_component(cov, config)
    await cover.register_cover(cov, config)
    cg.add(gateway.register_cover(cov))
