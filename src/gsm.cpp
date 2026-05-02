/*
	Файл с определением основной логики приёма/отправки сообщений.
*/
#include <Arduino.h>
#include <SSLClient.h>
#include "defines.h"
#include "slave.h"
#include "ntp.h"
#include "TelegramAPI.hpp"
#include <WiFi.h>
#include "cert.h"
#include "gsm.h"

#define MODEM_UART_BAUD 115200

#define QUEUE_SIZE 20
QueueHandle_t strQueue = xQueueCreate(QUEUE_SIZE, sizeof(char*));
char* received = nullptr;

// Отправка сообщений (постановка в очередь)
bool sendMessage(const char* text) {          // или const String&
    if (text == nullptr) return false;

    size_t len = strlen(text) + 1;
    char* copy = (char*)malloc(len);   // или  malloc(len); / pvPortMalloc(len);
    if (copy == nullptr) return false;

    strcpy(copy, text);

    if (xQueueSend(strQueue, &copy, 0) != pdPASS) {
        free(copy);                           // не удалось отправить — освобождаем free(copy) / vPortFree(copy);
		return false;
    }
	return true;
}
// Отправка сообщений (постановка в очередь)
bool sendMessage(const String& text) {
	return sendMessage(text.c_str());
}

GSM_Info gsm; // статус модема

struct SMS_Queue {
	String txt = ""; // текст который надо отправить
	bool active = false; // флаг того, что sms ещё не отправлена
};
SMS_Queue sms_q;

timerMinim gsmLazyTimer(10000); // таймер для отсчёта промежутков по 10 секунд, для проверки состояния модема
timerMinim gsmSleepTimer(30000); // таймер для отсчёта времени до засыпания

#ifdef USE_GSM

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>  // должен быть после указания, какой тип модема используется


// Layers stack
TinyGsm modem(gsmSerial);
TinyGsmClient gsmTransportLayer(modem);

#endif

WiFiClient webTransportLayer;
// SSLClient securePresentationLayer(&gsmTransportLayer);
SSLClient securePresentationLayer(&webTransportLayer); // транспорт по умолчанию wifi, но потом он меняется вместе с каналом

TelegramAPI tg(securePresentationLayer);


bool fl_gprs = false; // отправка сообщений через gprs
bool fl_call = false; // входящее сообщение. Звонок или SMS
bool fl_dtmf = false; // звонок активен, ожидаются dtmf команды
unsigned long callStart = 0;
time_t last_telegram = 0;
time_t disable_telegram = 0;
int pinned = -1;

// Function prototypes
bool checkConnection();
String handleMessage(TResult &t);

#include "gsmApi.h"  // должен быть после определения modem

// проверка, работает ли активный канал: gprs или wifi
bool checkConnection() {
	#ifdef USE_GSM
		if( fl_gprs ) return gprs_check();
	#else
		if( fl_gprs ) return false;
	#endif
	return WiFi.status() == WL_CONNECTED;
}

// инициализация телеграм бота
void setup_telegram() {
	tg.setBotToken(gs.tb_token);
	tg.setChatID(extractFirstNumber(gs.tb_chats).toInt());
	tg.attachCallback(handleMessage);
	tg.attachCheckConnection(checkConnection);
	tg.setInterval(gs.tb_accelerated);
	last_telegram = getTimeU();
}

// инициализация задачи. Тоже, что setup() для основной задачи Arduino
void setup2() {
	Serial.println("Setup2");

	#ifdef USE_GSM

	// Set SIM module baud rate and UART pins
	gsmSerial.begin(MODEM_UART_BAUD, SERIAL_8N1, PIN_gsmRX, PIN_gsmTX);

	// securePresentationLayer.setCACert(TELEGRAM_CA_CERT);

	// Настройка/инициализация PIN на работу с модемом
	LOG(println, "Setup modem");
	pinMode(PIN_gsmRST, OUTPUT);
	// RST
	pinMode(PIN_gsmDTR, OUTPUT);
	digitalWrite(PIN_gsmDTR, LOW);
	// RING
	pinMode(PIN_gsmRING, INPUT);
	// Reset pin high
	digitalWrite(PIN_gsmRST, HIGH);

	// // Initialize SIM800. Как показала практика hard reset 99% не нужен.
	// LOG(println, "Initializing modem...");
	// while( !modem.begin() ) {
	// 	LOG(println, "hard reset :(");
	// 	hardResetModem();
	// }

	#endif

	setup_telegram();
	sendMessage("hello :) " + gs.host_name);

	LOG(println, "Started!");
}

