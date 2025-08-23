/*
  Автомат для полива. С контролем влажности почвы.
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "defines.h"
#include "settings.h"
#include <EncButton.h> // https://github.com/GyverLibs/EncButton
#include "wifi_init.h"
#include "blinkMinim.h"
#include "ntp.h"
#include "web.h"
#include "slave.h"
#include "ftp.h"
#include "pump.h"
#include "gsm.h"

touch_value_t last_touch;

PumpWater p[PUMPS]; // управление насосами
Pump_Queue pq[PUMPS]; // очередь заданий на запуск насосов
Pump_State ps[PUMPS]; // состояние насосов и статистика

VirtButton btn;

blinkMinim led(PIN_LED, LOW);

timerMinim ntpSyncTimer(3600000U * gs.sync_time_period);  // Таймер синхронизации времени с NTP-сервером 3600000U
timerMinim hubRegTimer(60000U * gs.hub_period); // Таймер перерегистрации на hub (часах)
timerMinim ntpRetry(5000); // паузы для повторного опроса NTP сервера
timerMinim seconds(1000); // секундный интервал для разных целей
timerMinim telegramTimer(1000U * gs.tb_accelerated); // период опроса команд из Телеграм
timerMinim saveStateTimer(300000); // записывать состояние каждые 5 минут

// файловая система подключена
bool fs_isStarted = false;
// флаг требования сброса пароля
bool fl_password_reset_req = false;
// флаг наличия питания
bool fl_5v = true;
// напряжение на аккумуляторе
uint8_t battery = 0;
uint16_t battery_raw = 0;
// текущая влажность
uint16_t moi_raw[SENSORS] = {0};
// текущая влажность в процентах
uint8_t moi_per[SENSORS] = {0};
// накопитель для вычисления среднего значения влажности
uint32_t moi_tmp[SENSORS] = {0};
// счётчик пройденных циклов чтения влажности
uint16_t moi_count = 0;
// текущая минута
int a_minute = -1;
// флаг необходимости записи состояние на диск
bool fl_need_save_state = false;

// Function to connect to the network and GPRS
TaskHandle_t gsmTaskHandle = NULL;
static void TaskGSMCode( void * pvParameters );
esp_chip_info_t chip_info;

void setup() {
  	// put your setup code here, to run once:
	Serial.begin(115200);
	LOG(println, "system start");
	led.blink(ON, 50);
	// настройка pin для GSM модуля
	gsm_begin();
	// начальный вектор псевдослучайных чисел
	randomSeed(analogRead(A0)+analogRead(A0));
	// get the ESP32 chip information
	esp_chip_info(&chip_info);
	// подключение файловой системы
	if( LittleFS.begin() ) {
		if( LittleFS.exists("/index.html") ) {
			fs_isStarted = true; // встроенный диск подключился
			LOG(println, "LittleFS mounted");
		} else {
			LOG(println, "LittleFS is empty");
			led.blink(ON, 100);
		}
	} else {
		LOG(println, "ERROR LittleFS mount");
		led.blink(ON, 100);
	}
	// чтение/создание файлов конфигурации
	if(!load_config_main()) {
		LOG(println, "Create new config file");
		//  Создаем файл запив в него данные по умолчанию, при любой ошибке чтения
		save_config_main();
	}
	if(!load_moisture_calibration()) {
		LOG(println, "Create new moisture calibration file");
		//  Создаем файл запив в него данные по умолчанию, при любой ошибке чтения
		save_moisture_calibration();
	}
	if(!load_pump_calibration()) {
		LOG(println, "Create new pump calibration file");
		//  Создаем файл запив в него данные по умолчанию, при любой ошибке чтения
		save_pump_calibration();
	}
	if(!load_schedulers()) {
		LOG(println, "Create new schedulers file");
		//  Создаем файл запив в него данные по умолчанию, при любой ошибке чтения
		save_schedulers();
	}
	if(!load_pump_state()) {
		LOG(println, "Create new pump_state file");
		//  Создаем файл запив в него данные по умолчанию, при любой ошибке чтения
		save_pump_state();
	}

	// сброс очереди работы насосов
	for(uint8_t i=0; i<PUMPS; i++) {
		pq[i].active = false;
		pq[i].seconds = 0;
		pq[i].need = false;
	}

	// инициализация насосов
	for(uint8_t i=0; i<PUMPS; i++) {
		if( pumpPIN[i] <= 200 ) // реле подключено непосредственно к пину
			p[i].begin(pumpPIN[i], RELAY_LEVEL);
		else // заготовка, реле подключено через, к примеру, через сдвиговый регистр. надо указать функции работы с pin реле
			p[i].begin(pumpPIN[i]-200, RELAY_LEVEL, digitalWrite, digitalRead);
	}

	pinMode(PIN_5V, INPUT);

	last_touch = touchRead(PIN_BUTTON);

	// инициализация подключения к wifi (в фоновом режиме)
	wifi_setup();

	// Start the connection task
	xTaskCreatePinnedToCore(
		TaskGSMCode,         /* Function to implement the task */
		"TaskGSMCode",       /* Name of the task */
		10000,               /* Stack size in words */
		NULL,                /* Task input parameter */
		0,                   /* Priority of the task */
		&gsmTaskHandle,      /* Task handle. */
		0);                  /* Core where the task should run */
}

