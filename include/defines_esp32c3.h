#ifndef DEFINES_ESP32C3_H
#define DEFINES_ESP32C3_H

// #define USE_GSM // исспользовать GSM модем
#define USE_MOISTURE_SENSORS // использовать датчики влажности
#define USE_SHIFT_REGISTER // использовать микросхему сдвигового регистра для мультиплексирования выводов под насосы (74HC595)
#define USE_AHTx0 // исспользовать датчик температуры и влажности AHT10/AHT20

#ifdef USE_GSM
    // Определение всех параметров, ножек и пр.
    #define gsmSerial Serial2
    #define PIN_gsmTX 17 // RX SM800L 4
    #define PIN_gsmRX 16 // TX SM800L 5
    #define PIN_gsmRST 2 // RST
    #define PIN_gsmDTR 4 // DTR (sleep)
    #define PIN_gsmRING 15 // RING
#endif

#ifdef USE_SHIFT_REGISTER
    #define PIN_LATCH 7 	// (12) защёлка сдвигового регистра (ST_CP)
    #define PIN_CLOCK 6	    // (11) тактовая сдвигового регистра (SH_CP)
    #define PIN_DATA 10		// (14) данные сдвигового регистра (DS)
    #define PIN_OE 2        // (13) подключение к нагрузке (!OE) (можно закоментировать и запаять этот выход на землю. Читай readme)
#endif

#define PIN_LED 8 // светодиод напаянный на плату
#define PIN_LED_GREEN 200 // дополнительный светодиод, если нет - закомментировать

const uint8_t pumpPIN[] = {207, 206, 205, 204, 203, 202}; // список PIN к которым подключены насосы

#define RELAY_LEVEL HIGH // реле срабатывает по высокому HIGH или низкому LOW уровню

// #define PIN_BAT 36 // напряжение аккумулятора (+ через делитель 300к/1Mb и диод)

const uint8_t sensorPIN[] = {0, 1, 3, 4}; // список PIN к которым подключены датчики влажности. 
    // В любом случае надо объявить этот список, даже если датчики не используются.
    // Тогда надо указать один любой пин, он не будет использоваться, но даст скомпилировать код
    // Для esp32c3 можно использовать только 4 аналоговых пина из 6-ти.
    // Пин 2 подтянут к +3.3, иначе плата не стартует. А пин 5 конфликтует с wifi.

// #define PIN_5V 13 // датчик напряжения 5V, наличие сети. Закомментировать, если не используется.

#ifdef USE_AHTx0
    #define PIN_SCL 21
    #define PIN_SDA 20
    #define AHT_ADDRESS 0x38
#endif

#define PIN_BUTTON 9 // пин кнопки. Закомментировать, если не используется.
#define BUTTON_TYPE 0 // 0 - обычная кнопка с внутренней подтяжкой, 1 - инверсная кнопка с внешней подтяжкой (внешняя сенсорная), 2 - сенсорная esp32
#define TOUCH_THRESHOLD 60 // порог срабатывания сенсорной кнопки

#define SCHEDULES 12 // количество ячеек расписания полива

#endif