// Обработка сообщений как входящих, так и исходящих
void messagePool() {
	// сначала обработка входящих
	// затем проверка очереди сообщений на отправку
	// но в начале надо поставить из основной очереди в кеш сообщение для отправки, если есть
	if (!sms_q.active && xQueueReceive(strQueue, &received, 0) == pdPASS) {
		if (received) {
			vTaskDelay(10);
			LOG(printf, "To send: %s\n", received);
			sms_q.txt = String(received);
			sms_q.active = true;
			free(received);               // ОБЯЗАТЕЛЬНО освобождаем! free() / vPortFree(received)
		}
	}
	// проверка, не закончилось ли время бана при сбое опроса телеграм
	if(disable_telegram && getTimeU() - disable_telegram > gs.tb_ban) disable_telegram = 0;

	#ifdef USE_GSM

	// ожидание подключения модема. Он может быть просто выключен.
	if ( ! gsm.isInit ) {
		if( gsmLazyTimer.isReady() ) { // попытка инициализации модема
			if( digitalRead(PIN_gsmRING) == HIGH ) { // возможно модем подключен
				LOG(println, "GSM init");
				// if( gsm_check() ) gsm_present = true; // модем отвечает, работает
				if( modem.begin() ) {
					LOG(println, "GSM init ok");
					gsm.isInit = true;
					gsm.isSleep = false;
					modem.sleepEnable(true);
					gsmSleepTimer.reset();
				}
			}
		}
	}
	// LOG(print, gsm.isInit);
	// секция ожидания чего-то входящего от GSM. Это может быть или звонок или SMS, для всего остального инициатор микроконтроллер
	if (gsm.isInit) {
		if ( gsm.isSleep && digitalRead(PIN_gsmRING) == LOW ) {
			LOG(println, "DTR LOW");
			// похоже пришол звонок. будим модем если надо и уходим на новый цикл в надежде поймать RING
			gsm_wake();
			fl_call = true;
			callStart = millis();
			return; // выход после каждого блока для сброса watchdog timer
		}
		if ( gsmSerial.available() > 0 ) {
			LOG(println, "Serial available");
			// в этом месте данные могут появится только если пришел звонок. Или случайный мусор.
			fl_call = true;
			callStart = millis();
			// в этом месте могут появится только данные, которые не касаются инициализации, настроек, работы GPRS.
			// по этому надо прочесть всю строку, чтобы понять, что хотел сказать модем
			// и попутно ставится флаг fl_call для блокировки обращений к gprs
			checkGsm();
			gsmSleepTimer.reset();
			return;
		}
		if (gs.active_channel == gprs ) { // канал через GSM/GPRS
			if (!disable_telegram) { 
				if (!fl_call && telegramTimer.isReady()) { // пришло время проверить сообщения в телеграм
					LOG(println, "new message request");
					if (gsm.isSleep) gsm_wake();
					if (tg.checkMessage(true) < 0 ) disable_telegram = getTimeU();
					gsmSleepTimer.reset();
					return;
				}
				if (sms_q.active) {
					vTaskDelay(30);
					if (tg.sendMessage(sms_q.txt)) sms_q.active = false;
				}
			}
		}
		if (gs.active_channel == sms && sms_q.active) { // канал SMS, только отправка, работает на приём нестабильно
			LOG(printf, "number for send: %s", extractFirstNumber(gs.sms_phone).c_str());
			if( modem.sendSMS(extractFirstNumber(gs.sms_phone), sms_q.txt) ) sms_q.active = false;
			gsmSleepTimer.reset();
		}
		// если модем не спит, ничем не занят, и прошло 10 секунд, то обновить информацию о модеме
		if( !fl_dtmf && !fl_call && ! gsm.isSleep && gsmLazyTimer.isReady() ) {
			gsm.info = modem.getModemInfo();
			gsm.rssi = modem.getSignalQuality();
			// SIM800RegStatus gsm_registrationStatus
			gsm.status = modem.getRegistrationStatus(); // 1 - ok, остальное ошибки

			if( gsmSleepTimer.isReady() ) { // пришло время спать
				digitalWrite(PIN_gsmDTR, HIGH);
				gsm.isSleep = true;
				LOG(println, "Sleep time");
			}
		}

		if ( !fl_dtmf && millis() - callStart > 10000L ) fl_call = false;
	}
	#endif

	// секция опроса новых сообщений из не GSM
	if (!gs.active_channel) return; // нет канала для уведомлений
	if (gs.active_channel == wifi) { // канал через wifi
		if (!disable_telegram) { 
			if (telegramTimer.isReady())
				if (tg.checkMessage(true) < 0 ) disable_telegram = getTimeU();
			if (sms_q.active) {
				vTaskDelay(30);
				if (tg.sendMessage(sms_q.txt)) sms_q.active = false;
			}
		}
	}
	if (gs.active_channel == hub) { // канал через wifi и hub
		// только отправка сообщений. Приём через web.cpp / slave.cpp
		if (sms_q.active)
			if (tb_send_msg(sms_q.txt)) sms_q.active = false;
	}
	// проверка времени ускорения работы telegram
	if (last_telegram > 0 && getTimeU() - last_telegram > gs.tb_accelerate) {
		LOG(printf, "disable telegram accelerate (last_telegram=%u, now=%u)\n", last_telegram, getTimeU());
		telegramTimer.setInterval(1000U * gs.tb_rate);
		last_telegram = 0;
	}
}