void loop() {
  	// put your main code here, to run repeatedly:

	// if( blink.isReady() ) digitalWrite(LED, !digitalRead(LED));
	led.tick();

	// текущее состояние сенсорной кнопки (виртуальная кнопка)
	btn.tick(touchRead(PIN_BUTTON) < TOUCH_THRESHOLD ? true: false);

	// текущее состояние насосов
	for(uint8_t i=0; i<PUMPS; i++) p[i].tick();

	wifi_process();
	if( wifi_isConnected ) {
		// установка времени по ntp.
		if( fl_timeNotSync )
			// первичная установка времени. Если по каким-то причинам опрос не удался, повторять не чаще, чем раз в секунду.
			if( ntpRetry.isReady() ) syncTime();
		if(ntpSyncTimer.isReady()) // это плановая синхронизация, не критично, если опрос не прошел
			syncTime();
		// запуск сервисов, которые должны запуститься после запуска сети. (сеть должна подниматься в фоне)
		ftp_process();
		web_process();
		if(hubRegTimer.isReady()) registration_dev();
	}

	if( btn.hold() ) {
		LOG(println, "touch button is hold");
		Serial.println(gs.host_name);
		led.invert();
	}
	if( btn.hasClicks() ) {
		switch(btn.getClicks()) {
			case 1:
				LOG(println, "1 click");
				if(wifi_isPortal) wifi_startConfig(false);
				else
				if(!wifi_isConnected) wifi_startConfig(true);
				if(fl_password_reset_req) {
					fl_password_reset_req = false;
					led.blink(OFF);
				}
				else 
				{
					led.blink(ON, 200, 3, 400);
					// аварийный останов всех насосов
					for(uint8_t i=0; i<PUMPS; i++)
						p[0].stop();
				}
				break;
			case 2:
				LOG(println, "2 click");
				if(fl_password_reset_req) {
					gs.web_password = "";
					LOG(println, PSTR("password disabled"));
				} else {
					// for(uint8_t i=0; i<PUMPS; i++)
					// 	p[0].stop();
					gsm_sendSMS("test SMS");
				}
				break;
			case 3:
				LOG(println, "3 click");
				if( wifi_isConnected ) registration_dev();
				break;
			case 4:
				LOG(println, "4 click");
				fl_password_reset_req = true;
				led.blink(ON, 1000, 0, 100);
				break;
			case 5:
				LOG(println, "5 click");
				if(!wifi_isPortal) wifi_startConfig(true);
				break;
		}
	}

	if(digitalRead(PIN_5V) != fl_5v) {
		delay(50); // проверка на случайную помеху. Резисторы стоят сильно большие и случайные наводки могут давать сбой логики.
		if(digitalRead(PIN_5V) != fl_5v) {
			fl_5v = ! fl_5v;
			// отослать sms об изменении состояния питания
			gsm_sendSMS(fl_5v ? String("Power is ON :) b:") + battery + "%": String("Power is OFF :( b:") + battery + "%");
			// if(sec_enable) save_log_file(fl_5v ? SEC_TEXT_POWERED: SEC_TEXT_POWEROFF);
			// fl_allowLEDS = fl_5v;
			// если нет питания - отключить питание матрицы и снизить яркость на минимум
			// if(!fl_5v) {
				// set_brightness(1);
				// old_brightness = 1;
			// } else {
				// digitalWrite(PIN_RELAY, RELAY_OFF);
				// if(gs.bright_mode==2) set_brightness(gs.bright0);
			// }
		}
	}

	// накопление значений влажности
	for(uint8_t i=0; i<SENSORS; i++) moi_tmp[i] += analogRead(sensorPIN[i]);
	moi_count++;

	// всё, что может обновляться лениво, с секундным интервалом
	if( seconds.isReady() ) {
		// напряжение батареи
		battery_raw = analogRead(PIN_BAT);
		uint16_t t_bat = constrain(battery_raw, gs.low_v, gs.high_v);
		uint16_t half = (gs.high_v - gs.low_v) / 2 + gs.low_v;
		if( t_bat > half )
			battery = map(t_bat, half, gs.high_v, 20, 100);
		else
			battery = map(t_bat, gs.low_v, half, 0, 20);

		// датчик влажности
		for(uint8_t i=0; i<SENSORS; i++) {
			moi_raw[i] = moi_tmp[i]/moi_count;
			moi_tmp[i] = 0;
			moi_per[i] = map(moi_raw[i], mc[i].moi0, mc[i].moi100, 0, 100);
		}
		moi_count = 0;

		if( ! fl_timeNotSync ) { // если время не определилось, то расписание проверять бесполезно
			tm now = getTime();
			if( now.tm_min != a_minute ) { // новая минута, просмотр, есть ли задания для этой минуты
				a_minute = now.tm_min;
				uint16_t minutes_left = now.tm_hour*60 + now.tm_min;
				// LOG(printf, "minutes_left=%u, now %u.%u\n", minutes_left, now.tm_hour, now.tm_min);
				for(uint8_t i=0; i<SCHEDULERS; i++) {
					if( scheduler[i].cm & 128 ) { // планировщик активен
						uint16_t test = 0;
						while( test < 1440 ) { // упрощённая схема. От времени и до полуночи
							if( scheduler[i].t + test == minutes_left ) { // задача активна и время настало)
								// проверка условия
								uint8_t cond = (scheduler[i].cm >> 5) & 3;
								bool fl_doit = false;
								if( cond == 0 ) fl_doit = true;
								else
								if( cond == 1 ) { // условие - влажность ниже
									uint8_t sensors = scheduler[i].cm & 31;
									uint16_t moi = 0;
									if( sensors == 0 ) { // среднее по всем сенсорам
										for(uint8_t ii=0; ii<SENSORS; ii++) {
											moi += moi_per[ii];
										}
										moi = moi / SENSORS;
										// moi = (moi_per[0] + moi_per[1]) / 2; 
									} else moi = moi_per[sensors-1]; // конкретный сенсор
									if( moi < scheduler[i].cv ) fl_doit = true; // условие выполнено
								}
								else
								if( cond == 2 ) { // условие - влажность выше
									uint8_t sensors = scheduler[i].cm & 31;
									uint16_t moi = 0;
									if( sensors == 0 ) { // среднее по всем сенсорам
										for(uint8_t ii=0; ii<SENSORS; ii++) {
											moi += moi_per[ii];
										}
										moi = moi / SENSORS;
										// moi = (moi_per[0] + moi_per[1]) / 2; 
									} else moi = moi_per[sensors-1]; // конкретный сенсор
									if( moi > scheduler[i].cv ) fl_doit = true; // условие выполнено
								}
								if( fl_doit ) { // можно запускать, выбор насосов, которые надо включить
									for(uint8_t ii=0; ii<PUMPS; ii++) {
										if( (scheduler[i].s >> ii) & 1 ) { // постановка в очередь
											pq[ii].active = true;
											pq[ii].need = scheduler[i].p;
										}
									}
								}							
								break;
							}
							test += scheduler[i].r == 0 ? 1440: scheduler[i].r; // проверка времени повтора
							// LOG(printf, "scheduler test=%u\n", test);
						}
					}
				}
			}
		}
	}

	// если есть питание 5V - проверить очередь задач
	if( fl_5v ) {
		// стадия 1 - определение есть ли уже запущенные насосы
		bool all_free = true;
		for(uint8_t i=0; i<PUMPS; i++)
			if( p[i].active() ) all_free = false;
		// стадия 2 - определение надо ли что-то включить
		if( all_free )
			for(uint8_t i=0; i<PUMPS; i++)
				if( pq[i].active ) {
					uint32_t calc_msec = 1000;
					if( pq[i].seconds ) // запуск на запрошенное число секунд
						calc_msec = constrain(pq[i].seconds,1,255)*1000;
					else // запуск на запрошенное число порций
						calc_msec = constrain(pq[i].need,1,255) * pc.in_ms * gs.doze + pc.empty_ms;
					p[i].run(calc_msec); // непосредственно запуск насоса
					pq[i].active = false;
					pq[i].seconds = 0;
					ps[i].count++;
					ps[i].vol += float(calc_msec-pc.empty_ms) / float(pc.in_ms) / 1000;
					fl_need_save_state = true;
					saveStateTimer.reset();
					break; // выйти, так как в одно время должен быть запущен только один насос (блок питания больше не потянет)
				}
	}

	// отложенная запись состояния для уменьшения нагрузки на flash
	if( saveStateTimer.isReady() && fl_need_save_state ) {
		save_pump_state();
		fl_need_save_state = false;
		// отправка сообщения с текущим статусом
		String t = "Watering: ";
		// float sum = 0.0f;
		for(uint8_t i=0; i<PUMPS; i++) {
			t += String("p:") + (i+1) + " ,c:" + ps[i].count + ", v:" + ps[i].vol + ", "; 
			// sum += ps[i].vol;
		}
		t += String("s1:") + moi_per[0] + "%, s2:" + moi_per[1] + "%";
		gsm_sendSMS(t);
	}

	

	// touch_value_t new_touch = touchRead(BUTTON);
	// if( new_touch != last_touch ) {
	// 	Serial.printf("touch value = %u\n", last_touch);
	// 	last_touch = new_touch;
	// }

}

static void TaskGSMCode( void * pvParameters ) {
	LOG(print, "TaskGSMCode running on core ");
	LOG(println, xPortGetCoreID());
	vTaskDelay(1);

	for(;;) {
		// единственное, что должна делать эта задача - обслуживать gsm модем
		gsm_pool();
		// обязательная пауза, чтобы задача смогла вернуть управление FreeRTOS, иначе будет срабатывать watchdog timer
		vTaskDelay(1000/portTICK_PERIOD_MS); // раз в секунду
	}
}