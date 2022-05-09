#pragma once

#include <Arduino.h>

struct MotorConfig {
    float zero_electric_angle;
    int pole_pairs;
    int sensor_direction;
    int calibrated;
};