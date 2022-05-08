#include <AceButton.h>

#if (defined(PIN_SDA) && (PIN_SDA >= 0 ) && defined(PIN_SCL) && (PIN_SCL >= 0 ))
#include <Arduino.h>
#include <Wire.h>
#endif
#if (defined(SK_LEDS) && (SK_LEDS > 0))
#include <FastLED.h>
#endif

#if (defined(SK_STRAIN) && (SK_STRAIN > 0))
#include <HX711.h>
#endif

#if (defined(SK_ALS) && (SK_ALS > 0))
#include <Adafruit_VEML7700.h>
#endif

#include "interface_task.h"
#include "util.h"

using namespace ace_button;

#define COUNT_OF(A) (sizeof(A) / sizeof(A[0]))

#if (defined(SK_LEDS) && (SK_LEDS > 0))
CRGB leds[NUM_LEDS];
#endif

#if (defined(SK_STRAIN) && (SK_STRAIN > 0))
HX711 scale;
#endif

#if (defined(SK_ALS) && (SK_ALS > 0))
Adafruit_VEML7700 veml = Adafruit_VEML7700();
#endif



InterfaceTask::InterfaceTask(const uint8_t task_core, MotorTask &motor_task, DisplayTask *display_task) : Task("Interface", 4048, 1, task_core), motor_task_(motor_task), display_task_(display_task)
{
#if (defined(SK_DISPLAY) && (SK_DISPLAY > 0))
    assert(display_task != nullptr);
#endif
}

InterfaceTask::~InterfaceTask() {}

void InterfaceTask::run()
{

    KnobConfig config;

#if (defined(PIN_BUTTON_NEXT) && (PIN_BUTTON_NEXT >= 34))
    pinMode(PIN_BUTTON_NEXT, INPUT);
#else
    pinMode(PIN_BUTTON_NEXT, INPUT_PULLUP);
#endif
    AceButton button_next((uint8_t)PIN_BUTTON_NEXT);
    button_next.getButtonConfig()->setIEventHandler(this);

#if (defined(PIN_BUTTON_NEXT) && (PIN_BUTTON_NEXT > -1))
#if (defined(PIN_BUTTON_NEXT) && (PIN_BUTTON_NEXT >= 34))
    pinMode(PIN_BUTTON_PREV, INPUT);
#else
    pinMode(PIN_BUTTON_PREV, INPUT_PULLUP);
#endif
    AceButton button_prev((uint8_t)PIN_BUTTON_PREV);
    button_prev.getButtonConfig()->setIEventHandler(this);
#endif

#if (defined(SK_LEDS) && (SK_LEDS > 0))
    FastLED.addLeds<SK6812, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
#endif

#if (defined(PIN_SDA) && (PIN_SDA >= 0 ) && defined(PIN_SCL) && (PIN_SCL >= 0 ))
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);
#endif
#if (defined(SK_STRAIN) && (SK_STRAIN > 0))
    scale.begin(38, 2);
#endif

#if (defined(SK_ALS) && (SK_ALS > 0))
    if (veml.begin())
    {
        veml.setGain(VEML7700_GAIN_2);
        veml.setIntegrationTime(VEML7700_IT_400MS);
    }
    else
    {
        Serial.println("ALS sensor not found!");
    }
