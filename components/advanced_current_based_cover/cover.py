from esphome import automation
import esphome.codegen as cg
from esphome.components import cover, sensor, number, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_MAX_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

CONF_POSITION_SENSOR = "position_sensor"
CONF_CALIBRATION_STATUS_SENSOR = "calibration_status_sensor"
CONF_OPEN_DURATION_NUMBER = "open_duration_number"
CONF_CLOSE_DURATION_NUMBER = "close_duration_number"

CONF_OPEN_SENSOR = "open_sensor"
CONF_OPEN_MOVING_CURRENT_THRESHOLD = "open_moving_current_threshold"
CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD = "open_obstacle_current_threshold"
CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD = "open_endstop_current_threshold"

CONF_CLOSE_SENSOR = "close_sensor"
CONF_CLOSE_MOVING_CURRENT_THRESHOLD = "close_moving_current_threshold"
CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD = "close_obstacle_current_threshold"
CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD = "close_endstop_current_threshold"

CONF_OBSTACLE_ROLLBACK = "obstacle_rollback"
CONF_TIMEOUT_MARGIN = "timeout_margin"
CONF_MALFUNCTION_DETECTION = "malfunction_detection"
CONF_MALFUNCTION_ACTION = "malfunction_action"
CONF_START_SENSING_DELAY = "start_sensing_delay"
CONF_STARTUP_DELAY = "startup_delay"

# Advanced calibration features
CONF_AUTO_CALIBRATION_ON_BOOT = "auto_calibration_on_boot"
CONF_CALIBRATION_COMPLETE_ACTION = "calibration_complete_action"
CONF_CALIBRATION_FAILED_ACTION = "calibration_failed_action"
CONF_SAVE_CALIBRATION = "save_calibration"
CONF_ENDSTOP_DETECTION_TIME = "endstop_detection_time"
CONF_CALIBRATION_ENDSTOP_THRESHOLD = "calibration_endstop_threshold"

# Dynamic threshold number entities
CONF_OPEN_MOVING_CURRENT_THRESHOLD_NUMBER = "open_moving_current_threshold_number"
CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD_NUMBER = "open_obstacle_current_threshold_number"
CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD_NUMBER = "open_endstop_current_threshold_number"
CONF_CLOSE_MOVING_CURRENT_THRESHOLD_NUMBER = "close_moving_current_threshold_number"
CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD_NUMBER = "close_obstacle_current_threshold_number"
CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD_NUMBER = "close_endstop_current_threshold_number"
CONF_CALIBRATION_ENDSTOP_THRESHOLD_NUMBER = "calibration_endstop_threshold_number"

advanced_current_based_ns = cg.esphome_ns.namespace("advanced_current_based")
AdvancedCurrentBasedCover = advanced_current_based_ns.class_(
    "AdvancedCurrentBasedCover", cover.Cover, cg.Component
)

# Actions
CalibrateAction = advanced_current_based_ns.class_("CalibrateAction", automation.Action)
ForceOpenAction = advanced_current_based_ns.class_("ForceOpenAction", automation.Action)
ForceCloseAction = advanced_current_based_ns.class_("ForceCloseAction", automation.Action)

