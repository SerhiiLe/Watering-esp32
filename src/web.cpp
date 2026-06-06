/*
	встроенный web сервер для настройки часов
	(для начальной настройки ip и wifi используется wifi_init)
*/

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
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
#include "pump.hpp"
#include "gsm.h"
#ifdef USE_AHTx0
#include "temperature.h"
#endif
#include <WebServerUtils.h>

#define HPP(txt, ...) HTTP.client().printf(txt, __VA_ARGS__)

WebServer HTTP(80);
HTTPUpdateServer httpUpdater;
WebServerUtils<WebServer> web(HTTP);
bool web_isStarted = false;

void save_settings();
void sysinfo();
void moisture();
void moisture_json();
void save_pump();
void save_moisture();
void pump_on();
void full_status();
void hw_info();
void schedule_save();
void schedule_off();
void maintence();
void set_clock();
void onoff();
void logout();
void calibrate();
void api();
void registration();
void send();

bool fileSend(String path);
bool fl_mdns = false;

cur_slave slave[MAX_SLAVES];

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

// отправка простого json
void json_send(String s, uint16_t r = 200) {
	HTTP.send(r, "application/json", s);
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
		HTTP.on("/moisture_json", moisture_json);
		HTTP.on("/save_moisture", save_moisture);
		HTTP.on("/save_pump", save_pump);
		HTTP.on("/pump_on", pump_on);
		HTTP.on("/full_status", full_status);
		HTTP.on("/hw_info", hw_info);
		HTTP.on("/schedule_save", schedule_save);
		HTTP.on("/schedule_off", schedule_off);
		HTTP.on("/clear", maintence);
		HTTP.on("/clock", set_clock);
		HTTP.on("/onoff", onoff);
		HTTP.on("/logout", logout);
		HTTP.on("/api", api);
		HTTP.on("/registration", registration);
		HTTP.on("/send", send);
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
	if(!fs_isStarted) {
		// файловая система не загружена, переход на страничку обновления
		HTTP.client().print("HTTP/1.1 200\r\nContent-Type: text/html\r\nContent-Length: 80\r\nConnection: close\r\n\r\n<html><body><h1><a href='/update'>File system not exist!</a></h1></body></html>");
		return true;
	}
	return web.fileSend(path);
}

/****** обработка разных запросов ******/

// сохранение настроек
void save_settings() {
	if (is_no_auth()) return;
	web.need_save = false;

	if ( web.to_string("host_name", gs.host_name) )
		if(fl_mdns)	MDNS.setInstanceName(gs.host_name);
	web.checkbox("blink_g", gs.blink_g);

		bool sync_time = false;
	if ( web.to_int("tz_shift", gs.tz_shift, -12, 12) )
		sync_time = true;
	if ( web.checkbox("tz_dst", gs.tz_dst) )
		sync_time = true;
	if ( web.to_int("sync_time_period", gs.sync_time_period, 1, 255) )
		ntpSyncTimer.setInterval(3600000U * gs.sync_time_period);

	web.to_int("high_v", gs.high_v, 0, 4095);
	web.to_int("low_v", gs.low_v, 0, 4095);

	// set_simple_int("msec_in_ml", gs.msec_in_ml, 100, 65000);
	web.to_int("doze", gs.doze, 10, 255);

	web.to_int("active_channel", gs.active_channel, ActiveChannel::none,
		#ifdef USE_GSM
		ActiveChannel::sms
		#else
		ActiveChannel::wifi
		#endif
	);

	bool need_registration = false;
	if ( web.to_string("hub_name", gs.hub_name) )
		need_registration = true;
	if ( web.to_string("hub_pin", gs.hub_pin) )
		need_registration = true;
	if ( web.to_int("hub_period", gs.hub_period, 1, 255) )
		hubRegTimer.setInterval(60000U * gs.hub_period);
	web.to_string("slave_pin", gs.slave_pin);
	web.to_int("slave_timeout", gs.slave_timeout, 1, 255);

	bool fl_setTelegram = false;
	web.to_string("tb_name", gs.tb_name);
	if ( web.to_string("tb_chats", gs.tb_chats) )
		fl_setTelegram = true;
	if ( web.to_string("tb_token", gs.tb_token) )
		fl_setTelegram = true;
	if ( web.to_int("tb_rate", gs.tb_rate, 0, 3600) && last_telegram == 0 )
		telegramTimer.setInterval(1000U * gs.tb_rate);
	web.to_int("tb_accelerated", gs.tb_accelerated, 1, 600);
	web.to_int("tb_accelerate", gs.tb_accelerate, 1, 3600);
	web.to_int("tb_ban", gs.tb_ban, 120, 3600);

	web.to_string("sms_phone", gs.sms_phone);
	web.to_string("pin_code", gs.pin_code);
	web.to_string("gprs_apn", gs.gprs_APN);
	web.to_string("gprs_user", gs.gprs_user);
	web.to_string("gprs_pass", gs.gprs_pass);

	bool need_web_restart = false;
	if ( web.to_string("web_login", gs.web_login) )
		need_web_restart = true;
	if ( web.to_string("web_password", gs.web_password) )
		need_web_restart = true;

	HTTP.sendHeader("Location","maintenance.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_settings need_save=%i\n", web.need_save);
	if (web.need_save) save_config_main();
	if (sync_time) syncTime();
	if (need_web_restart) httpUpdater.setup(&HTTP, gs.web_login, gs.web_password);
	if (need_registration) registration_dev();
	if (fl_setTelegram) setup_telegram();
}

