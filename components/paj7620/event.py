import esphome.codegen as cg
from esphome.components import event, i2c
import esphome.config_validation as cv
import esphome.pins as pins

CODEOWNERS = ["@aalaei"]
DEPENDENCIES = ["i2c"]

CONF_INT_PIN = "int_pin"

paj7620_ns = cg.esphome_ns.namespace("paj7620")
PAJ7620Component = paj7620_ns.class_(
    "PAJ7620Component", event.Event, cg.PollingComponent, i2c.I2CDevice
)

EVENT_TYPES = [
    "up",
    "down",
    "left",
    "right",
    "forward",
    "backward",
    "clockwise",
    "count_clockwise",
    "wave",
]

CONFIG_SCHEMA = (
    event.event_schema(PAJ7620Component, device_class="motion")
    .extend(
        {
            # Active-low GESTURE-interrupt output (INT). When wired, gestures are
            # read as soon as they're ready instead of on a fixed poll interval --
            # far fewer I2C transactions (also reduces the odds of catching the
            # sensor mid-transaction on an MCU reset -- see the wedge/power-cycle
            # issue this replaces most of the polling for) and less bus time spent
            # between gestures. `update_interval` still applies as a safety-net
            # poll in case an interrupt is missed or glitched.
            cv.Optional(CONF_INT_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("100ms"))
    .extend(i2c.i2c_device_schema(0x73))
)


async def to_code(config):
    var = await event.new_event(config, event_types=EVENT_TYPES)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if int_pin_config := config.get(CONF_INT_PIN):
        cg.add(var.set_int_pin(await cg.gpio_pin_expression(int_pin_config)))
