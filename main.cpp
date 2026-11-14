#include "Grove_LCD_RGB_Backlight.h"
#include "Photoresistor.h"
#include "Thermistor.h"
#include "mbed.h"
// #include "rainbow_bg.h"

Photoresistor photoresistor(A0);
Thermistor thermistor(A3);
Grove_LCD_RGB_Backlight lcd(D14, D15); // LCD display 2 rows and 16 columns (I2C)
AnalogIn rotarySensor(A1);
InterruptIn button(D2);
DigitalOut led(D4);

enum class AppState { NORMAL, SET_HIGH, SET_LOW, WARNING, ALARM, ACKNOWLEDGE};
volatile AppState currentState = AppState::NORMAL; // (volatile) States handled by ISR > read from RAM
AppState prevState = AppState::NORMAL;

float lowerThreshold = 10.0f; // Default lower threshold for temperature
float upperThreshold = 20.0f; // Default upper threshold for temperature
bool ledState = false;        // State of the LED (on/off)

// Helper to convert state enum to a short string
const char* getStateName(AppState state) {
    switch (state) {
        case AppState::NORMAL:   return "NORM";
        case AppState::WARNING:   return "WARN";
        case AppState::ACKNOWLEDGE:   return "ACK";
        case AppState::ALARM:   return "ALARM";
        case AppState::SET_HIGH: return "SET HI";
        case AppState::SET_LOW:  return "SET LO";
    }
    return "UNK";
}

// LCD PRINT - UNUSED, LCD not working :(
void updateLCD(AppState state, float luxPct, bool ledOn, float lowThresh, float highThresh) {
    char line1[17];
    char line2[17];

    // Row 0: Mode, Light %, and LED status
    snprintf(line1, sizeof(line1), "%-6s L:%.0f%% %s", getStateName(state), luxPct, ledOn ? "ON " : "OFF");
    
    // Row 1: Thresholds
    snprintf(line2, sizeof(line2), "Lo:%.0f%% Hi:%.0f%%", lowThresh, highThresh);

    lcd.clear();
    lcd.locate(0, 0);
    lcd.print(line1);
    lcd.locate(0, 1);
    lcd.print(line2);
}

// Find the i2C pins
// ic2 has to be public in Grove_LCD_RGB_Backlight.h to test
// void i2c_bus_scan() {
//     printf("--- Starting I2C Bus Scan ---\n");
//     for (int addr = 0; addr < 256; addr += 2) {
//         char data = 0;
//         if (lcd.i2c.write(addr, &data, 1) == 0) { // Or use i2c.write(addr, &data, 1)
//             printf("Found device at 8-bit addr: 0x%02X (7-bit: 0x%02X)\n", addr, addr >> 1);
//         }
//     }
//     printf("--- Finish I2C Bus Scan ---\n");
// }

void ISR_Button() {
    switch (currentState) {
        case AppState::NORMAL:
        case AppState::WARNING:
        case AppState::ACKNOWLEDGE:
            prevState = currentState;
            currentState = AppState::SET_HIGH;
            break;

        case AppState::SET_HIGH:
            currentState = AppState::SET_LOW;
            break;

        case AppState::SET_LOW:
            currentState = prevState;
            break;

        case AppState::ALARM:
            currentState = AppState::ACKNOWLEDGE;
            break;
    }
}

int main() {
    //i2c_bus_scan();
    button.mode(PullUp);
    button.fall(&ISR_Button);

    float rotaryVal = 0.0f;

    lcd.clear();
    lcd.setRGB(255, 255, 255);

    while (true) {
        float currentLux = photoresistor.calculateLux();
        float currentTemp = thermistor.read_celsius();

        // Convert lux to % scale
        float luxPercentage = (currentLux / 355.0f) * 100.0f;
        float rotaryVal = rotarySensor.read();
        
        switch (currentState) {
            case AppState::NORMAL:
                if (currentTemp > upperThreshold || currentTemp < lowerThreshold) {
                    if (currentTemp > upperThreshold + 3.0f || currentTemp < lowerThreshold - 3.0f) {
                        currentState = AppState::ALARM;
                    } else {
                        currentState = AppState::WARNING;
                    }
                }
                break;
            case AppState::WARNING:
                if (currentTemp > upperThreshold + 3.0f || currentTemp < lowerThreshold - 3.0f) {
                    currentState = AppState::ALARM;
                }
                if (currentTemp <= upperThreshold && currentTemp >= lowerThreshold) {
                    currentState = AppState::NORMAL;
                }
                break;
            
            case AppState::ALARM:
                break;

            case AppState::ACKNOWLEDGE:
                if (currentTemp <= upperThreshold && currentTemp >= lowerThreshold) {
                    currentState = AppState::NORMAL;
                }

                break;

            case AppState::SET_HIGH:
                if (rotaryVal >= 0.7f && upperThreshold < 100.0f) {
                    upperThreshold += 1.0f;
                } else if (rotaryVal <= 0.3 && upperThreshold > (lowerThreshold + 1.0f)) {
                    upperThreshold -= 1.0f;
                }

                break;

            case AppState::SET_LOW:
                if (rotaryVal >= 0.7f && lowerThreshold < (upperThreshold - 1.0f)) {
                    lowerThreshold += 1.0f;
                } else if (rotaryVal <= 0.3 && lowerThreshold > 0.0f) {
                    lowerThreshold -= 1.0f;
                }
                
                break;
        }

        printf("State: %s | Temp: %.1f | Lux: %.1f | LED: %s | Low: %.1f | High: %.1f | RotVal: %.1f \n",
               getStateName(currentState), currentTemp, currentLux, ledState ? "ON" : "OFF", lowerThreshold, upperThreshold, rotaryVal);

        // updateLCD(currentState, luxPercentage, ledState, lowerThreshold, upperThreshold);

        ThisThread::sleep_for(200ms); // Avoid 100% CPU Usage
    }
}