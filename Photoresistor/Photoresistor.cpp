#include "Photoresistor.h"

Photoresistor::Photoresistor(PinName pin) : _pin(pin) {}

// Lux = [((Vref * luxRel) * Vout) - luxRel] / Rl
float Photoresistor::calculateLux() {
    // ADCres = 65535 using read_u16()
    // ADCres = 1.0 using read()
    float ADCres = 1.0f;
    float vout = _pin.read();

    if (vout <= 0.001f) {
        return 0.4f;
    }

    float vCalc = ((_vref * _luxRel) * vout) - _luxRel;
    float lux = vCalc / _rl;

    float scaledLux = lux * _conversion;

    if (scaledLux < 0) {
        scaledLux = 0.4f;
    } else if (scaledLux > 355.0f) {
        scaledLux = 355.0f;
    }

    return scaledLux;
}
