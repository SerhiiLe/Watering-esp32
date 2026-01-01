#ifndef DEFINES_ESP32C3_H
#define DEFINES_ESP32C3_H

// #define USE_GSM // исспользовать GSM модем
// #define USE_MOISTURE_SENSORS // использовать датчики влажности
#define USE_SHIFT_REGISTER // использовать микросхему сдвигового регистра для мультиплексирования выводов под насосы (74HC595)

#ifdef USE_GSM
    #define DUMP_AT_COMMANDS
    // Определение всех параметров, ножек и пр.
    #define gsmSerial Serial2
    #define PIN_gsmTX 17 // RX SM800L 4
    #define PIN_gsmRX 16 // TX SM800L 5
    #define PIN_gsmRST 2 // RST
    #define PIN_gsmDTR 4 // DTR (sleep)
    #define PIN_gsmRING 15 // RING
#endif

#ifdef USE_SHIFT_REGISTER
    #define PIN_DO 6    // вывод данных
    #define PIN_CLK 7   // тактовый сигнал
    #define PIN_CS 10   // активация чипа
#endif

#define PIN_LED 8 // напаянный на плату

const uint8_t pumpPIN[] = {200, 201, 202, 203, 204, 205}; // список PIN к которым подключены насосы

#define RELAY_LEVEL HIGH // реле срабатывает по высокому HIGH или низкому LOW уровню

#define PIN_BAT 36 // напряжение аккумулятора (+ через делитель 300к/1Mb и диод)

const uint8_t sensorPIN[] = {0, 1, 2, 3, 4, 5}; // список PIN к которым подключены датчики влажности. 
    // В любом случае надо объявить этот список, даже если датчики не используются. Тогда надо указать один любой пин, он не будет использоваться, но даст скомпилировать код

#define PIN_5V 13 // датчик напряжения 5V, наличие сети. Закомментировать, если не используется.

#define PIN_BUTTON 27 // пин кнопки. Закомментировать, если не используется.
#define BUTTON_TYPE 0 // 0 - обычная кнопка с внутренней подтяжкой, 1 - инверсная кнопка с внешней подтяжкой (внешняя сенсорная), 2 - сенсорная esp32
#define TOUCH_THRESHOLD 60 // порог срабатывания сенсорной кнопки

#endif
