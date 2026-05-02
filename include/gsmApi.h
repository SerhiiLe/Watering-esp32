/*
    Файл с основным набором функций для работы с платой sim800l.
    Это не полноценный API, просто сборник функций, чтобы разгрузить файлы с логикой
*/
#ifndef GSMAPI_H
#define GSMAPI_H

#include <Arduino.h>

// ═════════════════════════════════════════════════════════════════════════════
//  Вспомогательные функции не относящиеся напрямую к GSM модулю
// ═════════════════════════════════════════════════════════════════════════════

// вырезание значения в кавычках из полученной строки
String extractQuoted(const String& line) {
  int q1 = line.indexOf('"'), q2 = line.indexOf('"', q1 + 1);
  return (q1 < 0 || q2 < 0) ? "" : line.substring(q1 + 1, q2);
}

// сравнение номера с номером для отправки SMS. Только от него принимать звонки.
// Можно указать несколько номеров через запятую, отправка sms только на первый.
bool isWhitelisted(const String& n, const String& from) {
    // простая, но не точная реализация
	return (n.length() > 0 && from.length() > 0 && from.indexOf(n) >= 0);

    // // полная, точная реализация
	// int f=0, t=0;
	// String number = "";
	// while(1) {
	// 	t = from.indexOf(',', f);
	// 	if ( t < 0 ) {
	// 		number = from.substring(f);
	// 		number.trim();
	// 		return (n == number);
	// 	}
	// 	String number = from.substring(f,t);
	// 	number.trim();
	// 	if (n == number) return true;
	// 	f = t+1;
    // }
}

// только первый номер телефона
String extractFirstNumber(const String& from) {
    int t = from.indexOf(',');
    if (t<0) return from;
    String number = from.substring(0,t);
    number.trim();
    return number;
}

// номер индекса sms (целое после последней запятой)
int extractIndex(const String& line) {
  int c = line.lastIndexOf(',');
  return c < 0 ? -1 : line.substring(c + 1).toInt();
}

#ifdef USE_GSM

// ═════════════════════════════════════════════════════════════════════════════
//  Базовые функции работы с GSM модулем
// ═════════════════════════════════════════════════════════════════════════════

#include <pdulib.h>

PDU pdu;

String dtmfBuffer = "";

// Проверка, подключен ли модем к сети
bool gsm_check() {
	// проверка, подключен ли модем к сети
	if( modem.isNetworkConnected() ) return true;
	// не подключено, что странно, модем не отключается от сети в режиме сна, наверное модем был не инициализирован
	// modem.restart();
	if (gs.pin_code.length() > 0 && modem.getSimStatus() != 3) { modem.simUnlock(gs.pin_code.c_str()); } // разблокировка SIM если стоит pin-код
	// delay(200); // небольшая пауза. В общем то она не нужна, так как будет ожидание на следующем этапе
	for (int i=0; i<10; i++) {
		if( !modem.waitForNetwork(500) ) {
			LOG(println, "Failed to connect to GSM network");
			vTaskDelay(100);
			continue;
		} else return true;
	}
	return false;
}

