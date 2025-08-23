/*
	Работа с настройками.
	Инициализация по умолчанию, чтение из файла, сохранение в файл
*/

#include <Arduino.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <LittleFS.h>
#include "defines.h"
#include "settings.h"
#include "ntp.h"
#include "pump.h"

Global_Settings gs; // определение структуры глобальной (главной) конфигурации
Moisture_Calibrate mc[SENSORS]; // калибровка датчиков влажности
Log_State ls; // состояние журнала
Scheduler_State scheduler[SCHEDULERS]; // состояние расписания
Pump_Calibrate pc; // калибровка насосов

bool load_config_main() {
	if(!fs_isStarted) return false;

	File configFile = LittleFS.open("/config.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "Failed to open main config file");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();

	// Test if parsing succeeds.
	if (error) {
		LOG(printf, "deserializeJson() failed: %s\n", error.c_str());
		return false;
	}

	// Fetch values.
	// const char* sensor = doc["sensor"];
	// long time = doc["time"];
	// double latitude = doc["data"][0];
	// double longitude = doc["data"][1];

	gs.host_name = doc["host_name"].as<String>();
	gs.sec_enable = doc["sec_enable"];

	gs.tz_shift = doc["tz_shift"];
	gs.tz_dst = doc["tz_dst"];
	gs.sync_time_period = doc["sync_time_period"]; ntpSyncTimer.setInterval(3600000U * gs.sync_time_period);

	gs.high_v = doc["high_v"];
	gs.low_v = doc["low_v"];

	// gs.msec_in_ml = doc["msec_in_ml"];
	gs.doze = doc["doze"];

	gs.hub_name = doc["hub_name"].as<String>();
	gs.hub_pin = doc["hub_pin"].as<String>();
	gs.hub_period = doc["hub_period"];

	gs.tb_name = doc["tb_name"].as<String>();
	gs.tb_chats = doc["tb_chats"].as<String>();
	gs.tb_token = doc["tb_token"].as<String>();
	gs.tb_secret = doc["tb_secret"].as<String>();
	gs.tb_rate = doc["tb_rate"];
	gs.tb_accelerated = doc["tb_accelerated"];
	telegramTimer.setInterval(1000U * gs.tb_accelerated);
	gs.tb_accelerate = doc["tb_accelerate"];
	gs.tb_ban = doc["tb_ban"];

	gs.sms_use = doc["sms_use"];
	gs.sms_phone = doc["sms_phone"].as<String>();

	gs.web_login = doc["web_login"].as<String>();
	gs.web_password = doc["web_password"].as<String>();

	LOG(println, "load config.json");
	return true;
}

void save_config_main() {
	if(!fs_isStarted) return;

	JsonDocument doc; // временный буфер под объект json

	doc["host_name"] = gs.host_name;
	doc["sec_enable"] = gs.sec_enable;

	doc["tz_shift"] = gs.tz_shift;
	doc["tz_dst"] = gs.tz_dst;
	doc["sync_time_period"] = gs.sync_time_period;

	doc["high_v"] = gs.high_v;
	doc["low_v"] = gs.low_v;

	// doc["msec_in_ml"] = gs.msec_in_ml;
	doc["doze"] = gs.doze;

	doc["hub_name"] = gs.hub_name;
	doc["hub_pin"] = gs.hub_pin;
	doc["hub_period"] = gs.hub_period;

	doc[F("tb_name")] = gs.tb_name;
	doc[F("tb_chats")] = gs.tb_chats;
	doc[F("tb_token")] = gs.tb_token;
	doc[F("tb_secret")] = gs.tb_secret;
	doc[F("tb_rate")] = gs.tb_rate;
	doc[F("tb_accelerated")] = gs.tb_accelerated;
	doc[F("tb_accelerate")] = gs.tb_accelerate;
	doc[F("tb_ban")] = gs.tb_ban;

	doc["sms_use"] = gs.sms_use;
	doc["sms_phone"] = gs.sms_phone;

	doc["web_login"] = gs.web_login;
	doc["web_password"] = gs.web_password;

	File configFile = LittleFS.open("/config.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open config file for writing");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush();
	configFile.close(); // не забыть закрыть файл
	delay(4);

	LOG(println, "save /config.json");
}

// чтение калибровки датчиков влажности
bool load_moisture_calibration() {
	if(!fs_isStarted) return false;

	File configFile = LittleFS.open("/moisture.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "File moisture.json not found");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();
	
	// Test if parsing succeeds.
	if (error) {
		LOG(printf, "deserializeJson() failed: %s\n", error.c_str());
		return false;
	}

	for(uint8_t i=0; i<min((int)SENSORS,(int)doc.size()); i++) {
		mc[i].moi0 = doc[i]["moi0"];
		mc[i].moi100 = doc[i]["moi100"];
	}

	LOG(println, "moisture calibration loaded");
	return true;
}

void save_moisture_calibration() {
	if(!fs_isStarted) return;

	JsonDocument doc; // временный буфер под объект json

	for(uint8_t i=0; i<SENSORS; i++) {
		doc[i]["moi0"] = mc[i].moi0;
		doc[i]["moi100"] = mc[i].moi100;
	}

	File configFile = LittleFS.open("/moisture.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open config file for writing (moisture.json)");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush(); // подождать, пока данные запишутся. Хотя close должен делать это сам, но без иногда перезагружается.
	configFile.close(); // не забыть закрыть файл
	delay(2);

	LOG(println, "moisture calibration saved");
}

// чтение калибровки наносов
bool load_pump_calibration() {
		if(!fs_isStarted) return false;

	File configFile = LittleFS.open("/pump.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "File pump.json not found");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();
	
	// Test if parsing succeeds.
	if (error) {
		LOG(printf, "deserializeJson() failed: %s\n", error.c_str());
		return false;
	}

	pc.pump = doc["pump"];
	pc.sec1 = doc["sec1"];
	pc.tara = doc["tara"];
	pc.weight1 = doc["weight1"];
	pc.weight2 = doc["weight2"];
	pc.in_ms = doc["in_ms"];
	pc.empty_ms = doc["empty_ms"];

	LOG(println, "pump calibration loaded");
	return true;
}

void save_pump_calibration() {
	if(!fs_isStarted) return;

	JsonDocument doc; // временный буфер под объект json

	doc["pump"] = pc.pump;
	doc["sec1"] = pc.sec1;
	doc["tara"] = pc.tara;
	doc["weight1"] = pc.weight1;
	doc["weight2"] = pc.weight2;
	doc["in_ms"] = pc.in_ms;
	doc["empty_ms"] = pc.empty_ms;

	File configFile = LittleFS.open("/pump.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open config file for writing (pump.json)");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush(); // подождать, пока данные запишутся. Хотя close должен делать это сам, но без иногда перезагружается.
	configFile.close(); // не забыть закрыть файл
	delay(2);

	LOG(println, "pump calibration saved");
}

// чтение расписания
bool load_schedulers() {
	File configFile = LittleFS.open("/scheduler.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "Failed to open config for scheduler file");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();

	// Test if parsing succeeds.
	if (error) {
		LOG(printf_P, PSTR("deserializeJson() failed: %s\n"), error.c_str());
		return false;
	}

	if(!doc["data"].is<JsonArray>()) return false;
	for( int i=0; i<min((int)SCHEDULERS,(int)doc["data"].size()); i++) {
		scheduler[i].s = doc["data"][i]["s"];
		scheduler[i].t = doc["data"][i]["t"];
		scheduler[i].r = doc["data"][i]["r"];
		scheduler[i].cm = doc["data"][i]["cm"];
		scheduler[i].cv = doc["data"][i]["cv"];
		scheduler[i].p = doc["data"][i]["p"];
	}
	return true;
}

// запись расписания
void save_schedulers() {
	JsonDocument doc; // временный буфер под объект json

	doc["pc"] = PUMPS;
	doc["sc"] = SENSORS;
	JsonArray data = doc["data"].to<JsonArray>();
	for( int i=0; i<SCHEDULERS; i++) {
		data[i]["s"] = scheduler[i].s;
		data[i]["t"] = scheduler[i].t;
		data[i]["r"] = scheduler[i].r;
		data[i]["cm"] = scheduler[i].cm;
		data[i]["cv"] = scheduler[i].cv;
		data[i]["p"] = scheduler[i].p;
	}

	File configFile = LittleFS.open("/scheduler.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open scheduler.json for writing");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush();
	configFile.close(); // не забыть закрыть файл
	delay(2);
	LOG(println, "save /scheduler.json");
}

bool load_pump_state() {
	File configFile = LittleFS.open("/pump_state.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "Failed to open pump_state.json");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();

	// Test if parsing succeeds.
	if (error) {
		LOG(printf_P, PSTR("deserializeJson() failed: %s\n"), error.c_str());
		return false;
	}

	for( int i=0; i<min((int)PUMPS,(int)doc.size()); i++) {
		ps[i].count = doc[i]["c"];
		ps[i].vol = doc[i]["v"];
		ps[i].last = doc[i]["l"];
	}
	return true;
}

extern PumpWater p[];

void save_pump_state() {
	JsonDocument doc; // временный буфер под объект json

	for( int i=0; i<PUMPS; i++) {
		doc[i]["c"] = ps[i].count;
		doc[i]["v"] = ps[i].vol;
		doc[i]["l"] = ps[i].last < p[i].last() ? p[i].last(): ps[i].last;
	}

	File configFile = LittleFS.open("/pump_state.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open pump_state.json for writing");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush();
	configFile.close(); // не забыть закрыть файл
	delay(2);
}

// чтение лога
bool load_config_log() {
	if(!fs_isStarted) return false;

	File configFile = LittleFS.open("/log.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "Failed to open config for log file");
		return false;
	}

	JsonDocument doc; // временный буфер под объект json

	DeserializationError error = deserializeJson(doc, configFile);
	configFile.close();
	
	// Test if parsing succeeds.
	if (error) {
		LOG(printf_P, "deserializeJson() failed: %s\n", error.c_str());
		return false;
	}

	ls.enable = doc["enable"];
	ls.curFile = doc["curFile"];

	LOG(println, "log state loaded");
	return true;
}

void save_config_log() {
	if(!fs_isStarted) return;

	JsonDocument doc; // временный буфер под объект json

	doc["enable"] = ls.enable;
	doc["curFile"] = ls.curFile;
	doc["logs_count"] = LOG_COUNT;

	File configFile = LittleFS.open("/log.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open config file for writing (log.json)");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush(); // подождать, пока данные запишутся. Хотя close должен делать это сам, но без иногда перезагружается.
	configFile.close(); // не забыть закрыть файл
	delay(2);

	LOG(println, "log state saved");
}

// чтение последних cnt строк лога
String read_log_file(int16_t cnt) {
	if(!fs_isStarted) return String(F("no fs"));

	// всего надо отдать cnt последних строк.
	// Если файл только начал писаться, то надо показать последние записи предыдущего файла
	// сначала считывается предыдущий файл
	int16_t cur = 0;
	int16_t aCnt = 0;
	char aStr[cnt][LOG_MAX];
	uint8_t prevFile = ls.curFile > 0 ? ls.curFile-1: LOG_COUNT-1; // вычисление предыдущего лог-файла
	char fileName[32];
	sprintf(fileName, LOG_FILE, prevFile);
	File logFile = LittleFS.open(fileName, "r");
	if(logFile) {
		while(logFile.available()) {
			strncpy(aStr[cur], logFile.readStringUntil('\n').c_str(), LOG_MAX); // \r\n
			cur = (cur+1) % cnt;
			aCnt++;
		}
	}
	logFile.close();
	// теперь считывается текущий файл
	sprintf_P(fileName, LOG_FILE, ls.curFile);
	logFile = LittleFS.open(fileName, "r");
	if(logFile) {
		while(logFile.available()) {
			strncpy(aStr[cur], logFile.readStringUntil('\n').c_str(), LOG_MAX);
			cur = (cur+1) % cnt;
			aCnt++;
		}
	}
	logFile.close();
	// теперь надо склеить массив в одну строку и отдать назад
	String str = "";
	char *ptr;
	for(int16_t i = min(aCnt,cnt); i > 0; i--) {
		cur = cur > 0 ? cur-1: cnt-1;
		ptr = aStr[cur] + strlen(aStr[cur]) - 1;
		if( i>1 ) strcpy(ptr, "\n"); // возврат последнего символа перевод строки
		str += aStr[cur];
	}
	return str;
}

void save_log_file(String text) {
	save_log_file(text.c_str());
}
void save_log_file(const char* text) {
	if(!fs_isStarted) return;

	char fileName[32];
	sprintf(fileName, LOG_FILE, ls.curFile);
	File logFile = LittleFS.open(fileName, "a");
	if (!logFile) {
		// не получилось открыть файл на дополнение
		LOG(println, "Failed to open log file");
		return;
	}
	// проверка, не превышен ли лимит размера файла, если да, то открыть второй файл.
	size_t size = logFile.size();
	if (size > LOG_SIZE) {
		LOG(println, "Log file size is too large, switch file");
		logFile.close();
		ls.curFile = (ls.curFile+1) % LOG_COUNT;
		save_config_log();
		sprintf(fileName, LOG_FILE, ls.curFile);
		logFile = LittleFS.open(fileName, "w");
		if (!logFile) {
			// ошибка создания файла
			LOG(println, "Failed to open new log file");
			return;
		}
	}
	// составление строки которая будет занесена в файл
	char str[LOG_MAX];
	tm t = getTime();
	snprintf_P(str, LOG_MAX, PSTR("%04u-%02u-%02u %02u:%02u| %s"), t.tm_year +1900, t.tm_mon +1, t.tm_mday, t.tm_hour, t.tm_min, text);

	LOG(println, str);
	logFile.println(str);

	logFile.flush();
	logFile.close();
	delay(2);
}

