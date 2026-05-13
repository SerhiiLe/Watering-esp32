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
#include "pump.hpp"

Global_Settings gs; // определение структуры глобальной (главной) конфигурации
Moisture_Calibrate mc[SENSORS]; // калибровка датчиков влажности
Log_State ls; // состояние журнала
Schedule_State schedule[SCHEDULES]; // состояние расписания
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
	gs.blink_g = doc["blink_g"];
	
	gs.tz_shift = doc["tz_shift"];
	gs.tz_dst = doc["tz_dst"];
	gs.sync_time_period = doc["sync_time_period"]; ntpSyncTimer.setInterval(3600000U * gs.sync_time_period);

	gs.high_v = doc["high_v"];
	gs.low_v = doc["low_v"];

	// gs.msec_in_ml = doc["msec_in_ml"];
	gs.doze = doc["doze"];

	gs.active_channel = doc["active_channel"];

	gs.hub_name = doc["hub_name"].as<String>();
	gs.hub_pin = doc["hub_pin"].as<String>();
	gs.hub_period = doc["hub_period"];
	gs.slave_pin = doc["slave_pin"].as<String>();
	gs.slave_timeout = doc["slave_timeout"];

	gs.tb_name = doc["tb_name"].as<String>();
	gs.tb_chats = doc["tb_chats"].as<String>();
	gs.tb_token = doc["tb_token"].as<String>();
	gs.tb_rate = doc["tb_rate"];
	gs.tb_accelerated = doc["tb_accelerated"]; telegramTimer.setInterval(1000U * gs.tb_accelerated);
	gs.tb_accelerate = doc["tb_accelerate"];
	gs.tb_ban = doc["tb_ban"];

	gs.sms_phone = doc["sms_phone"].as<String>();
	gs.pin_code = doc["pin_code"].as<String>();
	gs.gprs_APN = doc["gprs_apn"].as<String>();
	gs.gprs_user = doc["gprs_user"].as<String>();
	gs.gprs_pass = doc["gprs_pass"].as<String>();

	gs.web_login = doc["web_login"].as<String>();
	gs.web_password = doc["web_password"].as<String>();

	LOG(println, "load config.json");
	return true;
}

void save_config_main() {
	if(!fs_isStarted) return;

	JsonDocument doc; // временный буфер под объект json

	doc["host_name"] = gs.host_name;
	doc["blink_g"] = gs.blink_g;
	
	doc["tz_shift"] = gs.tz_shift;
	doc["tz_dst"] = gs.tz_dst;
	doc["sync_time_period"] = gs.sync_time_period;

	doc["high_v"] = gs.high_v;
	doc["low_v"] = gs.low_v;

	// doc["msec_in_ml"] = gs.msec_in_ml;
	doc["doze"] = gs.doze;

	doc["active_channel"] = gs.active_channel;

	doc["hub_name"] = gs.hub_name;
	doc["hub_pin"] = gs.hub_pin;
	doc["hub_period"] = gs.hub_period;
	doc["slave_pin"] = gs.slave_pin;
	doc["slave_timeout"] = gs.slave_timeout;

	doc[F("tb_name")] = gs.tb_name;
	doc[F("tb_chats")] = gs.tb_chats;
	doc[F("tb_token")] = gs.tb_token;
	doc[F("tb_rate")] = gs.tb_rate;
	doc[F("tb_accelerated")] = gs.tb_accelerated;
	doc[F("tb_accelerate")] = gs.tb_accelerate;
	doc[F("tb_ban")] = gs.tb_ban;

	doc["sms_phone"] = gs.sms_phone;
	doc["pin_code"] = gs.pin_code;
	doc["gprs_apn"] = gs.gprs_APN;
	doc["gprs_user"] = gs.gprs_user;
	doc["gprs_pass"] = gs.gprs_pass;

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
bool load_schedules() {
	File configFile = LittleFS.open("/schedule.json", "r");
	if (!configFile) {
		// если файл не найден  
		LOG(println, "Failed to open config for schedule file");
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
	for( int i=0; i<min((int)SCHEDULES,(int)doc["data"].size()); i++) {
		schedule[i].s = doc["data"][i]["s"];
		schedule[i].t = doc["data"][i]["t"];
		schedule[i].r = doc["data"][i]["r"];
		schedule[i].cm = doc["data"][i]["cm"];
		schedule[i].cv = doc["data"][i]["cv"];
		schedule[i].p = doc["data"][i]["p"];
	}
	return true;
}

// запись расписания
void save_schedules() {
	JsonDocument doc; // временный буфер под объект json

	doc["pc"] = PUMPS;
	doc["sc"] = SENSORS;
	JsonArray data = doc["data"].to<JsonArray>();
	for( int i=0; i<SCHEDULES; i++) {
		data[i]["s"] = schedule[i].s;
		data[i]["t"] = schedule[i].t;
		data[i]["r"] = schedule[i].r;
		data[i]["cm"] = schedule[i].cm;
		data[i]["cv"] = schedule[i].cv;
		data[i]["p"] = schedule[i].p;
	}

	File configFile = LittleFS.open("/schedule.json", "w"); // открытие файла на запись
	if (!configFile) {
		LOG(println, "Failed to open schedule.json for writing");
		return;
	}
	serializeJson(doc, configFile); // Записываем строку json в файл
	configFile.flush();
	configFile.close(); // не забыть закрыть файл
	delay(2);
	LOG(println, "save /schedule.json");
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
