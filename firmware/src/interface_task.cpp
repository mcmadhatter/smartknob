#include <AceButton.h>
#include "interface_task.h"

using namespace ace_button;

#define COUNT_OF(A) (sizeof(A) / sizeof(A[0]))
KnobConfig currKnobCfg;


InterfaceTask::InterfaceTask(const uint8_t task_core, MotorTask &motor_task) : Task{"Interface", 2048, 1, task_core}, motor_task_(motor_task)
{
}

InterfaceTask::~InterfaceTask() {}

void InterfaceTask::run()
{
#if PIN_BUTTON_NEXT >= 34
    pinMode(PIN_BUTTON_NEXT, INPUT);
#else
    pinMode(PIN_BUTTON_NEXT, INPUT_PULLUP);
#endif
    AceButton button_next((uint8_t)PIN_BUTTON_NEXT);
    button_next.getButtonConfig()->setIEventHandler(this);

#if PIN_BUTTON_PREV > -1
#if PIN_BUTTON_PREV >= 34
    pinMode(PIN_BUTTON_PREV, INPUT);
#else
    pinMode(PIN_BUTTON_PREV, INPUT_PULLUP);
#endif
    AceButton button_prev((uint8_t)PIN_BUTTON_PREV);
    button_prev.getButtonConfig()->setIEventHandler(this);
#endif

    changeConfig(false);

    while (1)
    {
        button_next.check();
#if PIN_BUTTON_PREV > -1
        button_prev.check();
#endif
        if (Serial.available())
        {
            int v = Serial.read();
            if (v == ' ')
            {
                changeConfig(true);
            }
        }
        delay(10);
    }
}

void InterfaceTask::handleEvent(AceButton *button, uint8_t event_type, uint8_t button_state)
{
    switch (event_type)
    {
    case AceButton::kEventPressed:
        if (button->getPin() == PIN_BUTTON_NEXT)
        {
            changeConfig(true);
        }
#if PIN_BUTTON_PREV > -1
        if (button->getPin() == PIN_BUTTON_PREV)
        {
            changeConfig(false);
        }
#endif
        break;
    case AceButton::kEventReleased:
        break;
    }
}

void InterfaceTask::changeConfig(bool next)
{
    int32_t configCount = 0;
    DynamicJsonDocument doc = DynamicJsonDocument(2048);
    File file = SPIFFS.open(KNOB_CFG_PATH, "r");

    if (!file)
    {
        Serial.println(F("Failed to open config file"));
    }
    else
    {
        // Parse the JSON object in the file
        DeserializationError err = deserializeJson(doc, file);
        if (err)
        {
            Serial.println(F("Failed to deserialize configuration: "));
            Serial.println(err.f_str());
        }
        else
        {
            // Get a reference to the root array
            JsonArray arr = doc.as<JsonArray>();
            configCount = (int32_t)arr.size();

            if (configCount < 1)
            {
                /* nothing to do, not enough configs. */
            }
            else
            {
                if (next)
                {
                    current_config_ = (current_config_ + 1) % configCount;
                }
                else
                {
                    if (current_config_ == 0)
                    {
                        current_config_ = configCount - 1;
                    }
                    else
                    {
                        current_config_--;
                    }
                }

                if (arr[current_config_].is<JsonObject>())
                {
                    JsonObject obj = arr[current_config_];
                    std::string descriptor = obj["descriptor"];
                    strncpy(currKnobCfg.descriptor, descriptor.c_str(), 50);
                    currKnobCfg.descriptor[49] = 0;
                    currKnobCfg.num_positions = static_cast<int32_t>(obj["num_positions"]);
                    currKnobCfg.position = static_cast<int32_t>(obj["position"]);
                    currKnobCfg.position_width_radians = obj["position_width_radians"];
                    currKnobCfg.detent_strength_unit = obj["detent_strength_unit"];
                    currKnobCfg.endstop_strength_unit = obj["endstop_strength_unit"];
                    currKnobCfg.snap_point = obj["snap_point"];
#ifdef DEBUG_KNOB_CFG
                    Serial.print("Using Knob Configuration: ");
                    Serial.println(currKnobCfg.descriptor);
                    Serial.print("detent_strength_unit ");
                    Serial.println(currKnobCfg.detent_strength_unit);
                    Serial.print("endstop_strength_unit ");
                    Serial.println(currKnobCfg.endstop_strength_unit);
                    Serial.print("num_positions ");
                    Serial.println(currKnobCfg.num_positions);
                    Serial.print("position ");
                    Serial.println(currKnobCfg.position);
                    Serial.print("position_width_radians ");
                    Serial.println(currKnobCfg.position_width_radians);
                    Serial.print("snap_point ");
                    Serial.println(currKnobCfg.snap_point);

                    Serial.printf("Changing config to %d:\n%s\n", current_config_, currKnobCfg.descriptor);
#endif
                    motor_task_.setConfig(currKnobCfg);
                }
            }
        }
    }
}