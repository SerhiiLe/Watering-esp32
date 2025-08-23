/*
	встроенный web сервер для настройки часов
	(для начальной настройки ip и wifi используется wifi_init)
*/

#include <Arduino.h>
#include <WebServer.h>
#include "mHTTPUpdateServer.h"
#include <ESPmDNS.h>
#include <rom/rtc.h>
#include <esp_system.h>
#include <LittleFS.h>
#include <time.h>
#include "defines.h"
#include "web.h"
#include "settings.h"
#include "ntp.h"
#include "wifi_init.h"
#include "slave.h"
#include "blinkMinim.h"
#include "pump.h"

#define HPP(txt, ...) HTTP.client().printf(txt, __VA_ARGS__)

WebServer HTTP(80);
HTTPUpdateServer httpUpdater;
bool web_isStarted = false;

void save_settings();
void sysinfo();
void moisture();
void save_pump();
void save_moisture();
void pump_on();
void full_status();
void scheduler_save();
void scheduler_off();
void maintence();
void set_clock();
void onoff();
void logout();
void calibrate();
void api();

bool fileSend(String path);
bool need_save = false;
bool fl_mdns = false;

// отключение веб сервера для активации режима настройки wifi
void web_disable() {
	HTTP.stop();
	web_isStarted = false;
	LOG(println, "HTTP server stoped");

	MDNS.disableWorkstation();
	fl_mdns = false;
	LOG(println, "MDNS responder stoped");
}

// отправка простого текста
void text_send(String s, uint16_t r = 200) {
	HTTP.send(r, "text/plain", s);
}
// отправка сообщение "не найдено"
void not_found() {
	text_send("Not Found", 404);
}

// диспетчер вызовов веб сервера
void web_process() {
	if( web_isStarted ) {
		HTTP.handleClient();
	} else {
		HTTP.begin();
		// Обработка HTTP-запросов
		HTTP.on("/save_settings", save_settings);
		HTTP.on("/sysinfo", sysinfo);
		HTTP.on("/moisture", moisture);
		HTTP.on("/save_moisture", save_moisture);
		HTTP.on("/save_pump", save_pump);
		HTTP.on("/pump_on", pump_on);
		HTTP.on("/full_status", full_status);
		HTTP.on("/scheduler_save", scheduler_save);
		HTTP.on("/scheduler_off", scheduler_off);
		HTTP.on("/clear", maintence);
		HTTP.on("/clock", set_clock);
		HTTP.on("/onoff", onoff);
		HTTP.on("/logout", logout);
		HTTP.on("/api", api);
		HTTP.on("/who", [](){
			text_send(gs.host_name);
		});
		HTTP.onNotFound([](){
			if(!fileSend(HTTP.uri()))
				not_found();
			});
		web_isStarted = true;
  		httpUpdater.setup(&HTTP, gs.web_login, gs.web_password);
		LOG(println, "HTTP server started");

		if(MDNS.begin(gs.host_name)) {
			MDNS.addService("http", "tcp", 80);
			fl_mdns = true;
			LOG(printf, "MDNS responder started. name=%s, ip=%s\n", gs.host_name, WiFi.localIP().toString().c_str());
			registration_dev();
		} else {
			LOG(printf, "Error in start mDNS. name=%s, ip=%s\n", gs.host_name, WiFi.localIP().toString().c_str());
		}
	}
}

// страничка выхода, будет предлагать ввести пароль, пока он не перестанет совпадать с реальным
void logout() {
	if(gs.web_login.length() > 0 && gs.web_password.length() > 0)
		if(HTTP.authenticate(gs.web_login.c_str(), gs.web_password.c_str()))
			HTTP.requestAuthentication(DIGEST_AUTH);
	if(!fileSend("/logged-out.html"))
		not_found();
}

// список файлов, для которых авторизация не нужна, остальные под паролем
bool auth_need(String s) {
	if(s == "/index.html") return false;
	if(s == "/about.html") return false;
	if(s == "/send.html") return false;
	if(s == "/logged-out.html") return false;
	if(s.endsWith(".js")) return false;
	if(s.endsWith(".css")) return false;
	if(s.endsWith(".ico")) return false;
	if(s.endsWith(".png")) return false;
	return true;
}

