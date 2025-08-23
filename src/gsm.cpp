// работа с GSM модулем

#include <Arduino.h>
#include <SSLClient.h>
#include "defines.h"

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>

#define GSM_UART_BAUD 115200
// Your GPRS credentials (leave empty, if missing)
const char apn[] = "Internet";  // Your APN
const char gprs_user[] = "";    // User
const char gprs_pass[] = "";    // Password
const char simPIN[] = "";       // SIM card PIN code, if any

// Layers stack
TinyGsm modem(gsmSerial);
TinyGsmClient gsmTransportLayer(modem);
SSLClient securePresentationLayer(&gsmTransportLayer);

// * Certificate bundle with 41 most used root certificates
extern const uint8_t ca_cert_bundle_start[] asm("_binary_crt_x509_crt_bundle_bin_start");
extern const uint8_t ca_cert_bundle_end[] asm("_binary_crt_x509_crt_bundle_bin_end");

GSM_Info gsm; // статус модема

struct SMS_Queue {
	String txt = ""; // текст который надо отправить
	bool active = false; // флаг того, что sms ещё не отправлена
};
SMS_Queue sms_q;

// Функция для проверки соединения с модулем
bool gsm_check() {
	LOG(print, "Проверка связи с модулем SIM800... ");
  
	// modem.testAT() отправляет команду "AT" и ждет ответа "OK"
	// Она возвращает true, если ответ получен, и false в противном случае.
	// Второй аргумент (1000) - это таймаут в миллисекундах.
	if (modem.testAT(1000)) {
		LOG(println, "Успех! Модуль GSM найден и отвечает.");
		return true;
	} else {
		LOG(println, "Провал! Модуль не найден или не отвечает.");
		return false;
	}
	// String date_time = modem.getGSMDateTime(DATE_FULL);
	// modem.getNetworkTime(int *year, int *month, int *day, int *hour, int *minute, int *second, float *timezone);
	// modem.getNetworkUTCTime(int *year, int *month, int *day, int *hour, int *minute, int *second, float *timezone);
	// modem.isGprsConnected();
	// modem.isNetworkConnected();
	// modem.sendAT("+CMGF=1", "+CMGL=\"ALL\""); // перевести модем в текстовый режим, запросить список всех sms
	// modem.sendAT("+CMGR=1"); // прочитать sms #1
	// modem.sendAT("+CMGD=1"); // удалить sms #1
/*	
	<stat> integer type in PDU mode (default 0), or string type in text mode (default
	"REC UNREAD"); indicates the status of message in memory; defined values:
	0 "REC UNREAD"   received unread message (i.e. new message)
	1 "REC READ"     received read message
	2 "STO UNSENT"   stored unsent message (only applicable to SMs)
	3 "STO SENT"     stored sent message (only applicable to SMs)
	4 "ALL"          all messages (only applicable to +CMGL command)
*/
}

void gsm_begin() {
	// Set SIM module baud rate and UART pins
	gsmSerial.begin(GSM_UART_BAUD, SERIAL_8N1, PIN_gsmRX, PIN_gsmTX);

	// Set certificate bundle to SSL client
	securePresentationLayer.setCACertBundle(ca_cert_bundle_start);

	// пин аппаратной перезагрузки (hard reset) low на 100мс
	pinMode(PIN_gsmRST, OUTPUT);
	// Reset pin high
	digitalWrite(PIN_gsmRST, HIGH);
	
	// пин для контроля режима сна. high = sleep. После просыпания за 50мс надо опять загружать сертификаты, 
	pinMode(PIN_gsmDTR, OUTPUT);
	digitalWrite(PIN_gsmDTR, LOW);

	// пин по которому приходит сигнал вызова low на 120vc, можно использовать как сигнал пробуждения платы от сна
	pinMode(PIN_gsmRING, INPUT);
}

bool gsm_init() {

	return false;
}

void gsm_sendSMS(const String txt) {
	sms_q.txt = txt;
	sms_q.active = true;
}

timerMinim gsmLazyTimer(10000); // таймер для отсчёта промежутков по 10 секунд, для проверки состояния модема
timerMinim gsmSleepTimer(30000); // таймер для отсчёта времени до засыпания

// эти функции работают в отдельном процессе на другом ядре, чтобы не тормозить основной блок
void gsm_pool() {
	if( ! gsm.isInit && gsmLazyTimer.isReady() ) { // попытка инициализации модема
		if( digitalRead(PIN_gsmRING) == HIGH ) { // возможно модем подключен
			// if( gsm_check() ) gsm_present = true; // модем отвечает, работает
			if( modem.begin() ) {
				gsm.isInit = true;
				modem.sleepEnable(true);
				gsmSleepTimer.reset();
			}
		}
	} else {
		// модем модем подключен, проверка очереди
		if( sms_q.active ) {
			if( gsm.isSleep ) {
				digitalWrite(PIN_gsmDTR, LOW);
				gsm.isSleep = false;
				delay(60);
				securePresentationLayer.setCACertBundle(ca_cert_bundle_start);
			}
			if( ! modem.isNetworkConnected() ) return;
			if( modem.sendSMS(gs.sms_phone, sms_q.txt) ) sms_q.active = false;
			gsmSleepTimer.reset();
		}
		// если модем не спит, и прошло 10 секунд, то обновить информацию о модеме
		if( ! gsm.isSleep && gsmLazyTimer.isReady() ) {
			gsm.info = modem.getModemInfo();
			gsm.rssi = modem.getSignalQuality();
			// SIM800RegStatus gsm_registrationStatus
			gsm.status = modem.getRegistrationStatus(); // 1 - ok, остальное ошибки

			if( gsmSleepTimer.isReady() ) { // пришло время спать
				digitalWrite(PIN_gsmDTR, HIGH);
				gsm.isSleep = true;
			}
		}
	}
}
