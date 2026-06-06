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
#include <StringConverters.h>

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

// определение адреса хаба и регистрация на нём
bool registration_dev() {
	if (gs.hub_name.length() < 1) return false; // нет имени - нет регистрации

	// одновременные запросы к wifi приводят к перезагрузке, надо блокировать.
	xSemaphoreTake(xMutex, portMAX_DELAY);

	// найти адрес хаба
	hub_ip = resolve_ip((gs.hub_name).c_str());
	// если хаб не найден, то выйти. Может быть найдётся в следующем цикле
	if(hub_ip == INADDR_NONE) {
		xSemaphoreGive(xMutex);
		return false;
	}

	String serverPath = "http://" + hub_ip.toString() + "/registration?pin=" + gs.hub_pin + "&name=" + StringConverters::urlEncode(gs.host_name);

	// Your Domain name with URL path or IP address with path
	http.begin(client, serverPath);

	// If you need Node-RED/server authentication, insert user and password below
	//http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

	// Send HTTP GET request
	int httpResponseCode = http.GET();

	bool success = true;
	if (httpResponseCode > 0) {
		String payload = http.getString();
		if(payload[0] != '1') success = false;
		LOG(printf, "HTTP Response code: %u, payload: %s\n", httpResponseCode, payload);
	} else {
		success = false;
		LOG(printf, "Error code: %s", httpResponseCode);
	}
	// Free resources
	http.end();
	xSemaphoreGive(xMutex);
	return success;
}

// отсылка сообщений в телеграм через хаб
bool tb_send_msg(const char *msg) {
	// если хаб не найден, то выйти.
	LOG(println,msg);
	if(hub_ip == INADDR_NONE) return false;
	
	xSemaphoreTake(xMutex, portMAX_DELAY);

	bool success = true;
	String serverPath = "http://" + hub_ip.toString() + "/send";
	String httpRequestData = "pin=" + gs.hub_pin + "&msg=" + StringConverters::urlEncode(msg);
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
	xSemaphoreGive(xMutex);
	return success;
}
// отсылка сообщений в телеграм через хаб
bool tb_send_msg(String msg) {
	return tb_send_msg(msg.c_str());
}