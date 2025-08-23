/*
	Отображение разных реакций через мигание светодиодов
*/
#ifndef BLINK_MINIM_H
#define BLINK_MINIM_H

#ifndef ON
#define ON 1
#define OFF 0
#endif

#include <Arduino.h>

class blinkMinim {
	public:

	// pin - номер ножки PIO, mode - включение светодиода по низкому LOW или высокому HIGH уровню
	blinkMinim(uint8_t pin=255, uint8_t level=HIGH) {
		if( pin != 255 ) begin(pin, level);
	}

	// дублирует конструктор, для случаев, если он был вызван без аргументов
	void begin(uint8_t pin, uint8_t level=LOW) {
		_pin = pin;
		_level = level;
		pinMode(_pin, OUTPUT);
		digitalWrite(_pin, off()); // после старта погашен
		clean();
	}

	void clean() {
		_mode = 0;
	}

	void tick() {
		unsigned long now = millis();
		uint8_t state = digitalRead(_pin);
		if( _mode > 0 && _old + (state != off() ? _duration: _interval) < now ) {
			// сработало событие
			_old = now;
			state = !state;
			digitalWrite(_pin, state);
			// если это был отсчёт по количеству, то уменьшить счётчик
			if( _cnt>0 && state == off() )
				// если счётчик дошел до нуля, то отключить моргание
				if( --_cnt == 0 ) {
					_mode = 0;
					// вызов callback функции после завершения отсчёта
					if( _userFunc ) _userFunc();
				}
		}
	}

	void blink(uint8_t mode=1, uint16_t interval=500, uint16_t cnt=0, uint16_t duration=0, void (*userFunc)(void)=nullptr) {
		_mode = mode > 0 ? 1: 0;
		_cnt = cnt;
		_interval = interval;
		_duration = duration > 0 ? duration: _interval;
		_old = millis();
		if(mode == 0) digitalWrite(_pin, off());
		_userFunc = userFunc;
	}

	void set(uint8_t mode=1) {
		digitalWrite(_pin, mode ? !off(): off()); 
	}

	void invert() {
		digitalWrite(_pin, !digitalRead(_pin)); 
	}

	private:

	// inline uint8_t on() {
	// 	return _level ? 1: 0;
	// }

	inline uint8_t off() {
		return _level ? 0: 1;
	}

	uint8_t _pin; // pin к которому подключен светодиод
	uint8_t _level; // уровень включенного светодиода HIGH / LOW
	uint8_t _mode = 0; // 0 off, 1 blink
	uint16_t _cnt = 0; // количество включений
	uint16_t _interval = 0; // интервал между включениями
	uint16_t _duration = 0; // продолжительность включения (0 - как interval)
	unsigned long _old = 0; // время последнего переключения
	void (*_userFunc)(void) = nullptr; // callback функция, которую нужно вызвать после окончания мигания

};

#endif