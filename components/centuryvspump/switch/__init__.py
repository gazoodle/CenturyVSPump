from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS
from esphome.cpp_helpers import logging

from .. import (
    add_century_vs_pump_base_properties,
    century_vs_pump_ns,
    CenturyVSPumpItemSchema,
)
from ..const import (
    CONF_CENTURY_VS_PUMP_ID,
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
    var = cg.new_Pvariable(
        config[CONF_ID],
    )
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
    await add_century_vs_pump_base_properties(var, config, CenturyVSPumpRunSwitch)
