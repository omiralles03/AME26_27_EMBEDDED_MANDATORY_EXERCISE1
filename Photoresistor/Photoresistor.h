#ifndef PHOTORESISTOR_H
#define PHOTORESISTOR_H

#include "mbed.h"

class Photoresistor {
    public:
        Photoresistor(PinName pin);

        float calculateLux();

    private:
        AnalogIn _pin;

        const float _rl = 10.0f;
        const float _vref = 3.3f;
        const float _luxRel = 50.0f;


        // Max lux is 3.3V ~ 1.0 => (3.3V * 500 * 1.0 vout) - 500 = 1150
        // 1150/10k = 0.115 max lux cap
        // Scale to reference -> 355/0.115 = 3086.9565
        const float _conversion = 355.0f / ((((_vref * _luxRel) * 1.0) - _luxRel) / _rl);
};

#endif
