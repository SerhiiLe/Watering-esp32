/*
	Меню общее для Telegram, Hub, SMS
*/

#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "defines.h"
#include "menu.h"
#include "gsm.h"
#include "ntp.h"
#include "slave.h"
#include "web.h"
#include "settings.h"
#include <WiFiClient.h>
#include <HTTPClient.h>
#ifdef USE_GSM
#include "gsmApi.h"
#endif


// вывод текущего статуса и статистики по насосам и датчикам влажности
String print_pumps_status() {
	String out = "";
	float sum = 0.0f;
	for(uint8_t i=0; i<PUMPS; i++) {
		out += "p:" + String(i+1) + ", c:" + String(ps[i].count) + ", v:" + String(ps[i].vol, 2) + "\n";
		sum += ps[i].vol;
	}
	#ifdef USE_MOISTURE_SENSORS
	for(uint8_t i=0; i<SENSORS; i++) {
		out += "s" + String(i+1) + ": " + String(moi[i].per) + "%\n";
	}
	#endif
	out += "sum: " + String(sum, 2) + " liter";
	return out;
}

// вывод времени в строку, входной формат - время в минутах с полуночи, на выходе hh:mm
String print_decoded_time(int t) {
	int h = t / 60;
	int m = t % 60;
	char buf[6];
	sprintf(buf, "%i:%02i", h, m);
	return String(buf);
}

// формирование строки с одной строкой расписания (i - номер расписания, с нуля)
String print_schedule(uint8_t i) {
	String out = "";
	// время  начала срабатывания
	out += "\n" + String(i+1) + ") " + print_decoded_time(schedule[i].t);
	// интервал повтора
	if (schedule[i].r > 0)
		out += " (" + print_decoded_time(schedule[i].r) + ")";
	// количество порций
	if (schedule[i].p > 1)
		out += " x" + String(schedule[i].p);
	// список задействованых насосов
	if (schedule[i].s == (1U<<PUMPS)-1)
		out += " pAll";
	else
		for (uint8_t ii=0; ii<PUMPS; ii++) {
			if (schedule[i].s & (1<<ii))
				out += " p" + String(ii+1);
		}
	#ifdef USE_MOISTURE_SENSORS
	// условие срабатывания
	uint8_t cond = (schedule[i].cm >> 5) & 3;
	if (cond > 0) {
		out += " if";
		uint8_t sensors = schedule[i].cm & 31;
		if (sensors > 0)
			out += " s" + String(sensors);
		out += String(cond == 1? " < ": " > ") + String(schedule[i].cv) + "%";
	}
	#endif
	out += String(schedule[i].cm & 128 ? " On": " Off");
	return out;
}

