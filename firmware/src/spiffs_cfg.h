#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "knob_data.h"
#include "motor_data.h"
#include "semaphore_guard.h"


class SpiffsCfg
{
public:
    SpiffsCfg();
    ~SpiffsCfg();
    static const int LastCfgIdx  = -1;
    bool GetKnobConfig(const int cfgIdx, KnobConfig *knobCfg);
    bool DeleteKnobConfig(const int cfgIdx);
    bool AppendKnobConfig(KnobConfig knobCfg);
    bool GetNextKnobConfig(int *cfgIdx, KnobConfig *knobCfg);
    bool GetPreviousKnobConfig(int *cfgIdx, KnobConfig *knobCfg);

    bool GetMotorConfig(MotorConfig* motorCfg);
    bool SetMotorConfig(MotorConfig* motorCfg);
  

private:
    static bool initComplete;
    static SemaphoreHandle_t mutex_;
    static StaticJsonDocument<300> jsonDoc;
};

