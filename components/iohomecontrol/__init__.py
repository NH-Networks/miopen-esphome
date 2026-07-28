"""ESPHome external component for io-homecontrol (Somfy/Velux) blinds.

Minimum ESPHome version: 2026.7.0
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover, button, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    UNIT_DECIBEL,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
)

AUTO_LOAD = ["cover", "button", "sensor", "text_sensor", "web_server_base", "spi"]

iohomecontrol_ns = cg.global_ns.namespace("iohomecontrol")

# Component classes
IohcGatewayComponent = iohomecontrol_ns.class_("IohcGatewayComponent", cg.Component)

# Button classes
IohcScanButton      = iohomecontrol_ns.class_("IohcScanButton",      button.Button, cg.Component)
IohcAddButton       = iohomecontrol_ns.class_("IohcAddButton",       button.Button, cg.Component)
IohcRemoveButton    = iohomecontrol_ns.class_("IohcRemoveButton",    button.Button, cg.Component)
IohcNewRemoteButton = iohomecontrol_ns.class_("IohcNewRemoteButton", button.Button, cg.Component)
IohcReloadButton    = iohomecontrol_ns.class_("IohcReloadButton",    button.Button, cg.Component)

# Action classes
ScanAction      = iohomecontrol_ns.class_("ScanAction",      automation.Action)
AddAction       = iohomecontrol_ns.class_("AddAction",       automation.Action)
RemoveAction    = iohomecontrol_ns.class_("RemoveAction",    automation.Action)
NewRemoteAction = iohomecontrol_ns.class_("NewRemoteAction", automation.Action)
ReloadAction    = iohomecontrol_ns.class_("ReloadAction",    automation.Action)

# Config keys
CONF_SCK_PIN        = "sck_pin"
CONF_MISO_PIN       = "miso_pin"
CONF_MOSI_PIN       = "mosi_pin"
CONF_NSS_PIN        = "nss_pin"
CONF_RESET_PIN      = "reset_pin"
CONF_DIO0_PIN       = "dio0_pin"
CONF_DIO1_PIN       = "dio1_pin"
CONF_FREQUENCY      = "frequency"
CONF_DEVICES_FILE   = "devices_file"
CONF_RSSI_SENSOR    = "rssi_sensor"
CONF_RX_COUNTER     = "rx_counter"
CONF_TX_COUNTER     = "tx_counter"
CONF_STATUS_SENSOR  = "status_sensor"
CONF_LAST_ADDR      = "last_addr_sensor"
CONF_PENDING        = "pending_sensor"
CONF_SCAN_BUTTON    = "scan_button"
CONF_ADD_BUTTON     = "add_button"
CONF_REMOVE_BUTTON  = "remove_button"
CONF_NEW_REMOTE_BTN = "new_remote_button"
CONF_RELOAD_BTN     = "reload_button"
CONF_TARGET         = "target"
CONF_NAME           = "name"
CONF_COZY_FILE      = "cozy_devices_file"
CONF_OTHER_FILE     = "other_devices_file"
CONF_RADIO_PLATFORM = "radio_platform"
CONF_REMOTES        = "remotes"
CONF_DEVICES        = "devices"

SCAN_BUTTON_SCHEMA      = button.button_schema(IohcScanButton)
ADD_BUTTON_SCHEMA       = button.button_schema(IohcAddButton)
REMOVE_BUTTON_SCHEMA    = button.button_schema(IohcRemoveButton)
NEW_REMOTE_BTN_SCHEMA   = button.button_schema(IohcNewRemoteButton)
RELOAD_BTN_SCHEMA       = button.button_schema(IohcReloadButton)

REMOTE_MAP_SCHEMA = cv.Schema({
    cv.Required(CONF_NAME): cv.string,
    cv.Required(CONF_DEVICES): cv.ensure_list(cv.string),
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(IohcGatewayComponent),
    cv.Optional(CONF_RADIO_PLATFORM, default="sx1276"): cv.one_of("sx1276", "cc1101", lower=True),
    cv.Required(CONF_SCK_PIN):   cv.int_,
    cv.Required(CONF_MISO_PIN):  cv.int_,
    cv.Required(CONF_MOSI_PIN):  cv.int_,
    cv.Required(CONF_NSS_PIN):   cv.int_,
    cv.Optional(CONF_RESET_PIN, default=-1): cv.int_,
    cv.Optional(CONF_DIO0_PIN, default=-1):  cv.int_,
    cv.Optional(CONF_DIO1_PIN, default=-1): cv.int_,
    cv.Optional(CONF_FREQUENCY,    default=868950000):  cv.int_,
    cv.Optional(CONF_DEVICES_FILE, default="/1W.json"): cv.string,
    cv.Optional(CONF_COZY_FILE, default="/2W.json"): cv.string,
    cv.Optional(CONF_OTHER_FILE, default="/other_2w.json"): cv.string,
    cv.Optional(CONF_REMOTES): cv.ensure_list(REMOTE_MAP_SCHEMA),
    cv.Optional(CONF_RSSI_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_RX_COUNTER):    sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_TX_COUNTER):    sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_STATUS_SENSOR): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_LAST_ADDR):     text_sensor.text_sensor_schema(),
    cv.Optional(CONF_PENDING):       text_sensor.text_sensor_schema(),
    cv.Optional(CONF_SCAN_BUTTON):    SCAN_BUTTON_SCHEMA,
    cv.Optional(CONF_ADD_BUTTON):     ADD_BUTTON_SCHEMA,
    cv.Optional(CONF_REMOVE_BUTTON):  REMOVE_BUTTON_SCHEMA,
    cv.Optional(CONF_NEW_REMOTE_BTN): NEW_REMOTE_BTN_SCHEMA,
    cv.Optional(CONF_RELOAD_BTN):     RELOAD_BTN_SCHEMA,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_library("Preferences", None)
    cg.add_library("LittleFS", None)
    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sck_pin(config[CONF_SCK_PIN]))
    cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
    cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
    cg.add(var.set_nss_pin(config[CONF_NSS_PIN]))
    cg.add(var.set_reset_pin(config[CONF_RESET_PIN]))
    cg.add(var.set_dio0_pin(config[CONF_DIO0_PIN]))
    cg.add(var.set_dio1_pin(config[CONF_DIO1_PIN]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_devices_file(config[CONF_DEVICES_FILE]))
    cg.add(var.set_cozy_file(config[CONF_COZY_FILE]))
    cg.add(var.set_other_file(config[CONF_OTHER_FILE]))
    cg.add(var.set_radio_platform(config[CONF_RADIO_PLATFORM]))

    if CONF_REMOTES in config:
        for remote in config[CONF_REMOTES]:
            name = remote[CONF_NAME]
            devices = remote[CONF_DEVICES]
            cg.add(var.add_remote_map(name, devices))

    if CONF_RSSI_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_RSSI_SENSOR])
        cg.add(var.set_rssi_sensor(sens))
    if CONF_RX_COUNTER in config:
        sens = await sensor.new_sensor(config[CONF_RX_COUNTER])
        cg.add(var.set_rx_counter(sens))
    if CONF_TX_COUNTER in config:
        sens = await sensor.new_sensor(config[CONF_TX_COUNTER])
        cg.add(var.set_tx_counter(sens))
    if CONF_STATUS_SENSOR in config:
        ts = await text_sensor.new_text_sensor(config[CONF_STATUS_SENSOR])
        cg.add(var.set_status_sensor(ts))
    if CONF_LAST_ADDR in config:
        ts = await text_sensor.new_text_sensor(config[CONF_LAST_ADDR])
        cg.add(var.set_last_addr_sensor(ts))
    if CONF_PENDING in config:
        ts = await text_sensor.new_text_sensor(config[CONF_PENDING])
        cg.add(var.set_pending_sensor(ts))

    if CONF_SCAN_BUTTON in config:
        btn = cg.new_Pvariable(config[CONF_SCAN_BUTTON][CONF_ID])
        await button.register_button(btn, config[CONF_SCAN_BUTTON])
        cg.add(var.set_scan_button(btn))
    if CONF_ADD_BUTTON in config:
        btn = cg.new_Pvariable(config[CONF_ADD_BUTTON][CONF_ID])
        await button.register_button(btn, config[CONF_ADD_BUTTON])
        cg.add(var.set_add_button(btn))
    if CONF_REMOVE_BUTTON in config:
        btn = cg.new_Pvariable(config[CONF_REMOVE_BUTTON][CONF_ID])
        await button.register_button(btn, config[CONF_REMOVE_BUTTON])
        cg.add(var.set_remove_button(btn))
    if CONF_NEW_REMOTE_BTN in config:
        btn = cg.new_Pvariable(config[CONF_NEW_REMOTE_BTN][CONF_ID])
        await button.register_button(btn, config[CONF_NEW_REMOTE_BTN])
        cg.add(var.set_new_remote_button(btn))
    if CONF_RELOAD_BTN in config:
        btn = cg.new_Pvariable(config[CONF_RELOAD_BTN][CONF_ID])
        await button.register_button(btn, config[CONF_RELOAD_BTN])
        cg.add(var.set_reload_button(btn))

@automation.register_action(
    "iohomecontrol.scan",
    ScanAction,
    cv.Schema({cv.GenerateID(): cv.use_id(IohcGatewayComponent)}),
    synchronous=True,
)
async def scan_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var

@automation.register_action(
    "iohomecontrol.add",
    AddAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(IohcGatewayComponent),
        cv.Required(CONF_TARGET): cv.templatable(cv.string),
    }),
    synchronous=True,
)
async def add_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_TARGET], args, cg.std_string)
    cg.add(var.set_target(template_))
    return var

@automation.register_action(
    "iohomecontrol.remove",
    RemoveAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(IohcGatewayComponent),
        cv.Required(CONF_TARGET): cv.templatable(cv.string),
    }),
    synchronous=True,
)
async def remove_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_TARGET], args, cg.std_string)
    cg.add(var.set_target(template_))
    return var

@automation.register_action(
    "iohomecontrol.new_remote",
    NewRemoteAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(IohcGatewayComponent),
        cv.Required(CONF_NAME): cv.templatable(cv.string),
    }),
    synchronous=True,
)
async def new_remote_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_NAME], args, cg.std_string)
    cg.add(var.set_name(template_))
    return var

@automation.register_action(
    "iohomecontrol.reload",
    ReloadAction,
    cv.Schema({cv.GenerateID(): cv.use_id(IohcGatewayComponent)}),
    synchronous=True,
)
async def reload_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var
