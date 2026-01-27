/*
	Работа с микросхемой сдвигового регистра SN74HC595
*/

#include <Arduino.h>
#include "defines.h"
#include "shiftReg.h"

#ifdef USE_SHIFT_REGISTER

uint8_t _sR_reg = 0;
uint8_t _sR_old = 0;

// Инициализация сдвигового регистра
void sR_init() {
	// установка режима OUTPUT для сдвигового регистра
	pinMode(PIN_LATCH, OUTPUT);
	pinMode(PIN_CLOCK, OUTPUT);
	pinMode(PIN_DATA, OUTPUT);
	// обнуление сдвигового регистра
	_sR_old = 0xff;
	_sR_reg = !RELAY_LEVEL;
	sR_go();
}   

// вывести подготовленные данные в регистр
void sR_go() {
	// если комбинация не изменилась, то ничего не делать.
	if(_sR_old == _sR_reg) return;
	// устанавливаем "защелку" на LOW, переводя в режим программирования
	digitalWrite(PIN_LATCH, LOW);
	// передаем последовательно на dataPin
	shiftOut(PIN_DATA, PIN_CLOCK, MSBFIRST, _sR_reg); 
	//"защелкиваем" регистр, тем самым устанавливая значения на выходах
	digitalWrite(PIN_LATCH, HIGH);
	delayMicroseconds(10); // время коммутации входа от 100 до 500 нс + запас на всякий случай...
	_sR_old = _sR_reg;
}

// установить пин в нужное состояние, но не применять
void sR_set(uint8_t pin, uint8_t state) {
	bitWrite(_sR_reg, pin, state & 1);
}

// установить пин в нужное состояние и сразу применить
void sR_write(uint8_t pin, uint8_t state) {
	sR_set(pin, state);
	sR_go();
}

// чтение состояния пина
int sR_read(uint8_t pin) {
	return bitRead(_sR_old, pin);
}

#else

void sR_init() {}
void sR_go() {}
void sR_set(uint8_t pin, uint8_t state) {}
void sR_write(uint8_t pin, uint8_t state) {}
int sR_read(uint8_t pin)  { return 0; }

#endif