#ifndef GSM_H
#define GSM_H

void gsm_begin();
bool gsm_init();
bool gsm_check();
void gsm_sendSMS(const String txt);
void gsm_pool();

#endif