// авторизация. много комментариев из документации, чтобы по новой не искать
bool is_no_auth() {
	// allows you to set the realm of authentication Default:"Login Required"
	// const char* www_realm = "Custom Auth Realm";
	// the Content of the HTML response in case of Unauthorized Access Default:empty
	// String authFailResponse = "Authentication Failed";
	if(gs.web_login.length() > 0 && gs.web_password.length() > 0 )
		if(!HTTP.authenticate(gs.web_login.c_str(), gs.web_password.c_str())) {
			//Basic Auth Method with Custom realm and Failure Response
			//return server.requestAuthentication(BASIC_AUTH, www_realm, authFailResponse);
			//Digest Auth Method with realm="Login Required" and empty Failure Response
			//return server.requestAuthentication(DIGEST_AUTH);
			//Digest Auth Method with Custom realm and empty Failure Response
			//return server.requestAuthentication(DIGEST_AUTH, www_realm);
			//Digest Auth Method with Custom realm and Failure Response
			HTTP.requestAuthentication(DIGEST_AUTH);
			return true;
		}
	return false;
}

// отправка файла
bool fileSend(String path) {
	// если путь пустой - исправить на индексную страничку
	if( path.endsWith("/") ) path += "index.html";
	// проверка необходимости авторизации
	if(auth_need(path))
		if(is_no_auth()) return false;
	// определение типа файла
	const char *ct = nullptr;
	if(path.endsWith(".html")) ct = "text/html";
	else if(path.endsWith(".css")) ct = "text/css";
	else if(path.endsWith(".js")) ct = "application/javascript";
	else if(path.endsWith(".json")) ct = "application/json";
	else if(path.endsWith(".png")) ct = "image/png";
	else if(path.endsWith(".jpg")) ct = "image/jpeg";
	else if(path.endsWith(".gif")) ct = "image/gif";
	else if(path.endsWith(".ico")) ct = "image/x-icon";
	else ct = "text/plain";
	// открытие файла на чтение
	if(!fs_isStarted) {
		// файловая система не загружена, переход на страничку обновления
		HPP("HTTP/1.1 200\r\nContent-Type: %s\r\nContent-Length: 80\r\nConnection: close\r\n\r\n<html><body><h1><a href='/update'>File system not exist!</a></h1></body></html>", ct);
		return true;
	}
	if(LittleFS.exists(path)) {
		File file = LittleFS.open(path, "r");
		// файл существует и открыт, выделение буфера передачи и отсылка заголовка
		char buf[1476];
		size_t sent = 0;
		int siz = file.size();
		HPP("HTTP/1.1 200\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", ct, siz);
		// отсылка файла порциями, по размеру буфера или остаток
		while(siz > 0) {
			size_t len = std::min((int)(sizeof(buf) - 1), siz);
			file.read((uint8_t *)buf, len);
			HTTP.client().write((const char*)buf, len);
			siz -= len;
			sent+=len;
		}
		file.close();  
	} else return false; // файла нет, ошибка
	return true;
}

// декодирование времени, заданного в поле input->time
uint16_t decode_time(String s) {
	// выделение часов и минут из строки вида 00:00
	size_t pos = s.indexOf(":");
	uint8_t h = constrain(s.toInt(), 0, 23);
	uint8_t m = constrain(s.substring(pos+1).toInt(), 0, 59);
	return h*60 + m;
}

/****** шаблоны простых операций для выделения переменных из web ******/

// определение выбран checkbox или нет
bool set_simple_checkbox(const char * name, uint8_t &var) {
	if( HTTP.hasArg(name) ) {
		if( var == 0 ) {
			var = 1;
			need_save = true;
			return true;
		}
	} else {
		if( var > 0 ) {
			var = 0;
			need_save = true;
			return true;
		}
	}
	return false;
}
// определение простых целых чисел
template <typename T>
bool set_simple_int(const char * name, T &var, long from, long to) {
	if( HTTP.hasArg(name) ) {
		if( HTTP.arg(name).toInt() != (long)var ) {
			var = constrain(HTTP.arg(name).toInt(), from, to);
			need_save = true;
			LOG(printf,"web int: %s change\n",name);
			return true;
		}
	}
	return false;
}
// определение дробных чисел
bool set_simple_float(const char * name, float &var, float from, float to, float prec=8.0f) {
	if( HTTP.hasArg(name) ) {
		if( round(HTTP.arg(name).toFloat()*pow(10.0f,prec)) != round(var*pow(10.0f,prec)) ) {
			var = constrain(HTTP.arg(name).toFloat(), from, to);
			need_save = true;
			LOG(printf,"web float: %s change\n",name);
			return true;
		}
	}
	return false;
}
// определение простых строк
bool set_simple_string(const char * name, String &var) {
	if( HTTP.hasArg(name) ) {
		if( HTTP.arg(name) != var ) {
			var = HTTP.arg(name);
			need_save = true;
			LOG(printf,"web string: %s change\n",name);
			return true;
		}
	}
	return false;
}
// определение времени
bool set_simple_time(const char * name, uint16_t &var) {
	if( HTTP.hasArg(name) ) {
		if( decode_time(HTTP.arg(name)) != var ) {
			var = decode_time(HTTP.arg(name));
			need_save = true;
			return true;
		}
	}
	return false;
}

