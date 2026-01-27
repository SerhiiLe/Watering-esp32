/*
    Опрос датчика температуры / влажности
*/

#include <Arduino.h>
#include "defines.h"
#include "temperature.h"

#ifdef USE_AHTx0

#include <Wire.h>
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;
bool fl_AHTIsInit = false;

bool temperature_init() {
    if( Wire.begin(PIN_SDA, PIN_SCL) )
		if( aht.begin() ) {
			fl_AHTIsInit = true;
			LOG(println, PSTR("AHTX0 found"));
            return true;
		}
    return false;
}

void getTemperature(float &temperature, float &humidity) {
	if(!fl_AHTIsInit) return;
    sensors_event_t _humidity, _temperature;
    aht.getEvent(&_humidity, &_temperature);
    temperature = _temperature.temperature;
    humidity = _humidity.relative_humidity;
}

#else

bool temperature_init() { return false; }
void getTemperature(float &temperature, float &humidity) {}

#endif