// ────────────────────────────────────────────────────────────────────────────────
//   Общее меню как для команд из WEB (hub), так и из телеграм, или SMS
// ────────────────────────────────────────────────────────────────────────────────
String shared_menu(const String &text) {
    if (text == "/help") { // справка
		return
		#ifdef USE_GSM
		gs.active_channel == 4 ? ( // for SMS
			"/status\n"
			"8*n — switch to channel n\n"
			"/active — active channel\n"
			"1*n — water the plant n\n"
			"/pumps\n"
			"/help"
		) : 
		#endif
			(
			"/status - состояние устройства\n"
			"/schedule - расписание\n"
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
    }
	if (text == "/status") {
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
		String slaves = "";
		for(uint8_t i=0; i<MAX_SLAVES; i++) {
			if(slave[i].registered >= getTimeU() - gs.slave_timeout*60) {
				slaves += "\n/" + String(i) + " " + slave[i].hostname;
			}
		}
		return (
		#ifdef USE_GSM
			"GSM info: " + gsm.info + "\n"
			"Signal: "   + gsm.rssi + " dBm\n" +
		#endif
			power + "Uptime: " + uptime + slaves
		);
	}
	if (text == "/slave")
		return switchActiveChannel(ActiveChannel::hub);
	if (text == "/wifi")
		return switchActiveChannel(ActiveChannel::wifi);
	#ifdef USE_GSM
	if (text == "/gprs")
		return switchActiveChannel(ActiveChannel::gprs);
	if (text == "/sms")
		return switchActiveChannel(ActiveChannel::sms);
	if (text == "/unread_sms") {
		if (gsm.isSleep) gsm_wake();
		requestAllSMS();
		return "Все SMS прочитаны";
	}
	#endif
	if (text == "/active") {
		const char *ch[] = {"none", "hub", "telegram/WiFi", "telegram/GPRS", "SMS"};
		return "active channel: " + String(gs.active_channel) + " - " + ch[gs.active_channel];
	}
	if (text.startsWith("1*")) { // запуск насоса (1*n где n=0 - все насосы, n=X - насос номер X)
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
	}
	if (text.startsWith("8*")) { // переключение канала обратной связи (8*n где 0-нет, 1-hub, 2-telegram/web, 3-telegram/gprs, 4-SMS)
		int f = text.lastIndexOf("*");
		int t = text.indexOf("#");
		String pn = (t < 0) ? text.substring(f+1): text.substring(f,t);
		return switchActiveChannel(pn.length() < 1 ? 100: pn.toInt());
	}
	if (text == "/pumps") // отправка сообщения с текущим статусом полива
		return print_pumps_status();
	if (isDigit(text[0])) {
		// запрос внешнего датчика
		int8_t n = constrain(text.toInt(), 0, MAX_SLAVES-1);
		if(slave[n].registered >= getTimeU() - gs.slave_timeout*60) {
			String url = String("http://") + slave[n].ip.toString() + String("/api?pin=") + gs.slave_pin + "&";
			int pos = 1;
			int len = text.length();
			if(len<2) {
				url += "help";
			} else {
				for(; pos<len; pos++)
					if (text[pos] != ' ') break;
				int pos2 = text.indexOf("=");
				bool fl_free_cmd = text.indexOf(' ',pos) > 0 || text.indexOf('*',pos) > 0;
				if (!fl_free_cmd && pos2>0) {
					url += text.substring(pos,pos2+1);
					if(pos2+1 < len)
						url += urlEncode(text.substring(pos2+1), true);
				} else {
					if (fl_free_cmd || text[pos] == '/')
						url += F("cmd=");
					url += urlEncode(text.substring(pos));
					if (text[pos] != '/' && !fl_free_cmd)
						url += "=";
				}
			}
			LOG(println, url);
			WiFiClient client;
			HTTPClient html;
			html.begin(client, url.c_str());
			int httpResponseCode = html.GET();
			String res;
			if (httpResponseCode == 200) {
				// ответ от датчика запихивается сразу в telegram, обработку делает FastBot
				res = slave[n].hostname + "\n" + html.getString();
			} else {
				res = slave[n].hostname + " error: " + String(httpResponseCode);
			}
			// Free resources
			html.end();
			return res;
		} else {
			return "датчик неактивен";
		}
	}
	if (text == "/schedule") { // вывод текущего расписания
		String all_sh = "schedule:";
		for (uint8_t i=0; i<SCHEDULES; i++) {
			all_sh += print_schedule(i);
		}
		return all_sh;
	}
	if (text[0] == 'e' && isDigit(text[1]) ) { // изменение конкретного номера расписания
		int currentPos = 0;
		String token;
		Schedule_State *ss = nullptr;
		uint8_t active = 0;
		bool is_init = false; // защита от смены номера расписания при многократном вхождении "eN"
		bool is_init_pumps = false;
		// Разбиваем строку на токены по пробелам
    	while (currentPos < text.length()) {
        	int spacePos = text.indexOf(' ', currentPos);
			if (spacePos == -1) { // строка закончилась, возможный последний параметр
				token = text.substring(currentPos);
				currentPos = text.length();
			} else { // вырезаем из строки один параметр и обрезаем на него строку
				token = text.substring(currentPos, spacePos);
				currentPos = spacePos + 1;
			}

			if (token.length() == 0) continue; // повторные пробелы

			LOG(printf, "\"%s\" ", token.c_str());

			// разбор параметров вида буква+цифра
			if (token.length() >= 2 && isAlpha(token[0]) && isDigit(token[1])) {
				char p = token[0];
				int v = token.substring(1).toInt();
				// значение получено, определяем куда его надо вставить
				if (p == 'e' && !is_init) { // это номер расписания
					if (v < 1 || v > SCHEDULES) return "wrong schedule number";
					ss = &schedule[v-1];
					is_init = true;
					active = v-1;
				} else {
					if (is_init) {
						if (p=='p') { // это номер помпы 
							if (v < 1 || v > PUMPS) continue; // параметр некорректен, пропуск
							if (!is_init_pumps) { // есть параметр с насосом. Значит обнуляем текущее значение
								ss->s = 0;
								is_init_pumps = true;
							}
							if (v) ss->s |= 1<<(v-1); 
						} else if (p=='s') {
							if (v > SENSORS) continue;
							ss->cm &= ~(31U);
							ss->cm |= (v & 31); // max 31
						} else if (p=='x') {
							ss->p = constrain(v, 1, 255);
						}
					}
				}
			} else { // разбор остальных параметров со свободным форматом
				if (is_init) {
					if (token.indexOf(':') != -1) {
						if (token.startsWith("(")) { // время начала
							ss->r = decode_time(token.substring(1));
						} else { // период повтора
							ss->t = decode_time(token);
						}
					} else if (token.startsWith("p")) { // все насосы
						ss->s = 0;
						for (uint8_t i=0; i<PUMPS; i++) ss->s |= 1<<i;
					} else if (token == "s") { // сенсор без указания номера - среднее
						ss->cm &= ~(31U);
					} else if (token == "<") {
						ss->cm &= ~(3 << 5);
						ss->cm |= 1 << 5;
					} else if (token == ">") {
						ss->cm &= ~(3 << 5);
						ss->cm |= 2 << 5;
					} else if (token == "=" || token.startsWith("noif")) { // отключить условие
						ss->cm &= ~(3 << 5);
					} else if (token.startsWith("on")) {
						ss->cm |= 128U;
					} else if (token.startsWith("off")) {
						ss->cm &= ~(128U);
					} else if (token.indexOf('%') != -1) {
						ss->cv = constrain(token.toInt(), 0, 100);
					}
				}
			}
		}
		save_schedules();
		return "save:" + print_schedule(active);
	}
	return "unknow command";
}

#endif