// перезагрузка железки, сброс ком-порта, отключение сети и диска, чтобы ничего не мешало перезагрузке
void reboot_board() {
	Serial.flush();
	WiFi.mode(WIFI_OFF);
	WiFi.getSleep(); //disable AP & station by calling "WiFi.mode(WIFI_OFF)" & put modem to sleep
	LittleFS.end();
	delay(1000);
	ESP.restart();
}

void remove_settings(const char *filename) {
	if (LittleFS.exists(filename)) {
		LOG(printf, "Delete file: %s\n", filename);
		LittleFS.remove(filename);
	}
}

void maintence() {
	if (is_no_auth()) return;
	HTTP.sendHeader("Location","/");
	HTTP.send(303); 

	if (HTTP.hasArg("t")) {
		bool fl_reboot = false;
		if (HTTP.arg("t") == "1") { // основные настройки
			remove_settings("/config.json");
			fl_reboot = true;
		}
		if (HTTP.arg("t") == "2") { // рассписание
			remove_settings("/schedule.json");
			fl_reboot = true;
		}
		if (HTTP.arg("t") == "3") { // счётчики расхода воды
			remove_settings("/pump_state.json");
			fl_reboot = true;
		}
		if (HTTP.arg("t") == "96") { // всё, включая настройки wifi
			foget_wifi();
    		File root = LittleFS.open("/");
			if (root) {
			    if (root.isDirectory()) {
				    File file = root.openNextFile();
					while (file) {
						String fileName = file.name();
						if (fileName.endsWith(".json")) {
							file.close(); // Close before deleting!
							LOG(printf, "Delete file: %s\n", fileName);
							LittleFS.remove("/" + fileName); 
						}
						file = root.openNextFile();
					}
				}
			}
			fl_reboot = true;
		}
		if( HTTP.arg("t") == "r" ) { // только перезагрузка
			fl_reboot = true;
		}
		if (fl_reboot) {
			HTTP.sendHeader("Location","/index.html");
			HTTP.send(303);
			delay(1);
			reboot_board();
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
		char buf[50];
		sniprintf(buf, sizeof(buf), 
			"{\"time\":\"%u\","
			"\"date\":\"%u-%02u-%02u\"}",
			t.tm_hour*60+t.tm_min,
			t.tm_year +1900, t.tm_mon +1, t.tm_mday
		);
		json_send(String(buf));
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
		}
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
	JsonDocument doc;

	doc["Uptime"] = getUptime(buf);
	doc["DateTime"] = getTimeDate(buf);
	#ifdef PIN_5V
	doc["fl_5v"] = fl_5v;
	#endif
	#ifdef PIN_BAT
	doc["Battery"] = battery.per;
	doc["Battery_raw"] = battery.raw;
	#endif
	doc["Rssi"] = wifi_rssi();
	doc["IP"] = wifi_currentIP().c_str();
	#ifdef USE_GSM
	doc["gsm_info"] = gsm.info.c_str();
	doc["gsm_rssi"] = map(gsm.rssi, 0, 32, 0, 100);
	doc["gsm_status"] = gsm.isSleep ? -10: gsm.status;
	#endif
	#ifdef USE_AHTx0
	doc["aht"] = fl_AHTIsInit ? "Ok":"None";
	#endif
	doc["FreeHeap"] = ESP.getFreeHeap();
	doc["MaxFreeBlockSize"] = ESP.getMaxAllocHeap();
	doc["HeapFragmentation"] = 100-ESP.getMaxAllocHeap()*100/ESP.getFreeHeap();
	doc["ResetReason"] = print_reset_reason(buf);
	doc["FullVersion"] = print_full_platform_info(buf);
	doc["CpuFreqMHz"] = ESP.getCpuFreqMHz();
	sprintf(buf, "%s %s", PSTR(__DATE__), PSTR(__TIME__));
	doc["BuildTime"] = buf;

	String jsonPayload;
	serializeJson(doc, jsonPayload);
	json_send(jsonPayload);
}

// Информация о состоянии датчика влажности
void moisture() {
	if(is_no_auth()) return;
	char buf[200] = "{";
	size_t s = 1;
	for(uint8_t i=0; i<SENSORS; i++) {
		s += snprintf(buf + s, sizeof(buf) - s,
			"\"raw%u\":%u,"
			"\"per%u\":%u%s",
			i, moi[i].raw,
			i, moi[i].per, i+1<SENSORS ? ",":"}"
		);
	}
	json_send(String(buf));
}

// список датчиков влажности
void moisture_json() {
	if(is_no_auth()) return;
	JsonDocument doc;
	for(uint8_t i=0; i<SENSORS; i++) {
		doc[i]["moi0"] = mc[i].moi0;
		doc[i]["moi100"] = mc[i].moi100;
	}
	String jsonPayload;
	serializeJson(doc, jsonPayload);
	json_send(jsonPayload);
}

// сохранение настроек датчиков влажности
void save_moisture() {
	if(is_no_auth()) return;
	web.need_save = false;

	char buf[20];
	for(uint8_t i=0; i<SENSORS; i++) {
		sprintf(buf,"moi0_%u",i);
		web.to_int(buf, mc[i].moi0, 0, 4095);
		sprintf(buf,"moi100_%u",i);
		web.to_int(buf, mc[i].moi100, 0, 4095);
	}

	HTTP.sendHeader("Location","moisture.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_moisture need_save=%i\n", web.need_save);
	if( web.need_save ) save_moisture_calibration();
}

// сохранить калибровку насосов
void save_pump() {
	if(is_no_auth()) return;
	web.need_save = false;

	web.to_int("pump", pc.pump, 1, PUMPS);
	web.to_int("sec1", pc.sec1, 1, 255);
	web.to_float("tara", pc.tara, 1, 1e6, 2.0f);
	web.to_float("weight1", pc.weight1, 1, 1e6, 2.0f);
	web.to_float("weight2", pc.weight2, 1, 1e6, 2.0f);
	web.to_int("in_ms", pc.in_ms, 1, 65000);
	web.to_int("empty_ms", pc.empty_ms, 1, 65000);

	HTTP.sendHeader("Location","pump.html");
	HTTP.send(303);
	delay(1);
	LOG(printf, "save_pump need_save=%i\n", web.need_save);
	if( web.need_save ) save_pump_calibration();
}

extern PumpWater p[];

// запуск насоса на нужное количество порций/секунд
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
		if( ! p[pump].active() ) {
			pq[pump].active = true;
			pq[pump].need = cnt;
			pq[pump].seconds = sec;
		}
		cond = true;
	}
	text_send(cond?"1":"0");
}

