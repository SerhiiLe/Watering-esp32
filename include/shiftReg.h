#ifndef shiftReg_h
#define shiftReg_h

void sR_init();
void sR_go();

void sR_write(uint8_t pin, uint8_t state);
int sR_read(uint8_t pin);
void sR_set(uint8_t pin, uint8_t state);

#ifdef PIN_OE
void sR_disable();
void sR_enable();
#endif

#endif