/****** обработка разных запросов ******/

// сохранение настроек
void save_settings() {
	if(is_no_auth()) return;
	need_save = false;

	if( set_simple_string("host_name", gs.host_name) )
		if(fl_mdns)	MDNS.setInstanceName(gs.host_name);
	set_simple_checkbox("sec_enable", gs.sec_enable);

		bool sync_time = false;
	if( set_simple_int("tz_shift", gs.tz_shift, -12, 12) )
		sync_time = true;
	if( set_simple_checkbox("tz_dst", gs.tz_dst) )
		sync_time = true;
	if( set_simple_int("sync_time_period", gs.sync_time_period, 1, 255) )
		ntpSyncTimer.setInterval(3600000U * gs.sync_time_period);

	set_simple_int("high_v", gs.high_v, 0, 4095);
	set_simple_int("low_v", gs.low_v, 0, 4095);

	// set_simple_int("msec_in_ml", gs.msec_in_ml, 100, 65000);
	set_simple_int("doze", gs.doze, 10, 255);

	bool need_registration = false;
	if( set_simple_string("hub_name", gs.hub_name) )
		need_registration = true;
	if( set_simple_string("hub_pin", gs.hub_pin) )
		need_registration = true;
	if( set_simple_int("hub_period", gs.hub_period, 1, 255) )
		hubRegTimer.setInterval(60000U * gs.hub_period);

	bool fl_setTelegram = false;
	set_simple_string("tb_name", gs.tb_name);
	if( set_simple_string("tb_chats", gs.tb_chats) )
		fl_setTelegram = true;
	if( set_simple_string("tb_token", gs.tb_token) )
		fl_setTelegram = true;
	set_simple_string("tb_secret", gs.tb_secret);
	if( set_simple_int("tb_rate", gs.tb_rate, 0, 3600) )
		telegramTimer.setInterval(1000U * gs.tb_rate);
	set_simple_int("tb_accelerated", gs.tb_accelerated, 1, 600);
	set_simple_int("tb_accelerate", gs.tb_accelerate, 1, 3600);
	set_simple_int("tb_ban", gs.tb_ban, 900, 3600);

	set_simple_checkbox("sms_use", gs.sms_use);
	set_simple_string("sms_phone", gs.sms_phone);

	bool need_web_restart = false;
	if( set_simple_string("web_login", gs.web_login) )
		need_web_restart = true;
	if( set_simple_string("web_password", gs.web_password) )
		need_web_restart = true;

	HTTP.sendHeader("Location","maintenance.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_settings need_save=%i\n", need_save);
	if( need_save ) save_config_main();
	if( sync_time ) syncTime();
	if(need_web_restart) httpUpdater.setup(&HTTP, gs.web_login, gs.web_password);
	if(need_registration) registration_dev();
	// if(fl_setTelegram) setup_telegram();
}

// перезагрузка железки, сброс ком-порта, отключение сети и диска, чтобы ничего не мешало перезагрузке
void reboot_watcher() {
	Serial.flush();
	WiFi.mode(WIFI_OFF);
	WiFi.getSleep(); //disable AP & station by calling "WiFi.mode(WIFI_OFF)" & put modem to sleep
	LittleFS.end();
	delay(1000);
	ESP.restart();
}