// информация для первой страницы о состоянии насосов, расходе воды, температуре, влажности
void full_status() {
	if(is_no_auth()) return;
	char buf[100];
	JsonDocument doc;

	doc["hostname"] = gs.host_name.c_str();
	#ifdef USE_AHTx0
		float temp = 0.0f, hum = 0.0f;
		getTemperature(temp, hum);
		doc["temperature"] = serialized(String(temp, 1));
		doc["humidity"] = serialized(String(hum, 1));
	#endif
	JsonArray pumps = doc["pumps"].to<JsonArray>();
	for(uint8_t i=0; i<PUMPS; i++) {
		pumps[i]["con"] = serialized(String(ps[i].vol, 2));
		pumps[i]["cnt"] = ps[i].count;
		pumps[i]["last"] = getTimeDateU(buf, ps[i].last < p[i].last() ? p[i].last(): ps[i].last);
		pumps[i]["pump"] = p[i].active();
	}
	JsonArray sensors = doc["sensors"].to<JsonArray>();
	for(uint8_t i=0; i<SENSORS; i++) {
		sensors.add(moi[i].per);
	}
	JsonArray slaves = doc["slaves"].to<JsonArray>();
	uint8_t cur_sl = 0;
	for(uint8_t i=0; i<MAX_SLAVES; i++) {
		if (slave[i].registered >= getTimeU() - gs.slave_timeout*60 + 60) {
			slaves[cur_sl]["num"] = i;
			slaves[cur_sl]["hostname"] = slave[i].hostname.c_str();
			slaves[cur_sl]["ip"] = slave[i].ip.toString().c_str();
			slaves[cur_sl]["timeout"] = gs.slave_timeout*60 + slave[i].registered - getTimeU();
			cur_sl++;
		}
	}

	String jsonPayload;
	serializeJson(doc, jsonPayload);
	json_send(jsonPayload);
}

