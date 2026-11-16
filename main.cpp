#include "Grove_LCD_RGB_Backlight.h"
#include "Photoresistor.h"
#include "Thermistor.h"
#include "mbed.h"
// #include "rainbow_bg.h"

Photoresistor photoresistor(A0);
Thermistor thermistor(A3);
Grove_LCD_RGB_Backlight lcd(D14, D15); // LCD display 2 rows and 16 columns (I2C)
AnalogIn rotarySensor(A1);
InterruptIn button(D5);
PwmOut led(D3);
PwmOut buzzer(D6);
Ticker buzzerTicker;

enum class AppState { NORMAL, SET_HIGH, SET_LOW, WARNING, ALARM, ACKNOWLEDGE};
volatile AppState currentState = AppState::NORMAL; // (volatile) States handled by ISR > read from RAM
AppState prevState = AppState::NORMAL;
AppState buzzerState = AppState::NORMAL;

float lowerThreshold = 10.0f; // Default lower threshold for temperature
float upperThreshold = 25.0f; // Default upper threshold for temperature
const float HYSTERESIS = 0.5f;

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

void updateLCD(AppState state, float tempC, float luxPct, float lowThresh, float highThresh) {
    char buf[17];
    char line1[17];
    char line2[17];

    // Row 0: Mode, Temperature (C), and Light %
    snprintf(buf, sizeof(buf), "%-5s %4.1fC %3.0f%%", getStateName(state), tempC, luxPct);
    snprintf(line1, sizeof(line1), "%-16s", buf);
    
    // Row 1: Temperature thresholds
    snprintf(buf, sizeof(buf), "Lo:%.0fC Hi:%.0fC", lowThresh, highThresh);
    snprintf(line2, sizeof(line2), "%-16s", buf);

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

volatile bool buzzerOn = false;
void toggleBuzzer() {
    buzzerOn = !buzzerOn;
    buzzer.write(buzzerOn ? 0.5f : 0.0f);
}

struct SensorReads {
    float lux;
    float tmp;
};

SensorReads average_read() {
    float sumLux = 0.0f;
    float sumTmp = 0.0f;

    for (int i = 0; i < 10; i++) {
        sumLux += photoresistor.calculateLux();
        sumTmp += thermistor.read_celsius();
        ThisThread::sleep_for(2ms);
    }

    return {
        sumLux / 10.0f,
        sumTmp / 10.0f,
    };
}

int main() {
    //i2c_bus_scan();
    button.mode(PullUp);
    button.fall(&ISR_Button);

    led.period_ms(10);

    buzzer.period_ms(10);
    buzzer.write(0.0f);

    float rotaryVal = 0.0f;

    lcd.init();
    lcd.clear();
    lcd.setRGB(255, 255, 255);

    while (true) {
        SensorReads reads = average_read();
        float currentLux = reads.lux;
        float currentTemp = reads.tmp;

        // Convert lux to % scale
        // At 3.3V => maxLux ~= 155
        float luxPercentage = (currentLux / 155.0f) * 100.0f;
        // Clamp values
        if (luxPercentage > 100.0f) { 
            luxPercentage = 100.0f; 
        }
        if (luxPercentage < 0.0f) {
            luxPercentage = 0.0f;
        }

        float dutyCycle = (100.0f - luxPercentage) / 100.0f;
        led.write(dutyCycle);

        rotaryVal = rotarySensor.read();
        
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
                if (currentTemp <= (upperThreshold - HYSTERESIS) && currentTemp >= (lowerThreshold + HYSTERESIS)) {
                    currentState = AppState::NORMAL;
                }
                break;
            
            case AppState::ALARM:
                break;

            case AppState::ACKNOWLEDGE:
                if (currentTemp <= (upperThreshold - HYSTERESIS) && currentTemp >= (lowerThreshold + HYSTERESIS)) {
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

        if (currentState != buzzerState) {
            buzzerState = currentState;

            switch (currentState) {
                case AppState::WARNING:
                    buzzerTicker.attach(&toggleBuzzer, 1000ms);
                    break;
                case AppState::ALARM:
                    buzzerTicker.attach(&toggleBuzzer, 200ms);
                    break;
                default:
                    buzzerTicker.detach();
                    buzzer.write(0.0f);
                    buzzerOn = false;
            }
        }

        printf("State: %s | Temp: %.1f | Lux: %.1f | LED PWM: %.1f | Buzz: %s | Low: %.1f | High: %.1f | RotVal: %.1f \n",
               getStateName(currentState),
               currentTemp,
               currentLux,
               dutyCycle * 100.0f,
               buzzerOn ? "ON" : "OFF",
               lowerThreshold,
               upperThreshold,
               rotaryVal);

        updateLCD(currentState, currentTemp, luxPercentage, lowerThreshold, upperThreshold);

        ThisThread::sleep_for(200ms); // Avoid 100% CPU Usage
    }
}