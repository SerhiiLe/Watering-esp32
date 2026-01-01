#ifndef DEFINES_ESP32_H
#define DEFINES_ESP32_H

#define USE_GSM // исспользовать GSM модем
#define USE_MOISTURE_SENSORS // использовать датчик(и) влажности
// #define USE_SHIFT_REGISTER // использовать микросхему сдвигового регистра для мультиплексирования выводов под насосы

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

#define PIN_LED 22 // напаянный на плату

const uint8_t pumpPIN[] = {12, 14, 26, 25}; // список PIN к которым подключены насосы

#define RELAY_LEVEL HIGH // реле срабатывает по высокому HIGH или низкому LOW уровню

#define PIN_BAT 36 // напряжение аккумулятора (+ через делитель 300к/1Mb и диод)

const uint8_t sensorPIN[] = {34, 35, 33, 39}; // список PIN к которым подключены датчики влажности

#define PIN_5V 13 // датчик напряжения 5V, наличие сети

#define PIN_BUTTON 27 // пин кнопки
#define BUTTON_TYPE 2 // 0 - обычная кнопка с внутренней подтяжкой, 1 - инверсная кнопка с внешней подтяжкой (внешняя сенсорная), 2 - сенсорная esp32
#define TOUCH_THRESHOLD 60 // порог срабатывания сенсорной кнопки

#endif