void maintence() {
	if(is_no_auth()) return;
	HTTP.sendHeader(F("Location"),"/");
	HTTP.send(303); 
	// initRString(PSTR("Сброс"));
	if( HTTP.hasArg("t") ) {
		if( HTTP.arg("t") == "c" && LittleFS.exists(F("/config.json")) ) {
			LOG(println, PSTR("reset settings"));
			LittleFS.remove(F("/config.json"));
			reboot_watcher();
		}
		if( HTTP.arg("t") == "l" ) {
			LOG(println, PSTR("erase logs"));
			char fileName[32];
			for(int8_t i=0; i<LOG_COUNT; i++) {
				sprintf_P(fileName, LOG_FILE, i);
				if( LittleFS.exists(fileName) ) LittleFS.remove(fileName);
			}
			reboot_watcher();
		}
		if( HTTP.arg("t") == "r" ) {
			reboot_watcher();
		}
	}
}

// Установка времени. Для крайних случаев, когда интернет отсутствует
void set_clock() {
	if(is_no_auth()) return;
	uint8_t type=0;
	String name = "t";
	if(HTTP.hasArg(name)) {
		struct tm tm;
		type = HTTP.arg(name).toInt();
		if(type==0 || type==1) {
			name = F("time");
			if(HTTP.hasArg(name)) {
				size_t pos = HTTP.arg(name).indexOf(":");
				tm.tm_hour = constrain(HTTP.arg(name).toInt(), 0, 23);
				tm.tm_min = constrain(HTTP.arg(name).substring(pos+1).toInt(), 0, 59);
				name = F("date");
				if(HTTP.hasArg(name)) {
					size_t pos = HTTP.arg(name).indexOf("-");
					tm.tm_year = constrain(HTTP.arg(name).toInt()-1900, 0, 65000);
					tm.tm_mon = constrain(HTTP.arg(name).substring(pos+1).toInt()-1, 0, 11);
					size_t pos2 = HTTP.arg(name).substring(pos+1).indexOf("-");
					tm.tm_mday = constrain(HTTP.arg(name).substring(pos+pos2+2).toInt(), 1, 31);
					name = F("sec");
					if(HTTP.hasArg(name)) {
						tm.tm_sec = constrain(HTTP.arg(name).toInt()+1, 0, 60);
						tm.tm_isdst = gs.tz_dst;
						time_t t = mktime(&tm);
						LOG(printf_P,"web time: %llu\n",t);
						// set the system time
						timeval tv = { t, 0 };
						settimeofday(&tv, nullptr);
					}
				}
			}
		} else {
			syncTime();
		}
		HTTP.sendHeader("Location","/maintenance.html");
		HTTP.send(303);
		delay(1);
	} else {
		tm t = getTime();
		HTTP.client().print("HTTP/1.1 200\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{");
		HPP("\"time\":\"%u\",", t.tm_hour*60+t.tm_min);
		HPP("\"date\":\"%u-%02u-%02u\"}", t.tm_year +1900, t.tm_mon +1, t.tm_mday);
		HTTP.client().stop();
	}
}

// Включение/выключение различных режимов
void onoff() {
	if(is_no_auth()) return;
	int8_t a=0;
	bool cond=false;
	String name = "a";
	if(HTTP.hasArg(name)) a = HTTP.arg(name).toInt();
	name = "t";
	if(HTTP.hasArg(name)) {
		if(HTTP.arg(name) == F("ftp")) {
			// включает/выключает ftp сервер, чтобы не кушал ресурсов просто так
			if(a) ftp_isAllow = !ftp_isAllow;
			cond = ftp_isAllow;
		} //else
		// if(HTTP.arg(name) == F("security")) {
		// 	// включает/выключает режим "охраны"
		// 	if(a) sec_enable = !(bool)sec_enable;
		// 		cond = sec_enable;
		// 	if(a) {
		// 		save_log_file(cond?SEC_TEXT_ENABLE:SEC_TEXT_DISABLE);
		// 		save_config_security();
		// 	}
		// }
	}
	text_send(cond?"1":"0");
}


const char* print_full_platform_info(char* buf) {
	int p = 0;
	const char *cpu;
	
	switch (chip_info.model) {
		case 1: // ESP32
			cpu = "ESP32";
			break;
		case 2: // ESP32-S2
			cpu = "ESP32-S2";
			break;
		case 9: // ESP32-S3
			cpu = "ESP32-S4";
			break;
		case 5: // ESP32-C3
			cpu = "ESP32-C3";
			break;
		case 6: // ESP32-H2
			cpu = "ESP32-H2";
			break;
		case 12: // ESP32-C2
			cpu = "ESP32-C2";
			break;
        case 13: // ESP32-C6
			cpu = "ESP32-C6";
			break;
        case 16: // ESP32-H2
			cpu = "ESP32-H2";
			break;
        case 18: // ESP32-P4
			cpu = "ESP32-P4";
			break;
		case 20: // ESP32-C61
			cpu = "ESP32-C61";
			break;
		case 23: // ESP32-C5
			cpu = "ESP32-C5";
			break;
		case 25: // ESP32-H21
			cpu = "ESP32-H21";
			break;
		case 28: // ESP32-H4
			cpu = "ESP32-H4";
			break;
		default:
			cpu = "unknown";
	}
	p = sprintf(buf, "Chip:%s_r%u/", cpu, chip_info.revision);
	p += sprintf(buf+p, "Cores:%u/%s", chip_info.cores, ESP.getSdkVersion());
	return buf;
}

