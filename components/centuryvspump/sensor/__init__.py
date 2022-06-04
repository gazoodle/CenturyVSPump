from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS, CONF_TYPE
from esphome.cpp_helpers import logging

from .. import (
    add_century_vs_pump_base_properties,
    century_vs_pump_ns,
    CenturyVSPumpItemSchema,
)
from ..const import (
    CONF_CENTURY_VS_PUMP_ID,
    CONF_PAGE,
    CONF_SCALE,
)

DEPENDENCIES = ["centuryvspump"]
CODEOWNERS = ["@gazoodle"]

_LOGGER = logging.getLogger(__name__)


CenturyVSPumpSensor = century_vs_pump_ns.class_(
    "CenturyVSPumpSensor", cg.Component, sensor.Sensor
)

SENSOR_TYPE = century_vs_pump_ns.enum("Type")

SENSOR_TYPES = {"rpm": SENSOR_TYPE.rpm, "custom": SENSOR_TYPE.custom}

CONFIG_SCHEMA = cv.All(
    sensor.SENSOR_SCHEMA.extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpSensor),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES),
            cv.Optional(CONF_ADDRESS, default=0): cv.positive_int,
            cv.Optional(CONF_PAGE, default=0): cv.positive_int,
            cv.Optional(CONF_SCALE, default=1): cv.positive_int,
        }
    ),
)


async def to_code(config):
    if config[CONF_TYPE] == "rpm":
        config[CONF_PAGE] = 0
        config[CONF_ADDRESS] = 0
        config[CONF_SCALE] = 4

    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_PAGE],
        config[CONF_ADDRESS],
        config[CONF_SCALE],
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
    await add_century_vs_pump_base_properties(var, config, CenturyVSPumpSensor)
