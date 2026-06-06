#ifndef DEFINE_VARS_H
#define DEFINE_VARS_H

// определение глобальных переменных

/*
Каналы для отправки сообщений:
0 - none - полностью автономная работа
1 - hub - отправка на мастер устройство найденное через mdns (web)
2 - wifi - отправка через телеграм и wifi
3 - gprs - отправка через телеграм и gprs
4 - sms - отпраавка напрямую через gsm-sms
*/
enum ActiveChannel {
	none,	// - полностью автономная работа
	hub,	// - отправка на мастер устройство найденное через mdns
	wifi,	// - отправка через телеграм и wifi
	gprs,	// - отправка через телеграм и gprs
	sms		// - отпраавка напрямую через gsm-sms
};

struct Global_Settings {
	String host_name = "water"; // название устройства в локальной сети, по умолчанию.
	uint8_t blink_g = 1; // моргать зелёным светодиодом (heartbeat)

	int8_t tz_shift = TIMEZONE; // временная зона, смещение локального времени относительно Гринвича (часы)
	uint8_t tz_dst = DSTSHIFT; // смещение летнего времени (часы)
	uint8_t sync_time_period = 8; // периодичность синхронизации ntp в часах

	uint16_t low_v = 2000; // значение напряжения при 2.8V, напряжение отключения микроконтроллера
	uint16_t high_v = 4095; // значение напряжения при 4.2V, полный заряд (4.25V выдаёт контроллер заряда)

	// uint16_t msec_in_ml = 1000; // количество миллисекунд необходимых для набора одного миллилитра
	uint8_t doze = 30; // количество миллилитров в одной порции

	String hub_name = "clock"; // название хаба, на котором надо регистрироваться
	String hub_pin = "5528"; // пин для подключения к хабу
	uint8_t hub_period = 10; // период обновления регистрации (keepalive)
	String slave_pin = "def555"; // пин-код доступа для подключения подчинённых устройств
	uint16_t slave_timeout = 20; // время в течении которого подчинённый считается действующим, в минутах

	String tb_name = ""; // имя бота, адрес. Свободная строка, только для справки
	String tb_chats = ""; // чаты из которых разрешено принимать команды
	String tb_token = ""; // API токен бота
	uint16_t tb_rate = 300; // интервал запроса новых команд в секундах
	uint16_t tb_accelerated = TELEGRAM_ACCELERATED; // ускоренный интервал запроса новых команд в секундах
	uint16_t tb_accelerate = TELEGRAM_ACCELERATE; // время в течении которого будет работать ускорение
	uint16_t tb_ban = TELEGRAM_BAN; // время на которе прекращается опрос новых сообщений, после сбоя, в секундах

	uint8_t active_channel = ActiveChannel::hub; // канал отправки сообщений
	String sms_phone = ""; // номера телефона, откуда принимать звонки и команды, первый номер - куда отправлять sms
	String pin_code = ""; // пин-код для разблокировки SIM. Но лучше его просто отключить.
	String gprs_APN = "Internet"; // адрес APN, сейчас почти всегда Internet
	String gprs_user = ""; // логин для подключения к GPRS, сейчас почти всегда пустой
	String gprs_pass = ""; // пароль для подключения к GPRS, сейчас почти всегда пустой

	String web_login = "admin"; // логин для вэб
	String web_password = ""; // пароль для вэб
};
extern Global_Settings gs;

struct Moisture_Calibrate {
	uint16_t moi0 = 950;
	uint16_t moi100 = 2500;
};
extern Moisture_Calibrate mc[];

struct Pump_Calibrate {
	uint8_t pump = 1; // номер насоса использованный для теста
	uint8_t sec1 = 5; // время первого теста
	float tara = 10.0f; // вес стаканчика
	float weight1 = 50.0f; // вес воды первого теста
	float weight2 = 100.0f; // вес воды второго теста
	uint16_t in_ms = 200; // время за которое прокачивается один грамм (миллилитр) воды в миллисекундах
	uint16_t empty_ms = 200; // время наполнения подводящей трубки
};
extern Pump_Calibrate pc;

struct Pump_Queue {
	uint8_t seconds = 0; // необходимое число секунд
	uint8_t need = 1; // необходимое число порций
	bool active = false; // нужен запуск
};
extern Pump_Queue pq[];

struct Log_State {
	uint8_t enable = 0; // писать события в журнал
	uint8_t curFile = 0; // порядковый номер файла журнала
};
extern Log_State ls;

struct Pump_State {
	float vol = 0.0f; // объём залитой воды
	uint32_t count = 0; // количество запусков.
	time_t last = 0; // последний запуск
};
extern Pump_State ps[];

struct Schedule_State {
	uint8_t s = 0; // выбранные насосы (один бит на насос)
	uint16_t t = 0; // время в минутах от полуночи
	uint16_t r = 0; // минуты до повтора
	uint8_t cm = 0; // условия срабатывания
	uint8_t cv = 60; // значение условия срабатывания
	uint8_t p = 1; // количество порций за одно срабатывание
	time_t next = 0; // время по utc, в которое таймер должен сработать.
};
extern Schedule_State schedule[];

struct Average {
	uint16_t raw = 0; // текущая влажность
	uint8_t per = 0; // текущая влажность в процентах
	uint32_t sum = 0; // накопитель для вычисления среднего значения влажности
};
extern Average moi[];
extern Average battery;

extern esp_chip_info_t chip_info;
extern SemaphoreHandle_t xMutex;
extern bool fs_isStarted;
extern bool fl_5v;
extern bool wifi_isConnected;
extern bool wifi_isPortal;
extern bool fl_timeNotSync;
extern bool ftp_isAllow;
extern bool fl_AHTIsInit;
extern time_t last_telegram;

struct GSM_Info {
	String info = "";
	int16_t rssi = 0;
	int8_t status = -1;
	bool isInit = false;
	bool isSleep = false;
	bool gprsStart = false;
};
extern GSM_Info gsm;

#define PUMPS sizeof(pumpPIN) // количество помп
#ifdef USE_MOISTURE_SENSORS
#define SENSORS sizeof(sensorPIN) // количество сенсоров
#else
#define SENSORS 0
#endif

#include <IPAddress.h>
struct cur_slave {
	String hostname;
	IPAddress ip = {0,0,0,0}; // IPADDR_NONE;
	time_t registered = 0;
};
extern cur_slave slave[];

//----------------------------------------------------

#include <TimerMinim.h>

extern TimerMinim ntpSyncTimer;
extern TimerMinim hubRegTimer;
extern TimerMinim telegramTimer;

#include <BlinkMinim.h>

extern BlinkMinim led;

//----------------------------------------------------
#if defined(LOG)
#undef LOG
#endif

#ifdef DEBUG
	//#define LOG                   Serial
	#define LOG(func, ...) Serial.func(__VA_ARGS__)
#else
	#define LOG(func, ...) ;
#endif


#endif