// декодирование информации о причине перезагрузки ядра
const char* print_reset_reason(char *buf) {
	int p = 0;
	uint8_t old_reason = 127;
	const char *res;
	for(int i=0; i<chip_info.cores; i++) {
		uint8_t reason = rtc_get_reset_reason(i);
		if( old_reason != reason ) {
			old_reason = reason;
			if( p ) p += sprintf(buf+p, ", ");
			switch ( reason ) {
				case 1 : res = "PowerON"; break;        	       /**<1, Vbat power on reset*/
				case 3 : res = "SW_RESET"; break;               /**<3, Software reset digital core*/
				case 4 : res = "OWDT_RESET"; break;             /**<4, Legacy watch dog reset digital core*/
				case 5 : res = "DeepSleep"; break;              /**<5, Deep Sleep reset digital core*/
				case 6 : res = "SDIO_RESET"; break;             /**<6, Reset by SLC module, reset digital core*/
				case 7 : res = "TG0WDT_SYS_RESET"; break;       /**<7, Timer Group0 Watch dog reset digital core*/
				case 8 : res = "TG1WDT_SYS_RESET"; break;       /**<8, Timer Group1 Watch dog reset digital core*/
				case 9 : res = "RTCWDT_SYS_RESET"; break;       /**<9, RTC Watch dog Reset digital core*/
				case 10 : res = "INTRUSION_RESET"; break;       /**<10, Intrusion tested to reset CPU*/
				case 11 : res = "TGWDT_CPU_RESET"; break;       /**<11, Time Group reset CPU*/
				case 12 : res = "SW_CPU_RESET"; break;          /**<12, Software reset CPU*/
				case 13 : res = "RTCWDT_CPU_RESET"; break;      /**<13, RTC Watch dog Reset CPU*/
				case 14 : res = "EXT_CPU_RESET"; break;         /**<14, for APP CPU, reseted by PRO CPU*/
				case 15 : res = "RTCWDT_BROWN_OUT"; break;	    /**<15, Reset when the vdd voltage is not stable*/
				case 16 : res = "RTCWDT_RTC_RESET"; break;      /**<16, RTC Watch dog reset digital core and rtc module*/
				default : res = "NO_MEAN";
			}
			p += sprintf(buf+p, "%s", res);
		}
	}
	return buf;
}

// Информация о состоянии железки
void sysinfo() {
	if(is_no_auth()) return;
	char buf[100];
	HTTP.client().print("HTTP/1.1 200\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{");
	HPP("\"Uptime\":\"%s\",", getUptime(buf));
	HPP("\"DateTime\":\"%s\",", getTimeDate(buf));
	HPP("\"fl_5v\":%i,", fl_5v);
	HPP("\"Battery\":%u,", battery);
	HPP("\"Battery_raw\":%u,", battery_raw);
	HPP("\"Rssi\":%i,", wifi_rssi());
	HPP("\"IP\":\"%s\",", wifi_currentIP().c_str());
	HPP("\"gsm_info\":\"%s\",", gsm.info.c_str());
	HPP("\"gsm_rssi\":\"%i\",", map(gsm.rssi, 0, 32, 0, 100));
	HPP("\"gsm_status\":\"%i\",", gsm.isSleep ? -10: gsm.status);
	HPP("\"FreeHeap\":%i,", ESP.getFreeHeap());
	HPP("\"MaxFreeBlockSize\":%i,", ESP.getMaxAllocHeap());
	HPP("\"HeapFragmentation\":%i,", 100-ESP.getMaxAllocHeap()*100/ESP.getFreeHeap());
	HPP("\"ResetReason\":\"%s\",", print_reset_reason(buf));
	HPP("\"FullVersion\":\"%s\",", print_full_platform_info(buf));
	HPP("\"CpuFreqMHz\":%i,", ESP.getCpuFreqMHz());
	HPP("\"BuildTime\":\"%s %s\"}", PSTR(__DATE__), PSTR(__TIME__));
}

