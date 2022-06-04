import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus

from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.cpp_helpers import logging

from .const import CONF_CENTURY_VS_PUMP_ID

CODEOWNERS = ["@gazoodle"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

# pylint: disable=invalid-name
century_vs_pump_ns = cg.esphome_ns.namespace("century_vs_pump")
CenturyVSPump = century_vs_pump_ns.class_(
    "CenturyVSPump", cg.PollingComponent, modbus.ModbusDevice
)

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPump),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(21))
)

CenturyVSPumpItemSchema = cv.Schema(
    {
        cv.GenerateID(CONF_CENTURY_VS_PUMP_ID): cv.use_id(CenturyVSPump),
    }
)


async def add_century_vs_pump_base_properties(
    var,
    config,
    sensor_type,
):
    pass


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_centuryvspump_device(var, config)


async def register_centuryvspump_device(var, config):
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_device(var, config)
