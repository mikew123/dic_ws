#ifndef LED_STUFF_H
#define LED_STUFF_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

class ledStuff {
private:
    uint8_t ledPin;
    TaskHandle_t blinkTaskHandle = nullptr;
    
    // Struct to hold blink parameters
    struct BlinkParams {
        uint8_t ledPin;
        uint8_t highBrightness;
        uint16_t highTime;
        uint8_t lowBrightness;
        uint16_t lowTime;
        int numBlinks;
        bool isDoubleBlink = false;
        uint8_t secondHighBrightness;
        uint16_t secondHighTime;
        uint8_t secondLowBrightness;
        uint16_t secondLowTime;
    };
    
    static void blinkTask(void* parameter);
    static void doubleBlinkTask(void* parameter);

public:
    // Constructor
    ledStuff(uint8_t pin = LED_BUILTIN);
    
    // Simple function to set LED intensity (0-255)
    void setIntensity(uint8_t intensity);
    
    // Single blink pattern: alternates between high and low brightness
    // Ends with LED at low intensity
    void ledBlink(int numBlinks, 
                  uint8_t highBrightness, 
                  uint16_t highTime,
                  uint8_t lowBrightness, 
                  uint16_t lowTime);
    
    // Double blink pattern: H, L, H, L each with separate times and intensities
    // If all times are 0, finishes with LED at low intensity
    void doubleBlinkPattern(int numCycles,
                            uint8_t firstHighBrightness,
                            uint16_t firstHighTime,
                            uint8_t firstLowBrightness,
                            uint16_t firstLowTime,
                            uint8_t secondHighBrightness,
                            uint16_t secondHighTime,
                            uint8_t secondLowBrightness,
                            uint16_t secondLowTime);
    
    // Stop any ongoing blink and set to intensity
    void stopBlink(uint8_t finalIntensity);
};

#endif // LED_STUFF_H