// Информация о состоянии железки
void moisture() {
	if(is_no_auth()) return;
	HTTP.client().print("HTTP/1.1 200\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{");
	for(uint8_t i=0; i<SENSORS; i++) {
		HPP("\"raw%u\":%u,", i, moi_raw[i]);
		HPP("\"per%u\":%u%s", i, moi_per[i], i+1<SENSORS ? ",":"}");
	}
}

// сохранение настроек
void save_moisture() {
	if(is_no_auth()) return;
	need_save = false;

	char buf[20];
	for(uint8_t i=0; i<SENSORS; i++) {
		sprintf(buf,"moi0_%u",i);
		set_simple_int(buf, mc[i].moi0, 0, 4095);
		sprintf(buf,"moi100_%u",i);
		set_simple_int(buf, mc[i].moi100, 0, 4095);
	}

	HTTP.sendHeader("Location","moisture.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_moisture need_save=%i\n", need_save);
	if( need_save ) save_moisture_calibration();
}

void save_pump() {
	if(is_no_auth()) return;
	need_save = false;

	set_simple_int("pump", pc.pump, 1, PUMPS);
	set_simple_int("sec1", pc.sec1, 1, 255);
	set_simple_float("tara", pc.tara, 1, 1e6, 2.0f);
	set_simple_float("weight1", pc.weight1, 1, 1e6, 2.0f);
	set_simple_float("weight2", pc.weight2, 1, 1e6, 2.0f);
	set_simple_int("in_ms", pc.in_ms, 1, 65000);
	set_simple_int("empty_ms", pc.empty_ms, 1, 65000);

	HTTP.sendHeader("Location","pump.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_pump need_save=%i\n", need_save);
	if( need_save ) save_pump_calibration();
}

extern PumpWater p[];

void pump_on() {
	if(is_no_auth()) return;
	bool cond=false;

	String name = "pump";
	if( HTTP.hasArg(name) ) {
		uint8_t pump = constrain(HTTP.arg(name).toInt(), 1, PUMPS)-1;
		uint8_t cnt = 1, sec = 0;
		name = "cnt";
		if( HTTP.hasArg(name) ) {
			cnt = constrain(HTTP.arg(name).toInt(), 1, 255);
		}
		name = "sec";
		if( HTTP.hasArg(name) ) {
			sec = constrain(HTTP.arg(name).toInt(), 0, 255);
		}
		// if( ! p[pump].status() ) {
		if( ! p[pump].active() ) {
			pq[pump].active = true;
			pq[pump].need = cnt;
			pq[pump].seconds = sec;
		}
		cond = true;
	}
	text_send(cond?"1":"0");
}

// печать байта в виде шестнадцатеричного числа
void print_byte(char *buf, byte c, size_t& pos) {
	if((c & 0xf) > 9)
		buf[pos+1] = (c & 0xf) - 10 + 'A';
	else
		buf[pos+1] = (c & 0xf) + '0';
	c = (c>>4) & 0xf;
	if(c > 9)
		buf[pos]=c - 10 + 'A';
	else
		buf[pos] = c+'0';
	pos += 2;
}