CONFIG_SCHEMA = (
    cover.cover_schema(AdvancedCurrentBasedCover)
    .extend(
        {
            cv.Optional(CONF_POSITION_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_CALIBRATION_STATUS_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_OPEN_DURATION_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_CLOSE_DURATION_NUMBER): cv.use_id(number.Number),
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_OPEN_MOVING_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=False
            ),
            cv.Optional(CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=False
            ),
            cv.Optional(CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=True
            ),
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CLOSE_MOVING_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=False
            ),
            cv.Optional(CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=False
            ),
            cv.Optional(CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD): cv.float_range(
                min=0, min_included=True
            ),
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OBSTACLE_ROLLBACK, default="10%"): cv.percentage,
            cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_TIMEOUT_MARGIN, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MALFUNCTION_DETECTION, default=True): cv.boolean,
            cv.Optional(CONF_MALFUNCTION_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_START_SENSING_DELAY, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_STARTUP_DELAY, default="0ms"
            ): cv.positive_time_period_milliseconds,
            # Advanced calibration features
            cv.Optional(CONF_AUTO_CALIBRATION_ON_BOOT, default=False): cv.boolean,
            cv.Optional(CONF_CALIBRATION_COMPLETE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_CALIBRATION_FAILED_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SAVE_CALIBRATION, default=True): cv.boolean,
            cv.Optional(
                CONF_ENDSTOP_DETECTION_TIME, default="1000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_CALIBRATION_ENDSTOP_THRESHOLD, default=0.10
            ): cv.float_range(min=0, min_included=False),
            # Dynamic threshold number entities
            cv.Optional(CONF_OPEN_MOVING_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_CLOSE_MOVING_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_CALIBRATION_ENDSTOP_THRESHOLD_NUMBER): cv.use_id(number.Number),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    if CONF_POSITION_SENSOR in config:
        sens = await cg.get_variable(config[CONF_POSITION_SENSOR])
        cg.add(var.set_position_sensor(sens))

    if CONF_CALIBRATION_STATUS_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CALIBRATION_STATUS_SENSOR])
        cg.add(var.set_calibration_status_sensor(sens))

    if CONF_OPEN_DURATION_NUMBER in config:
        num = await cg.get_variable(config[CONF_OPEN_DURATION_NUMBER])
        cg.add(var.set_open_duration_number(num))

    if CONF_CLOSE_DURATION_NUMBER in config:
        num = await cg.get_variable(config[CONF_CLOSE_DURATION_NUMBER])
        cg.add(var.set_close_duration_number(num))

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    # OPEN
    bin = await cg.get_variable(config[CONF_OPEN_SENSOR])
    cg.add(var.set_open_sensor(bin))
    cg.add(
        var.set_open_moving_current_threshold(
            config[CONF_OPEN_MOVING_CURRENT_THRESHOLD]
        )
    )
    if (
        open_obstacle_current_threshold := config.get(
            CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD
        )
    ) is not None:
        cg.add(var.set_open_obstacle_current_threshold(open_obstacle_current_threshold))

    if (
        open_endstop_current_threshold := config.get(
            CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD
        )
    ) is not None:
        cg.add(var.set_open_endstop_current_threshold(open_endstop_current_threshold))

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    # CLOSE
    bin = await cg.get_variable(config[CONF_CLOSE_SENSOR])
    cg.add(var.set_close_sensor(bin))
    cg.add(
        var.set_close_moving_current_threshold(
            config[CONF_CLOSE_MOVING_CURRENT_THRESHOLD]
        )
    )
    if (
        close_obstacle_current_threshold := config.get(
            CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD
        )
    ) is not None:
        cg.add(
            var.set_close_obstacle_current_threshold(close_obstacle_current_threshold)
        )

    if (
        close_endstop_current_threshold := config.get(
            CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD
        )
    ) is not None:
        cg.add(var.set_close_endstop_current_threshold(close_endstop_current_threshold))

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    cg.add(var.set_obstacle_rollback(config[CONF_OBSTACLE_ROLLBACK]))
    if (max_duration := config.get(CONF_MAX_DURATION)) is not None:
        cg.add(var.set_max_duration(max_duration))
    cg.add(var.set_timeout_margin(config[CONF_TIMEOUT_MARGIN]))
    cg.add(var.set_malfunction_detection(config[CONF_MALFUNCTION_DETECTION]))
    if malfunction_action := config.get(CONF_MALFUNCTION_ACTION):
        await automation.build_automation(
            var.get_malfunction_trigger(), [], malfunction_action
        )
    cg.add(var.set_start_sensing_delay(config[CONF_START_SENSING_DELAY]))
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))

    # Advanced calibration features
    cg.add(var.set_auto_calibration_on_boot(config[CONF_AUTO_CALIBRATION_ON_BOOT]))
    cg.add(var.set_save_calibration(config[CONF_SAVE_CALIBRATION]))
    cg.add(var.set_endstop_detection_time(config[CONF_ENDSTOP_DETECTION_TIME]))
    cg.add(var.set_calibration_endstop_threshold(config[CONF_CALIBRATION_ENDSTOP_THRESHOLD]))

    # Connect dynamic threshold number entities if provided
    if CONF_OPEN_MOVING_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_OPEN_MOVING_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_open_moving_current_threshold_number(num))

    if CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_open_obstacle_current_threshold_number(num))

    if CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_OPEN_ENDSTOP_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_open_endstop_current_threshold_number(num))

    if CONF_CLOSE_MOVING_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_CLOSE_MOVING_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_close_moving_current_threshold_number(num))

    if CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_close_obstacle_current_threshold_number(num))

    if CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_CLOSE_ENDSTOP_CURRENT_THRESHOLD_NUMBER])
        cg.add(var.set_close_endstop_current_threshold_number(num))

    if CONF_CALIBRATION_ENDSTOP_THRESHOLD_NUMBER in config:
        num = await cg.get_variable(config[CONF_CALIBRATION_ENDSTOP_THRESHOLD_NUMBER])
        cg.add(var.set_calibration_endstop_threshold_number(num))

    if calibration_complete_action := config.get(CONF_CALIBRATION_COMPLETE_ACTION):
        await automation.build_automation(
            var.get_calibration_complete_trigger(), [], calibration_complete_action
        )

    if calibration_failed_action := config.get(CONF_CALIBRATION_FAILED_ACTION):
        await automation.build_automation(
            var.get_calibration_failed_trigger(), [], calibration_failed_action
        )


@automation.register_action(
    "advanced_current_based_cover.calibrate",
    CalibrateAction,
    automation.maybe_simple_id(
        {
            cv.Required(automation.CONF_ID): cv.use_id(AdvancedCurrentBasedCover),
        }
    ),
)
async def calibrate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[automation.CONF_ID])
    return var


@automation.register_action(
    "advanced_current_based_cover.force_open",
    ForceOpenAction,
    automation.maybe_simple_id(
        {
            cv.Required(automation.CONF_ID): cv.use_id(AdvancedCurrentBasedCover),
        }
    ),
)
async def force_open_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[automation.CONF_ID])
    return var


@automation.register_action(
    "advanced_current_based_cover.force_close",
    ForceCloseAction,
    automation.maybe_simple_id(
        {
            cv.Required(automation.CONF_ID): cv.use_id(AdvancedCurrentBasedCover),
        }
    ),
)
async def force_close_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[automation.CONF_ID])
    return var
