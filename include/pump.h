#ifndef POMP_H
#define POMP_H

#include <Arduino.h>
#include "ntp.h"

#ifndef ON
#define ON 1
#define OFF 0
#endif

class PumpWater {

	public:
	
	// конструктор
	PumpWater() {}

	// инициализация объекта
	// pin - номер пина, level - уровень включения
	void begin(uint8_t pin, uint8_t level=HIGH, void (*writeBit)(uint8_t, uint8_t)=*digitalWrite, int (*readBit)(uint8_t)=*digitalRead) {
		_pin = pin;
		_level = level;
		_writeBit = writeBit;
		_readBit = readBit;
		if( _writeBit == digitalWrite )	pinMode(_pin, OUTPUT);
		_writeBit(_pin, off());
	}

	// проверка, когда надо отключить помпу
	void tick() {
		if( _halt ) return;
		unsigned long time = millis();
		if(_overflow) { // попытка защититься от переполнения
			if(time < _time) // ждём переполнения, которое наступает каждые 49 дней
				_overflow = false;
			else
				return;
		}
		if(time >= _next) {
			_writeBit(_pin, off());
			_halt = true;
			_last = getTimeU();
		}
	}

	// запустить помпу на msec миллисекунд
	void run(uint32_t msec=1000) {
		if(msec < 1000) msec = 1000; // не допускать работу менее, чем на 1 секунду. Система очень инертна.
		_time = millis();
		_next = _time + msec;
		_overflow = _time > _next;
		_halt = false;
		_writeBit(_pin, !off());
	}

	void stop() {
		_next = millis()-1;
		_overflow = false;
		tick();
	}

	// проверить, запущена ли помпа сейчас
	bool status() {
		if( _level )
			return _readBit(_pin);
		else
			return ! _readBit(_pin);
	}

	bool active() {
		return !_halt;
	}

	time_t last() {
		return _last;
	}

	private:

	inline uint8_t off() {
		return _level ? 0: 1;
	}

	uint8_t _pin, _level;
	unsigned long _next = 0, _time = 0;
	bool _overflow = false, _halt = true;
	time_t _last;
	void (*_writeBit)(uint8_t pin, uint8_t val);
	int (*_readBit)(uint8_t pin);
};

#endif