// ────────────────────────────────────────────────────────────────────────────────
//   Обработка поступивших вхядящих команд из разных источников
// ────────────────────────────────────────────────────────────────────────────────

// переключение активного канала
String switchActiveChannel(uint8_t ch) {
	String result;
	switch (ch) {
		case ActiveChannel::none: // ничего не отсылать
			break;
		case ActiveChannel::hub: // отправка через Hub
			if ( WiFi.status() != WL_CONNECTED ) {
				result = "Не получилось переключится: WiFi отключен";
			} else {
				securePresentationLayer.setClient(&webTransportLayer);
				fl_gprs = false;
				gs.active_channel = hub;
				result = "Канал переключен на Hub";
			}
			break;
		case ActiveChannel::wifi: // отправка через telegram и WiFi
			if ( WiFi.status() != WL_CONNECTED ) {
				result = "Не получилось переключится: WiFi отключен";
			} else {
				securePresentationLayer.setClient(&webTransportLayer);
				fl_gprs = false;
				gs.active_channel = wifi;
				result = "Канал переключен на WiFi";
			}
			break;
		#ifdef USE_GSM
		case ActiveChannel::gprs: // отправка через telegram и GPRS
			if (gsm.isSleep) gsm_wake();
			if ( ! gprs_check() ) {
				result = "Не получилось переключится: GPRS отключен";
			} else {
				securePresentationLayer.setClient(&gsmTransportLayer);
				fl_gprs = true;
				gs.active_channel = gprs;
				result = "Канал переключен на GPRS";
			}
			break;
		case ActiveChannel::sms: // отправка через SMS
			if (gsm.isSleep) gsm_wake();
			if ( ! gsm_check() ) {
				result = "Не получилось переключится: GSM отключен";
			} else {
				securePresentationLayer.setClient(&gsmTransportLayer);
				fl_gprs = true;
				gs.active_channel = sms;
				result = "Channel was switched to SMS";
			}
			break;
		#endif
		default:
			result = (
				"unknown channel\n"
				"0 - none\n"
				"1 - Hub\n"
				"2 - telegram/WiFi\n"
				#ifdef USE_GSM
				"3 - telegram/GPRS\n"
				"4 - SMS"
				#endif
			);
	}
	LOG(println, result);
	return result;
}

// ────────────────────────────────────────────────────────────────────────────────
//  Обработчик команд пришедших из телеграм.
//  Туда и ответы сразу отсылаются, в обход очереди
//  Но так как и запросы и ответы идут в одном потоке, то конфликта быть не должно
// ────────────────────────────────────────────────────────────────────────────────
String handleMessage(TResult &t) {
	// новое сообщение включает ускоренный опрос новых сообщений
	if(gs.tb_rate > gs.tb_accelerated) {
		telegramTimer.setInterval(1000U * gs.tb_accelerated);
		LOG(println, "enable telegram accelerate");
	}
	last_telegram = getTimeU();
	LOG(printf, "last_telegram=%u\n", last_telegram);


	t.text.toLowerCase();

	if (t.text.charAt(0) == '/' && isDigit(t.text.charAt(1))) { //} >= '0' && t.text.charAt(1) <= '9') {
		String n = "pin " + t.text.substring(1);
		t.text = n;
		LOG(println, "rewrite to: " + t.text);
	}

	if (t.text.endsWith("unpin") || t.text == "/") {
		pinned = -1;
		return "Welcome back to";
	}
	if (t.text.startsWith("pin")) {
		int n = constrain(t.text.substring(t.text.lastIndexOf(" ")).toInt(), 0, MAX_SLAVES-1);
		if (slave[n].registered >= getTimeU() - gs.slave_timeout*60) {
			pinned = n;
			return "Pinned " + String(pinned) + "\n/Unpin for back\n/Help";
		} else
			return "format:\npin n\nn=0..9";
	}
	if (t.text == "/start") {
		return (
			"Hi, " + t.from + "!\n\n"
			"/chatid - show ChatID\n"
			"/help - show help"
			);
	}
	if (t.text == "/chatid")
		return "ChatID: " + String(t.chatId);

	if (pinned >= 0 && (! isDigit(t.text[0] || t.text.indexOf("*") > 0 ))) {
		String n = String(pinned) + " " + t.text;
		t.text = n;
		LOG(println, "rewrite to: " + t.text);
	}

	if (t.text.length() > 0 && isWhitelisted(String(t.chatId), gs.tb_chats))
		return shared_menu(t.text);

	if (t.text == "/help")
		return "for registered users only";

	return "Эхо: " + t.text;
}

#include "menu.h" // такое странное место, потому что не смог по человечестки рапределить ресурсы