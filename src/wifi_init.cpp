/*
	Подключение к WiFi
	Если настроек нет или нет подключения к точке доступа,
	поднять свою точку доступа с captive portal для переадресации
	прямо из настроек wifi телефона. После выбора сети и пароля будет
	подключаться автоматически.
*/

#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "wifi_init.h"
#include "defines.h"
#include "web.h"

WiFiManager wm;

bool wifi_isConnected = false;
bool wifi_isPortal = false;
String wifi_message = "";

void wifi_setup() {
	WiFi.mode(WIFI_STA);
	wm.setWiFiAutoReconnect(true);
	wm.setEnableConfigPortal(false);

	//automatically connect using saved credentials if they exist
	//If connection fails it starts an access point with the specified name
	if(wm.autoConnect()) {
		LOG(println, PSTR("WiFi is connected :)"));
		if(fs_isStarted) led.blink(OFF);
	} else {
		LOG(println, PSTR("WiFi is not connected"));
		if(wm.getWiFiIsSaved()) led.blink(ON);
		else wifi_startConfig(true);
	}
}

void wifi_process() {
	if(WiFi.status() == WL_CONNECTED) {
		if( ! wifi_isConnected ) {
			wifi_isConnected = true;
			LOG(println, PSTR("WiFi now is connected"));
		}
	} else {
		if( wifi_isConnected ) {
			wifi_isConnected = false;
			LOG(println, PSTR("No WiFi now"));
		}
		wm.process();
	}
}

String wifi_currentIP() {
	return WiFi.localIP().toString();
}

int8_t wifi_rssi() {
	return WiFi.RSSI();
}

// Включение или отключение ConfigPortal для настройки WiFi
void wifi_startConfig(bool fl) {
	if(fl) {
		const char *SSID = "WateringAP";
		if(wifi_isConnected) {
			web_disable();
			WiFi.disconnect();
		}
		led.blink(ON, 200, 0, 500);
		wm.setConfigPortalBlocking(false);
		wm.startConfigPortal(SSID);
		LOG(println, PSTR("ConfigPortal is started"));
		wifi_isPortal = true;
	} else {
		if(wm.getWiFiIsSaved()) {
			WiFi.begin(wm.getWiFiSSID(),wm.getWiFiPass());
			led.blink(OFF);
			wifi_isPortal = false;
			if(wm.getConfigPortalActive()) wm.stopConfigPortal();
			LOG(println, PSTR("ConfigPortal is stopped"));
			return;
		} else
			led.blink(ON,100,3);
	}
}
