#ifndef ntp_h
#define ntp_h

void syncTime();
time_t getTimeU();
tm getTime(time_t *t = nullptr);
const char* getUptime(char *str);
const char* getTimeDate(char *str);
const char* getTimeDateU(char *str, time_t ut);

#endif