// кодирование строки для json
const char* jsonEncode(char* buf, const char *str, size_t max_length) {
	// size_t len = strlen(str);
	size_t p = 0, i = 0;
	byte c;

	while( str[i] != '\0' && p < max_length-8) {
		// Выделение символа UTF-8 и перевод его в UTF-16 для вывода в JSON
		// 0xxxxxxx - 7 бит 1 байт, 110xxxxx - 10 бит 2 байта, 1110xxxx - 16 бит 3 байта, 11110xxx - 21 бит 4 байта
		c = (byte)str[i++];
		if( c > 127  ) {
			buf[p++] = '\\';
			buf[p++] = 'u';
			// utf8 -> utf16
			if( c >> 5 == 6 ) {
				uint16_t cc = ((uint16_t)(str[i-1] & 0x1F) << 6);
				cc |= (uint16_t)(str[i++] & 0x3F);
				print_byte(buf, (byte)(cc>>8), p);
				print_byte(buf, (byte)(cc&0xff), p);
			} else if( c >> 4 == 14 ) {
				uint16_t cc = ((uint16_t)(str[i-1] & 0x0F) << 12);
				cc |= ((uint16_t)(str[i++] & 0x3F) << 6);
				cc |= (uint16_t)(str[i++] & 0x3F);
				print_byte(buf, (byte)(cc>>8), p);
				print_byte(buf, (byte)(cc&0xff), p);
			} else if( c >> 3 == 30 ) {
				uint32_t CP = ((uint32_t)(str[i-1] & 0x07) << 18);
				CP |= ((uint32_t)(str[i++] & 0x3F) << 12);
				CP |= ((uint32_t)(str[i++] & 0x3F) << 6);
				CP |= (uint32_t)(str[i++] & 0x3F);
				CP -= 0x10000;
				uint16_t cc = 0xD800 + (uint16_t)((CP >> 10) & 0x3FF);
				print_byte(buf, (byte)(cc>>8), p);
				print_byte(buf, (byte)(cc&0xff), p);
				cc = 0xDC00 + (uint16_t)(CP & 0x3FF);
				print_byte(buf, (byte)(cc>>8), p);
				print_byte(buf, (byte)(cc&0xff), p);
			}
		} else {
			buf[p++] = c;
		}
	}

	buf[p++] = '\0';
	return buf;
}

void full_status() {
	if(is_no_auth()) return;
	char buf[100];
	HTTP.client().print("HTTP/1.1 200\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{");

	HPP("\"hostname\":\"%s\",", jsonEncode(buf, gs.host_name.c_str(), sizeof(buf)));
	HPP("\"is_auth\":%i,", HTTP.authenticate(gs.web_login.c_str(), gs.web_password.c_str()) && gs.web_password.length() > 0 ? 1 : 0);

	HTTP.client().print("\"pumps\":[");
	for(uint8_t i=0; i<PUMPS; i++) {
		HPP("{\"con\":%.2f,", ps[i].vol);
		HPP("\"cnt\":%u,", ps[i].count);
		HPP("\"last\":\"%s\",", getTimeDateU(buf, ps[i].last < p[i].last() ? p[i].last(): ps[i].last));
		HPP("\"pump\":%u}%s", p[i].active(), i+1<PUMPS ? ",":"");
	}
	HTTP.client().print("],\"sensors\":[");
	for(uint8_t i=0; i<SENSORS; i++) {
		HPP("%u%s",moi_per[i], i+1<SENSORS ? ",":"");
	}
	HTTP.client().print("]}");

	HTTP.client().stop();
}

/*
cm биты
1 - выбор сенсора (0-31) 0 - среднее + 31 сенсор
2 - 
4 - 
8 -
16 - 
32 - выбор режима / условия (0-3)
64 - 
128 - включено
*/

void scheduler_save() {
	if(is_no_auth()) return;
	need_save = false;
	uint8_t target = 0;
	String name = "target";
	if( HTTP.hasArg(name) ) {
		target = HTTP.arg(name).toInt();
		name = "time";
		if( HTTP.hasArg(name) ) {
			// выделение часов и минут из строки вида 00:00
			uint16_t time = decode_time(HTTP.arg(name));
			if( scheduler[target].t != time ) {
				scheduler[target].t = time;
				need_save = true;
			}
		}
		name = "repeat";
		if( HTTP.hasArg(name) ) {
			// выделение часов и минут из строки вида 00:00
			uint16_t time = decode_time(HTTP.arg(name));
			if( scheduler[target].r != time ) {
				scheduler[target].r = time;
				need_save = true;
			}
		}
		uint8_t settings = 128;
		name = F("mode");
		if( HTTP.hasArg(name) ) settings |= constrain(HTTP.arg(name).toInt(), 0, 3) << 5;
		name = F("sensor");
		if( HTTP.hasArg(name) ) settings |= constrain(HTTP.arg(name).toInt(), 0, 31); // max 31
		if( settings != scheduler[target].cm ) {
			scheduler[target].cm = settings;
			need_save = true;
		}
		set_simple_int("moi", scheduler[target].cv, 0, 100);
		set_simple_int("por", scheduler[target].p, 0, 255);
		uint8_t pumps = 0;
		char buf[20];
		for(uint8_t i=0; i<SENSORS; i++) {
			sprintf(buf,"pump%u",i);
			if( HTTP.hasArg(buf) ) pumps |= 1 << i;
		}
		if( pumps != scheduler[target].s ) {
			scheduler[target].s = pumps;
			need_save = true;
		}
		// LOG(printf, "scheduler: t=%u, r=%u, cm=%u, cv=%u, p=%u, s=%u, need_save=%i\n",
		// 	scheduler[target].t, scheduler[target].r, scheduler[target].cm, scheduler[target].cv, scheduler[target].p, scheduler[target].s, need_save);
	}
	HTTP.sendHeader("Location", "scheduler.html");
	HTTP.send(303);
	delay(1);
	if( need_save ) save_schedulers();
}

