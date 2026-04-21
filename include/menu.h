#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "defines.h"
#include "gsm.h"
#include "ntp.h"
#ifdef USE_GSM
#include "gsmApi.h"
#endif

// ────────────────────────────────────────────────────────────────────────────────
//   Общее меню как для команд из WEB (hub), так и из телеграм, или SMS
// ────────────────────────────────────────────────────────────────────────────────
String shared_menu(const String &text) {
    if (text == "/help") { // справка
		return
		#ifdef USE_GSM
		gs.active_channel == 4 ? ( // 
			"/ping\n"
			"/status\n"
			"8*n — switch to channel n\n"
			"/active — active channel\n"
			"1*n — water the plant n\n"
			"/pumps\n"
			"/help"
		) : 
		#endif
			(
			"/ping - проверка связи\n"
			"/status - состояние модема\n"
			"/slave - переключится на хаб\n"
			"/wifi - переключится на WiFi\n"
			#ifdef USE_GSM
			"/gprs - переключится на GPRS\n"
			"/sms - переключится на SMS\n"
			"/unread_sms - удалить все sms\n"
			#endif
			"/active - приоритетный канал\n"
			"/chatid - показать ChatID\n"
            "1*n — включить насос n\n"
			"/pumps - состояние насосов\n"
			"8*n - переключить канал сообщений n\n"
			"/help - это меню"
		);
    } else if (text == "/ping") {
		return "Pong!";
	} else if (text == "/status") {
		char buf[100];
		String uptime = String(getUptime(buf));
		String power = "";
		#ifdef PIN_5V
			if( battery.per ) {
				power = (fl_5v ? "Power is ON :) b:" + String(battery.per) + "%\n": "Power is OFF :( b:" + String(battery.per) + "%\n");
			} else
				power = String(fl_5v ? "Power is ON :)\n": "Power is OFF :(\n");
		#endif
		#ifdef USE_GSM
		if (gsm.isSleep) gsm_wake();
		#endif
		return (
		#ifdef USE_GSM
			"GSM info: " + gsm.info + "\n"
			"Signal: "   + gsm.rssi + " dBm\n" +
		#endif
			power + "Uptime: "   + uptime
		);
	} else if (text == "/slave") {
		return switchActiveChannel(ActiveChannel::hub);
	} else if (text == "/wifi") {
		return switchActiveChannel(ActiveChannel::wifi);
	#ifdef USE_GSM
	} else if (text == "/gprs") {
		return switchActiveChannel(ActiveChannel::gprs);
	} else if (text == "/sms") {
		return switchActiveChannel(ActiveChannel::sms);
	} else if (text == "/unread_sms") {
		if (gsm.isSleep) gsm_wake();
		requestAllSMS();
		return "Все SMS прочитаны";
	#endif
	} else if (text == "/active") {
		return "active channel: " + String(gs.active_channel);
	} else if (text.startsWith("1*")) {
		int f = text.lastIndexOf("*");
		int t = text.indexOf("#");
		String pn = (t < 0) ? text.substring(f+1): text.substring(f,t);
		if (pn.length() < 1) return "unknown pump";
		int a = pn.toInt();
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
		return "water " + (a==0 ? "all":pn) + " ok";
	} else if (text.startsWith("8*")) {
		int f = text.lastIndexOf("*");
		int t = text.indexOf("#");
		String pn = (t < 0) ? text.substring(f+1): text.substring(f,t);
		return switchActiveChannel(pn.length() < 1 ? 100: pn.toInt());
	} else if (text == "/pumps") {
		// отправка сообщения с текущим статусом
		String out = gs.host_name;
		float sum = 0.0f;
		for(uint8_t i=0; i<PUMPS; i++) {
			out += "\np: " + String(i+1) + ", c: " + String(ps[i].count) + ", v: " + String(ps[i].vol, 2);
			sum += ps[i].vol;
		}
		out += "\nsum: " + String(sum, 2) + " liter";
		#ifdef USE_MOISTURE_SENSORS
		for(uint8_t i=0; i<SENSORS; i++) {
			out += "\ns" + String(i+1) + ": " + String(moi[i].per) + "%";
		}
		#endif
		#ifdef PIN_BAT
		out += "\nbat: " + String(battery.per) + "%";
		#endif
		return out;
    }
	return "unknow command";
}

#endif