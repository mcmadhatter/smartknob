/**
 * @file spiffs_cfg.cpp
 * @brief Provides a way to read/write configuration items JSON files that 
 *        are located on the ESP32 SPIFFS file system.
 * @version 0.1
 * @date 2022-05-08
 * 
 */
#include "spiffs_cfg.h"

SemaphoreHandle_t SpiffsCfg::mutex_;
bool SpiffsCfg::initComplete = false;

/**
 * @brief Construct a new Spiffs Cfg:: Spiffs Cfg object
 *        If this is the first time the function has been
 *        called, then the semaphore will be created.
 *
 */
SpiffsCfg::SpiffsCfg()
{
    /* The initialisation of the mutex and SPIFFS should only be done once. */
    if (!initComplete)
    {
        initComplete = true;
        mutex_ = xSemaphoreCreateMutex();
        assert(mutex_ != NULL);
        delay(300);
    }
}

/**
 * @brief Destroy the Spiffs Cfg:: Spiffs Cfg object
 *
 */
SpiffsCfg::~SpiffsCfg()
{
    SPIFFS.end();
    vSemaphoreDelete(mutex_);
}

/**
 * @brief Provides a way to get a knob configuration from the list of
 *        all avialable knob configurations. There can be between 1
 *        and N knob configuraion entires. To minimise memory usage,
 *        and maximise compatibility, we will deserialize each one
 *        one at a time. This takes 258 byes, so making the knobJSONDoc
 *        object 300 bytes gives some headroom for safety. This also
 *        allows for the Knobs.jsn config to be changed by another
 *        process during run time, e.g. to add or remove a config.
 *
 * @param cfgIdx the index to get the configuration for
 * @param knobCfg a pointer to a KnobConfig to update with the specified
 *        configuration.
 * @return true if there was an entry at that index, knobCfg will be updated
 *         with the requested configuration.
 * @return false on failure, or if there was no entry at that index.
 */
bool SpiffsCfg::GetKnobConfig(const int cfgIdx, KnobConfig *knobCfg)
{
    bool returnVal = false;

    if (knobCfg == nullptr)
    {
        Serial.println(F("NULL knobCfg pointer was supplied."));
    }
    else
    {
        File file = SPIFFS.open("/Knob.jsn", "r");

        if (!file)
        {
            Serial.println(F("Failed to open knob config file"));
        }
        else
        {
            int idx = 0U;
            bool contProcessing = true;
            SemaphoreGuard lock(mutex_);

            /* Although the Knob.jsn only conatins the arry of knob configrations,
               mke sure we start the json deserializing at the correct place. */
            file.find("[");
            do
            {
                /* There can be between 1 and N knob configuraion entires. To minimise
                   memory usage, and maximise compatibility, we will deserialize each one
                   one at a time. This takes 258 byes, so making the buffer 300 bytes allows
                   some overhead. This also gives the flexibility to change the number of
                   knob configurations, and to support an unlimited number of knob configurations. */
                StaticJsonDocument<300> knobJSONDoc;
                DeserializationError error = deserializeJson(knobJSONDoc, file);
                if (!error)
                {
                    /* Only copy if we are interested in the last index, or if the 
                       indexes match. The extra overhead for copying the configuration 
                       for every entry (when looking for the last entry) is less than
                       having to deserialize the entire array twice. */
                    if ((idx == cfgIdx) || (cfgIdx == LastCfgIdx))
                    {
                        knobCfg->num_positions = knobJSONDoc["num_positions"];
                        knobCfg->position = knobJSONDoc["position"];
                        knobCfg->position_width_radians = knobJSONDoc["position_width_radians"];
                        knobCfg->detent_strength_unit = knobJSONDoc["detent_strength_unit"];
                        knobCfg->endstop_strength_unit = knobJSONDoc["endstop_strength_unit"];
                        knobCfg->snap_point = knobJSONDoc["snap_point"];
                        strlcpy(knobCfg->descriptor, knobJSONDoc["descriptor"] | "unkown", 50);

                        returnVal = true;

                        if (idx == cfgIdx)
                        {
                            contProcessing = false;
                        }
                    }

                    else
                    {
                        idx++;
                    }
                }
                else
                {

                    Serial.print(F("deserializeJson() returned "));
                    Serial.println(error.f_str());
                    contProcessing = false;
                }
            } while ((contProcessing) && (file.findUntil(",", "]")));
            file.close();
        }
    }
    return returnVal;
}

/**
 * @brief Provides a way to get the next knob configuration from the list of
 *        all avialable knob configurations. This will wrap around, so the
 *        next index from the last index, will always wrap around to be the
 *        first index.
 *
 * @param cfgIdx a pointer to the current index, this will be updated with
 *        the next index.
 * @param knobCfg a pointer to a KnobConfig to update with the specified
 *        configuration.
 * @return true on success, cfgIdx and knobCfg will be updated
 * @return false on failure
 */
bool SpiffsCfg::GetNextKnobConfig(int *cfgIdx, KnobConfig *knobCfg)
{
    bool returnValue = false;
    if ((cfgIdx == nullptr) || (knobCfg == nullptr))
    {
        /* do nothing, invalid input pointers */
        Serial.println(F("null pointers on get config"));
    }
    /* try and get the next config. */
    else if (GetKnobConfig(*cfgIdx + 1, knobCfg))
    {
        returnValue = true;
        *cfgIdx = *cfgIdx + 1;
    }
    /* if unable to ge the next config, try to
       get the first config. */
    else if (GetKnobConfig(0, knobCfg))
    {
        returnValue = true;
        *cfgIdx = 0;
    }
    else
    {
        /* do nothing, unable to get config. */
        Serial.println(F("Unable to get knob config"));
    }
    return returnValue;
}

/**
 * @brief Provides a way to get the previous knob configuration from the list of
 *        all avialable knob configurations. This will wrap around to the end
 *        of the list of knob configurations if you try to get the previous
 *        entry from index 0.
 *
 * @param cfgIdx a pointer to the current index, this will be updated with
 *        the next index.
 * @param knobCfg a pointer to a KnobConfig to update with the specified
 *        configuration.
 * @return true on success, cfgIdx and knobCfg will be updated
 * @return false on failure
 */
bool SpiffsCfg::GetPreviousKnobConfig(int *cfgIdx, KnobConfig *knobCfg)
{
    bool returnValue = false;
    if ((cfgIdx == nullptr) || (knobCfg == nullptr))
    {
        /* do nothing, invalid input pointers */
    }
    else if (cfgIdx > 0)
    {
        *cfgIdx = *cfgIdx - 1;
        returnValue = GetKnobConfig(*cfgIdx, knobCfg);
    }
    else
    {
        returnValue = GetKnobConfig(LastCfgIdx, knobCfg);
    }
    return returnValue;
}

/**
 * @brief Removes the knob configuration entry at the specfied index.
 *
 *        NOT YET SUPPORTED
 *
 * @param cfgIdx the index in the list of knob configurations to remove
 * @return true on success
 * @return false on failure
 */
bool SpiffsCfg::DeleteKnobConfig(const int cfgIdx) { return false; }

/**
 * @brief Alows a new knob configuration entry to be added to the end of the knob configuration table
 *
 *        NOT YET SUPPORTED
 *
 * @param knobCfg - the configuration to append
 * @return true on success
 * @return false on failure
 */
bool SpiffsCfg::AppendKnobConfig(KnobConfig knobCfg) { return false; }