void scheduler_off() {
	if(is_no_auth()) return;
	uint8_t target = 0;
	String name = "t";
	if( HTTP.hasArg(name) ) {
		target = HTTP.arg(name).toInt();
		if( scheduler[target].cm & 128 ) {
			scheduler[target].cm &= ~(128U);
			save_schedulers();
			text_send("1");
		}
	} else
		text_send("0");
}

const char help[] = R"""(
(h)elp= - эта справка
(s)tate= - состояние системы
(e)nable=N - выдать одну порцию
N - номер насоса или 0 - все
)""";

const char* on_off(bool fl) {
	return fl ? "on": "off";
}

// запросы из телеграма
void api() {
	String name = "pin";
	if( HTTP.hasArg(name) ) {
		if(HTTP.arg(name).equals(gs.hub_pin)) {
			int num_args = HTTP.args();
			if(num_args==1) {
				HTTP.send(200, "text/plain", help);
				LOG(println, "api: send help");
			} else {
				for(int i=0; i<num_args; i++) {
					String arg_name = HTTP.argName(i);
					if(arg_name.startsWith("h")) {
						HTTP.send(200, "text/plain", help);
						LOG(println, "api: send help");
					}
					if(arg_name.startsWith("m")) {
						// включает/выключает режим "охраны"
						uint8_t t = gs.sec_enable;
						gs.sec_enable = HTTP.arg(i).toInt();
						// if( t != sec_enable ) {
						// 	save_log_file(sec_enable?SEC_TEXT_ENABLE:SEC_TEXT_DISABLE);
						// 	save_config_security();
						// }
						// if(sec_enable) {
						// 	save_log_file(SEC_TEXT_ENABLE);
						// 	text_send(F("отсылка сообщений включена"));
						// } else {
						// 	save_log_file(SEC_TEXT_DISABLE);
						// 	text_send(F("отсылка сообщений отключена"));
						// }
						LOG(printf, "api: switch security to ", gs.sec_enable);
					}
					if(arg_name.startsWith("e")) { // включение насосов на одну порцию
						int a = HTTP.arg(i).toInt();
						bool fl = false;
						if( a >= 1 && a <= PUMPS ) {
							pq[a-1].need = 1;
							pq[a-1].active = true;
						} else if( a == 0 ) {
							for(uint8_t ii=0; ii<PUMPS; ii++) {
								pq[ii].need = 1;
								pq[ii].active = true;
							}
						}
						text_send(String("enable=") + a + " ok");
					}
					if(arg_name.startsWith("s")) {
						#define MAX_STR 1200
						#define PP(txt, ...) p += snprintf(p, MAX_STR+out-p, txt, __VA_ARGS__)

						char* out = (char*) malloc(MAX_STR * sizeof(char));
						char* p = out;

						PP("hostname: %s\n",  gs.host_name.c_str());

						// отправка сообщения с текущим статусом
						float sum = 0.0f;
						for(uint8_t i=0; i<PUMPS; i++) {
							PP("p: %u, c: %u, v: %.2f\n", i+1, ps[i].count, ps[i].vol); 
							sum += ps[i].vol;
						}
						PP("sum: %.2f liter\n", sum);
						PP("s1: %u%%, s2: %u%%\n", moi_per[0], moi_per[1]);
						PP("bat: %u%%", battery);

						// PP("\nmode: %s", on_off(gs.sec_enable));
						*p = 0;

						#undef PP
						#undef MAX_STR
						text_send(String(out));
						free(out);
						LOG(println, F("api: send status"));
					}
					if(arg_name.startsWith("j")) {
						int a = HTTP.arg(i).toInt();
						if( a==0 ) a=10;
						if( a>48 ) a=48; // ограничение из-за ограничений памяти
						// text_send(read_log_file(a));
						LOG(printf, "api: send %d log string", a);
					}
				}
			}
		}
	}
}