#endif

    if (spiffsCfg.GetKnobConfig(0, &config))
    {
        motor_task_.setConfig(config);
    }

    // How far button is pressed, in range [0, 1]
    float press_value_unit = 0;

    // Interface loop:
    while (1)
    {
        button_next.check();
#if (defined(PIN_BUTTON_NEXT) && (PIN_BUTTON_NEXT > -1))
        button_prev.check();
#endif
        if (Serial.available())
        {
            int v = Serial.read();
            if (v == ' ')
            {
                changeConfig(true);
            }
            else if(v == 'l')
            {
                 KnobConfig config;

  
                if (spiffsCfg.GetKnobConfig(spiffsCfg.LastCfgIdx,&config))
                {
                    Serial.print(F("Changing config to "));
                    Serial.println(config.descriptor);
                    motor_task_.setConfig(config);
             
                }
                else
                {
                    Serial.print(F("Unable to get the last config "));
                }
    
            }
            else if(v == 'f')
            {
                 KnobConfig config;

  
                if (spiffsCfg.GetKnobConfig(0,&config))
                {
                    Serial.print(F("Changing config to "));
                    Serial.println(config.descriptor);
                    motor_task_.setConfig(config);
           
                }
                else
                {
                    Serial.print(F("Unable to get the first config "));
                }
    
            }
        }

#if (defined(SK_ALS) && (SK_ALS > 0))
        const float LUX_ALPHA = 0.005;
        static float lux_avg;
        float lux = veml.readLux();
        lux_avg = lux * LUX_ALPHA + lux_avg * (1 - LUX_ALPHA);
        static uint32_t last_als;
        if (millis() - last_als > 1000)
        {
            Serial.print("millilux: ");
            Serial.println(lux * 1000);
            last_als = millis();
        }
#endif

#if (defined(SK_STRAIN) && (SK_STRAIN > 0))
        // TODO: calibrate and track (long term moving average) zero point (lower); allow calibration of set point offset
        const int32_t lower = 950000;
        const int32_t upper = 1800000;
        if (scale.wait_ready_timeout(100))
        {
            int32_t reading = scale.read();

            // Ignore readings that are way out of expected bounds
            if (reading >= lower - (upper - lower) && reading < upper + (upper - lower) * 2)
            {
                static uint32_t last_reading_display;
                if (millis() - last_reading_display > 1000)
                {
                    Serial.print("HX711 reading: ");
                    Serial.println(reading);
                    last_reading_display = millis();
                }
                long value = CLAMP(reading, lower, upper);
                press_value_unit = 1. * (value - lower) / (upper - lower);

                static bool pressed;
                if (!pressed && press_value_unit > 0.75)
                {
                    motor_task_.playHaptic(true);
                    pressed = true;
                    changeConfig(true);
                }
                else if (pressed && press_value_unit < 0.25)
                {
                    motor_task_.playHaptic(false);
                    pressed = false;
                }
            }
        }
        else
        {
            Serial.println("HX711 not found.");

#if (defined(SK_LEDS) && (SK_LEDS > 0))
            for (uint8_t i = 0; i < NUM_LEDS; i++)
            {
                leds[i] = CRGB::Red;
            }
            FastLED.show();
#endif
        }
#endif

        uint16_t brightness = UINT16_MAX;
// TODO: brightness scale factor should be configurable (depends on reflectivity of surface)
#if (defined(SK_ALS) && (SK_ALS > 0))
        brightness = (uint16_t)CLAMP(lux_avg * 13000, (float)1280, (float)UINT16_MAX);
#endif

#if (defined(SK_DISPLAY) && (SK_DISPLAY > 0))
        display_task_->setBrightness(brightness); // TODO: apply gamma correction
#endif

#if (defined(SK_LEDS) && (SK_LEDS > 0))
        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            leds[i].setHSV(200 * press_value_unit, 255, brightness >> 8);

            // Gamma adjustment
            leds[i].r = dim8_video(leds[i].r);
            leds[i].g = dim8_video(leds[i].g);
            leds[i].b = dim8_video(leds[i].b);
        }
        FastLED.show();
#endif

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
#if (defined(PIN_BUTTON_NEXT) && (PIN_BUTTON_NEXT > -1))
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

    KnobConfig config;

    if (next)
    {
        if (spiffsCfg.GetNextKnobConfig(&current_config_, &config))
        {
            Serial.print(F("Changing config to "));
            Serial.print(current_config_);
            Serial.print(" -- ");
            Serial.println(config.descriptor);
            motor_task_.setConfig(config);
        }
        else
        {
            Serial.print(F("Unable to get the next config "));
        }
    }
    else
    {
        if (spiffsCfg.GetPreviousKnobConfig(&current_config_, &config))
        {
            Serial.print(F("Changing config to "));
            Serial.print(current_config_);
            Serial.print(" -- ");
            Serial.println(config.descriptor);
            motor_task_.setConfig(config);
        }
        else
        {
            Serial.print(F("Unable to get the previous config "));
        }
    }
}