// информация об аппаратной конфигурации
void hw_info() {
	JsonDocument doc;

	#ifdef USE_GSM
	doc["use_gsm"] = 1;
	#else
	doc["use_gsm"] = 0;
	#endif
	#ifdef USE_MOISTURE_SENSORS
	doc["use_moisture"] = 1;
	#else
	doc["use_moisture"] = 0;
	#endif
	#ifdef USE_AHTx0
	doc["use_aht"] = 1;
	#else
	doc["use_aht"] = 0;
	#endif
	#ifdef PIN_LED_GREEN
	doc["use_green"] = 1;
	#else
	doc["use_green"] = 0;
	#endif
	#ifdef PIN_BAT
	doc["use_battary"] = 1;
	#else
	doc["use_battary"] = 0;
	#endif
	#ifdef PIN_5V
	doc["use_5v"] = 1;
	#else
	doc["use_5v"] = 0;
	#endif

	String jsonPayload;
	serializeJson(doc, jsonPayload);
	json_send(jsonPayload);
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

void schedule_save() {
	if(is_no_auth()) return;
	web.need_save = false;
	uint8_t target = 0;
	String name = "target";
	if( HTTP.hasArg(name) ) {
		target = HTTP.arg(name).toInt();
		web.time("time", schedule[target].t);
		web.time("repeat", schedule[target].r);
		uint8_t settings = 128;
		name = F("mode");
		if( HTTP.hasArg(name) ) settings |= constrain(HTTP.arg(name).toInt(), 0, 3) << 5;
		name = F("sensor");
		if( HTTP.hasArg(name) ) settings |= constrain(HTTP.arg(name).toInt(), 0, 31); // max 31
		if( settings != schedule[target].cm ) {
			schedule[target].cm = settings;
			web.need_save = true;
		}
		web.to_int("moi", schedule[target].cv, 0, 100);
		web.to_int("por", schedule[target].p, 0, 255);
		uint8_t pumps = 0;
		char buf[20];
		for(uint8_t i=0; i<PUMPS; i++) {
			sprintf(buf,"pump%u",i);
			if( HTTP.hasArg(buf) ) pumps |= 1 << i;
		}
		if( pumps != schedule[target].s ) {
			schedule[target].s = pumps;
			web.need_save = true;
		}
		// LOG(printf, "schedule: t=%u, r=%u, cm=%u, cv=%u, p=%u, s=%u, need_save=%i\n",
		// 	schedule[target].t, schedule[target].r, schedule[target].cm, schedule[target].cv, schedule[target].p, schedule[target].s, need_save);
	}
	HTTP.sendHeader("Location", "schedule.html");
	HTTP.send(303);
	delay(1);
	if( web.need_save ) save_schedules();
}

void schedule_off() {
	if(is_no_auth()) return;
	uint8_t target = 0;
	String name = "t";
	if( HTTP.hasArg(name) ) {
		target = HTTP.arg(name).toInt();
		if( schedule[target].cm & 128 ) {
			schedule[target].cm &= ~(128U);
			save_schedules();
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
/help - расширенная справка
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
						text_send(help);
						// HTTP.send(200, "text/plain", help);
						LOG(println, "api: send help");
					}
					if (arg_name == "cmd") {
						text_send(shared_menu(HTTP.arg(i)));
					}
					if(arg_name.startsWith("e")) { // включение насосов на одну порцию
						text_send(shared_menu("1*" + HTTP.arg(i)));
					}
					if(arg_name.startsWith("s")) {
						text_send(shared_menu("/status"));
						LOG(println, F("api: send status"));
					}
				}
			}
		}
	}
}

