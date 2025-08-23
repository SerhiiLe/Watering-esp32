#ifndef slave_h
#define slave_h

bool registration_dev();
void resolver_loop();
String urlEncode(const char *str);
String urlEncode(String str);
bool tb_send_msg(const char *msg);
bool tb_send_msg(String msg);

#endif