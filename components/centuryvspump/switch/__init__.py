from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS
from esphome.cpp_helpers import logging

from .. import (
    add_century_vs_pump_base_properties,
    century_vs_pump_ns,
    # modbus_calc_properties,
    # validate_modbus_register,
    # ModbusItemBaseSchema,
    CenturyVSPumpItemSchema,
    # SensorItem,
    # MODBUS_REGISTER_TYPE,
    # SENSOR_VALUE_TYPE,
)
from ..const import (
    # CONF_BITMASK,
    # CONF_FORCE_NEW_RANGE,
    # CONF_MODBUS_CONTROLLER_ID,
    CONF_CENTURY_VS_PUMP_ID,
    # CONF_PAGE,
    # CONF_SCALE,
    # CONF_REGISTER_COUNT,
    # CONF_REGISTER_TYPE,
    # CONF_SKIP_UPDATES,
    # CONF_VALUE_TYPE,
)

DEPENDENCIES = ["centuryvspump"]
CODEOWNERS = ["@gazoodle"]

_LOGGER = logging.getLogger(__name__)


CenturyVSPumpRunSwitch = century_vs_pump_ns.class_(
    "CenturyVSPumpRunSwitch", cg.Component, switch.Switch  # , SensorItem
)


CONFIG_SCHEMA = cv.All(
    switch.SWITCH_SCHEMA.extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpRunSwitch),
        }
    ),
)


async def to_code(config):

    # byte_offset, reg_count = modbus_calc_properties(config)
    # value_type = config[CONF_VALUE_TYPE]
    var = cg.new_Pvariable(
        config[CONF_ID],
        # config[CONF_REGISTER_TYPE],
        # config[CONF_PAGE],
        # config[CONF_ADDRESS],
        # config[CONF_SCALE],
        # byte_offset,
        # config[CONF_BITMASK],
        # value_type,
        # reg_count,
        # config[CONF_SKIP_UPDATES],
        # config[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
    await add_century_vs_pump_base_properties(var, config, CenturyVSPumpRunSwitch)
