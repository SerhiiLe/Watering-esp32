/*
	Подключение к "базе" в качестве раба... ведомого
	определение адреса сервера хозяина через mDNS
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "defines.h"
#include "slave.h"

// Создание объектов для отправки запросов.
WiFiClient client;
HTTPClient http;

IPAddress hub_ip = INADDR_NONE;

// поиск IP по имени по протоколу Bonjour
IPAddress resolve_ip(const char *resolveName) {
	unsigned long startTime = millis();
  	const unsigned long timeout = 10000; // Таймаут 10 секунд
	IPAddress ip;

  	while (millis() - startTime < timeout) {
		ip = MDNS.queryHost(resolveName);
		if (ip != INADDR_NONE) {
			LOG(printf, "Resolved %s to IP address: %s\n", resolveName, ip.toString().c_str());
			break;
		} else {
			LOG(printf, "Unable to resolve %s.\n", resolveName);
		}
	}
	return ip;
}

// кодирование строки для GET запросов
String urlEncode(const char *str) {
	size_t len = strlen(str);
	int buffer_size = len * 3 < TELEGRAM_MAX_LENGTH ? len * 3: TELEGRAM_MAX_LENGTH;
	char* encodedString = (char*) malloc(buffer_size * sizeof(char));
	char* p = encodedString;
	char c;
	char code0;
	char code1;
	for(unsigned int i=0; i < len; i++) {
		if(buffer_size+encodedString-p < 3) break;
		c=str[i];
		if(isalnum(c)) {
			*p++ = c;
		} else {
			code1=(c & 0xf)+'0';
			if((c & 0xf) > 9) {
				code1 = (c & 0xf) - 10 + 'A';
			}
			c = (c>>4) & 0xf;
			code0 = c+'0';
			if(c > 9) {
				code0=c - 10 + 'A';
			}
			*p++ = '%';
			*p++ = code0;
			*p++ = code1;
		}
	}
	*p = 0;
	String result = String(encodedString);
	free(encodedString);
	return result;
}
// кодирование строки для GET запросов
String urlEncode(String str) {
	return urlEncode(str.c_str());
}

// определение адреса хаба и регистрация на нём
bool registration_dev() {
	// найти адрес хаба
	hub_ip = resolve_ip((gs.hub_name).c_str());
	// если хаб не найден, то выйти. Может быть найдётся в следующем цикле
	if(hub_ip == INADDR_NONE) return false;


	String serverPath = "http://" + hub_ip.toString() + "/registration?pin=" + gs.hub_pin + "&name=" + urlEncode(gs.host_name);

	// Your Domain name with URL path or IP address with path
	http.begin(client, serverPath);

	// If you need Node-RED/server authentication, insert user and password below
	//http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

	// Send HTTP GET request
	int httpResponseCode = http.GET();

	bool success = true;
	if (httpResponseCode > 0) {
		String payload = http.getString();
		if(payload.charAt(0) != '1') success = false;
		LOG(printf, "HTTP Response code: %u, payload: %s\n", httpResponseCode, payload);
	} else {
		success = false;
		LOG(printf, "Error code: %s", httpResponseCode);
	}
	// Free resources
	http.end();
	return success;
}

// отсылка сообщений в телеграм через хаб
bool tb_send_msg(const char *msg) {
	// если хаб не найден, то выйти.
	LOG(println,msg);
	if(hub_ip == INADDR_NONE) return false;
	// IPAddress ip = {172,16,1,133};

	if(gs.sec_enable==0) return true; 

	bool success = true;
	String serverPath = "http://" + hub_ip.toString() + "/send";
	String httpRequestData = "pin=" + gs.hub_pin + "&msg=" + urlEncode(msg);
	LOG(println,serverPath);
	LOG(println,httpRequestData);
	http.begin(client, serverPath);
	http.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpResponseCode = http.POST(httpRequestData);
		if (httpResponseCode > 0) {
		String payload = http.getString();
		if(payload.charAt(0) != '1') success = false;
		LOG(printf, "HTTP Response code: %u, payload: %s\n", httpResponseCode, payload);
	} else {
		success = false;
		LOG(printf, "Error code: %s", httpResponseCode);
	}
	// Free resources
	http.end();
	return success;
}
// отсылка сообщений в телеграм через хаб
bool tb_send_msg(String msg) {
	return tb_send_msg(msg.c_str());
}