// "proxy" отправка сообщений в телеграм через web запрос, для сторонних устройств
void send() {
	String name = "pin";
	if( HTTP.hasArg(name) ) {
		if( gs.slave_pin == HTTP.arg(name) ) {
			name = "msg";
			if( HTTP.hasArg(name) ) {
				sendMessage(HTTP.arg(name));
				text_send("1");
				return;
			}
		}
	}
	text_send("0");
}

// Регистрация подчинённого устройства
void registration() {
	// Авторизация примитивная, через shared key, или pin
	// должно быть всего два параметра: pin (shared key) и name - имя устройства
	String name = "pin";
	if( HTTP.hasArg(name) ) {
		if( gs.slave_pin == HTTP.arg(name) ) {
			// pin подошел
			name = "name";
			if( HTTP.hasArg(name) ) {
				IPAddress slave_ip = HTTP.client().remoteIP();
				bool fl_found = false;
				// проверка, зарегистрирован ли уже этот ip
				for(uint8_t i=0; i<MAX_SLAVES; i++) {
					if(slave[i].ip == slave_ip) {
						// надо ли обновить hostname
						if(slave[i].hostname != HTTP.arg(name))
							slave[i].hostname = HTTP.arg(name);
						// обновить время регистрации
						slave[i].registered = getTimeU();
						fl_found = true;
						break;
					}
				}
				if(!fl_found) {
					// новый подчинённый, поиск свободной ячейки
					for(uint8_t i=0; i<MAX_SLAVES; i++) {
						if(slave[i].registered < getTimeU() - gs.slave_timeout*60 - 60) {
							// найдена свободная ячейка
							slave[i].hostname = HTTP.arg(name);
							slave[i].ip = slave_ip;
							slave[i].registered = getTimeU();
							fl_found = true;
							break;
						}
					}
				}
				if(fl_found) {
					LOG(printf, "registered \"%s\" ip %s\n", HTTP.arg(name).c_str(), slave_ip.toString().c_str());
					text_send("1");
					return;
				}
			}
		}
	}
	// любая ошибка, в том числе закончились свободные ячейки
	text_send("0");
}