// Проверка подключен ли gprs
bool gprs_check() {
	// проверка, а вообще сеть подключена?
	if( !gsm_check() ) return false;
	// проверка подключен ли gprs
	if( modem.isGprsConnected() ) return true;
	// не подключено, тоже странно, gprs не должен отключаться в режиме сна
	// но могли закончится деньги!
	if (!modem.gprsConnect(gs.gprs_APN.c_str(), gs.gprs_user.c_str(), gs.gprs_pass.c_str())) {
		LOG(println, "Failed to connect to MODEM_APN (GPRS)");
		return false;
	}
	// securePresentationLayer.setCACertBundle(ca_cert_bundle_start);
	securePresentationLayer.setCACert(TELEGRAM_CA_CERT);
	return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  RESET / SLEEP / WAKE
// ═════════════════════════════════════════════════════════════════════════════
void hardResetModem() {
	LOG(println, "[RST GSM] ...");
	digitalWrite(PIN_gsmRST, LOW);  vTaskDelay(200);
	digitalWrite(PIN_gsmRST, HIGH); vTaskDelay(3000);
	gsmSerial.flush();
}

void gsm_wake() {
	if (gsm.isInit && gsm.isSleep) {
		// плата спит, пробуем разбудить
		digitalWrite(PIN_gsmDTR, LOW);
		gsm.isSleep = false;
		vTaskDelay(60); // время за которое плата должна проснуться, согласно документации
		// securePresentationLayer.setCACertBundle(ca_cert_bundle_start);
		securePresentationLayer.setCACert(TELEGRAM_CA_CERT);
		gsmSleepTimer.reset();
		LOG(println, "wake!");
	}
}

// ═════════════════════════════════════════════════════════════════════════════
//  SMS
// ═════════════════════════════════════════════════════════════════════════════

void deleteSMS(uint8_t index) {
    LOG(printf, "Удаление SMS %i\n", index);
    LOG(println, "+CMGD=" + String(index));
    modem.sendAT("+CMGD=" + String(index)); // удаление SMS
    modem.waitResponse();
}

void decodeSMS(const char* pduLine) {
    if (pdu.decodePDU(pduLine)) {
        LOG(println, "┌─── SMS ─────────────────────────");
        LOG(printf,  "│ От    : %s\n", pdu.getSender());
        LOG(printf,  "│ Время : %s\n", pdu.getTimeStamp());
        LOG(printf,  "│ Текст : %s\n", pdu.getText());
        LOG(println, "└─────────────────────────────────");

        char buff[300];
        sprintf(buff,"SMS\nОт: %s\nВремя: %s\nТекст: %s", pdu.getSender(), pdu.getTimeStamp(), pdu.getText());
        if (gs.active_channel != sms)
            sendMessage(buff);
        if (isWhitelisted(pdu.getSender(), gs.sms_phone))
            shared_menu(pdu.getText());

    } else
        LOG(println, "[SMS] Ошибка PDU");
}

void requestSMS(int index) {
    LOG(printf, "[SMS] Читаем idx=%d\n", index);
    modem.sendAT("+CMGF=0");
    modem.waitResponse();
    LOG(println, "+CMGR=" + String(index));
    modem.sendAT("+CMGR=" + String(index));
    modem.waitResponse(2000, "+CMGR:");
    String line = gsmSerial.readStringUntil('\n');
    LOG(println, line);
    line = gsmSerial.readStringUntil('\n');
    line.trim();
    LOG(println, line);
    modem.waitResponse();
    decodeSMS(line.c_str());
    LOG(println, "+CMGD=" + String(index));
    modem.sendAT("+CMGD=" + String(index)); // удаление SMS
    modem.waitResponse();
}

void requestAllSMS() {
    LOG(println, "SMS list");
    modem.sendAT("+CMGF=0");
    modem.waitResponse();
    modem.sendAT("+CMGL=4");
    String all_indexes = "";
    while (1) {
        int8_t st = modem.waitResponse(2000, "+CMGL:", "OK");
        if ( st == 1 ) {
            String line = gsmSerial.readStringUntil('\n');
            line.trim();
            LOG(println, line);
            all_indexes += line.substring(0,line.indexOf(',')+1);
            line = gsmSerial.readStringUntil('\n');
            line.trim();
            LOG(println, line);
            // тут могла быть команда декодирования SMS, но тогда не будет работать удаление прочитанной SMS
            // вместо этого сначала получаем все индексы сообщений, игнорируя сам текст сообщений
            // а потом читаем по одному, указывая полученные индексы
            // decodeSMS(line.c_str());
        } else break;
    }
    LOG(println, all_indexes);
    int f=0, t=0;
    while(1) {
        t = all_indexes.indexOf(',', f);
        if ( t < 0 ) break;
        requestSMS(all_indexes.substring(f,t).toInt());
        f = t+1;
    }
}

// Команды, которые можно отдать использую DTMF (цифры и *. Окончание и исполнение #)
// недостаток в том, что нет обратной связи. Генерируемый sim800l dtmf не передаётся на телефон.
// Этот функционал - последний вариант. Но может пригодится, если деньги закончились и надо переключить канал связи
void handleDtmf(const String& cmd) {
    LOG(printf, "DTMF command: %s\n", cmd.c_str());
    if (cmd == "0") { // 0#
        // завершение звонка
        modem.sendAT("H0");
        vTaskDelay(10);
        if (modem.waitResponse(2000) == 1) fl_call=false;
        LOG(println, "Завершение звонка");
        vTaskDelay(10);
    } else if (cmd.startsWith("1*") || cmd.startsWith("8*")) {
        // полив ... 1*(номер)# или переключить канал 8*(номер)#
        shared_menu(cmd);
    }
}

void checkGsm() {
    String line = gsmSerial.readStringUntil('\n');
    LOG(println, "GSM read:");
    LOG(println, line);
    // реакция на входящий звонок
    if (line.indexOf("RING") != -1) {
        LOG(println, "Входящий звонок, ждём определения номера");
        if ( modem.waitResponse(10000L, "+CLIP:") == 1 ) { // в течении 10 сек ждём номер звонящего
            line = gsmSerial.readStringUntil('\n'); // дочитываем строку до конца
            String callerNumber = extractQuoted(line);
            LOG(println, callerNumber);
            if (isWhitelisted(callerNumber, gs.sms_phone)) { // есть ли номер в белом списке?
                modem.sendAT("A");
                modem.waitResponse();
                // проигрывание приветствия в виде трёх тональных посылок. Но реально не работает
                modem.dtmfSend('A', 100);
                vTaskDelay(10);
                modem.dtmfSend('B', 100);
                vTaskDelay(10);
                modem.dtmfSend('C', 100);
                fl_dtmf = true;
                return;
            } else { 
                LOG(println, "[CALL] Не в списке → ATH");
                modem.sendAT("H");               // сброс немедленно, без задачи
                modem.waitResponse();
                callerNumber = "";
                fl_call = false;
                return;
            }
        }
    }
    // звонок завершен
    if (line.indexOf("NO CARRIER") != -1 || line.indexOf("BUSY") != -1 ) {
        LOG(println, "[CALL] Завершён");
        fl_dtmf = false;
        fl_call = false;
        return; 
    }
    // ── DTMF ─────────────────────────────────────────────────────────────────
    if (line.startsWith("+DTMF:")) {
        String digit = line.substring(line.lastIndexOf(' ') + 1);
        digit.trim();
        if ( digit == "#" ) {
            handleDtmf(dtmfBuffer);
            dtmfBuffer = "";
            return;
        }
        dtmfBuffer += digit;
        return;
    }
    // реакция на входящую sms
    if (line.indexOf("+CMTI:") != -1) {
        int idx = extractIndex(line);
        if (idx >= 0) {
            requestSMS(idx);
            LOG(printf, "[SMS] Очередь idx=%d\n", idx);
        }
    }

}

#endif // ifdef USE_GSM

#endif