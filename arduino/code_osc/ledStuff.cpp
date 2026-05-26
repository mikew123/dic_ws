#include "ledStuff.h"
#include <Arduino.h>

ledStuff::ledStuff(uint8_t pin) : ledPin(pin) {
    pinMode(ledPin, OUTPUT);
    analogWrite(ledPin, 0);
    blinkTaskHandle = nullptr;
}

void ledStuff::setIntensity(uint8_t intensity) {
    analogWrite(ledPin, intensity);
}

void ledStuff::blinkTask(void* parameter) {
    BlinkParams* params = (BlinkParams*)parameter;
    uint8_t ledPin = params->ledPin;
    
    // If all times are 0, just set to low intensity and finish
    if (params->highTime == 0 && params->lowTime == 0) {
        analogWrite(ledPin, params->lowBrightness);
        delete params;
        while(1) vTaskDelay(pdMS_TO_TICKS(100));
      //  vTaskDelete(NULL);
      //  return;
    }
    
    // Perform blink pattern
    for (int i = 0; i < params->numBlinks; i++) {
        // High brightness phase
        analogWrite(ledPin, params->highBrightness);
        vTaskDelay(pdMS_TO_TICKS(params->highTime));
        
        // Low brightness phase
        analogWrite(ledPin, params->lowBrightness);
        vTaskDelay(pdMS_TO_TICKS(params->lowTime));
    }
    
    // Ensure LED ends at low intensity
    analogWrite(ledPin, params->lowBrightness);
    
    delete params;
    while(1) vTaskDelay(pdMS_TO_TICKS(100));
//    vTaskDelete(NULL);
}

void ledStuff::doubleBlinkTask(void* parameter) {
    BlinkParams* params = (BlinkParams*)parameter;
    uint8_t ledPin = params->ledPin;
    
    // If all times are 0, just set to low intensity and finish
    if (params->highTime == 0 && params->lowTime == 0 && 
        params->secondHighTime == 0 && params->secondLowTime == 0) {
        analogWrite(ledPin, params->lowBrightness);
        delete params;
        while(1) vTaskDelay(pdMS_TO_TICKS(100));
        // vTaskDelete(NULL);
        // return;
    }
    
    // Perform double blink pattern cycles
    for (int cycle = 0; cycle < params->numBlinks; cycle++) {
        // First high
        if (params->highTime > 0) {
            analogWrite(ledPin, params->highBrightness);
            vTaskDelay(pdMS_TO_TICKS(params->highTime));
        }
        
        // First low
        if (params->lowTime > 0) {
            analogWrite(ledPin, params->lowBrightness);
            vTaskDelay(pdMS_TO_TICKS(params->lowTime));
        }
        
        // Second high
        if (params->secondHighTime > 0) {
            analogWrite(ledPin, params->secondHighBrightness);
            vTaskDelay(pdMS_TO_TICKS(params->secondHighTime));
        }
        
        // Second low
        if (params->secondLowTime > 0) {
            analogWrite(ledPin, params->secondLowBrightness);
            vTaskDelay(pdMS_TO_TICKS(params->secondLowTime));
        }
    }
    
    // Ensure LED ends at low intensity
    analogWrite(ledPin, params->secondLowBrightness);
    
    delete params;
    while(1) vTaskDelay(pdMS_TO_TICKS(100));
    //  vTaskDelete(NULL);
}

void ledStuff::ledBlink(int numBlinks, 
                        uint8_t highBrightness, 
                        uint16_t highTime,
                        uint8_t lowBrightness, 
                        uint16_t lowTime) {
    // Stop any existing blink task
    if (blinkTaskHandle != nullptr) {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = nullptr;
    }
    
    // Create parameters struct
    BlinkParams* params = new BlinkParams();
    params->ledPin = ledPin;
    params->highBrightness = highBrightness;
    params->highTime = highTime;
    params->lowBrightness = lowBrightness;
    params->lowTime = lowTime;
    params->numBlinks = numBlinks;
    params->isDoubleBlink = false;
    
    // Create task
    xTaskCreate(blinkTask, 
                "ledBlink", 
                2048, 
                params, 
                2, 
                &blinkTaskHandle);
}

void ledStuff::doubleBlinkPattern(int numCycles,
                                  uint8_t firstHighBrightness,
                                  uint16_t firstHighTime,
                                  uint8_t firstLowBrightness,
                                  uint16_t firstLowTime,
                                  uint8_t secondHighBrightness,
                                  uint16_t secondHighTime,
                                  uint8_t secondLowBrightness,
                                  uint16_t secondLowTime) {
    // Stop any existing blink task
    if (blinkTaskHandle != nullptr) {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = nullptr;
    }
    
    // Create parameters struct
    BlinkParams* params = new BlinkParams();
    params->ledPin = ledPin;
    params->highBrightness = firstHighBrightness;
    params->highTime = firstHighTime;
    params->lowBrightness = firstLowBrightness;
    params->lowTime = firstLowTime;
    params->secondHighBrightness = secondHighBrightness;
    params->secondHighTime = secondHighTime;
    params->secondLowBrightness = secondLowBrightness;
    params->secondLowTime = secondLowTime;
    params->numBlinks = numCycles;
    params->isDoubleBlink = true;
    
    // Create task
    xTaskCreate(doubleBlinkTask, 
                "doubleBlinkPattern", 
                2048, 
                params, 
                2, 
                &blinkTaskHandle);
}

void ledStuff::stopBlink(uint8_t finalIntensity) {
    if (blinkTaskHandle != nullptr) {
        vTaskDelete(blinkTaskHandle);
        blinkTaskHandle = nullptr;
    }
    analogWrite(ledPin, finalIntensity);
}
