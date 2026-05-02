#ifndef GSM_H
#define GSM_H

bool sendMessage(const char* txt);
bool sendMessage(const String& txt);
void messagePool();
void setup2();
void setup_telegram();

String switchActiveChannel(uint8_t ch);
String shared_menu(const String &text);
String print_pumps_status();
String print_decoded_time(int t);

#endif