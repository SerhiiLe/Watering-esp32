#ifndef DEFINES_H
#define DEFINES_H

#define DEBUG

// Определение всех параметров, ножек и пр.

#define gsmSerial Serial2
#define PIN_gsmTX 17 // RX SM800L 4
#define PIN_gsmRX 16 // TX SM800L 5
#define PIN_gsmRST 2 // RST
#define PIN_gsmDTR 4 // DTR (sleep)
#define PIN_gsmRING 15 // RING

#define PIN_LED 22 // напаянный на плату

const uint8_t pumpPIN[] = {12, 14, 27, 26}; // список PIN к которым подключены насосы

#define RELAY_LEVEL HIGH // реле срабатывает по высокому HIGH или низкому LOW уровню

#define PIN_BAT 36 // напряжение аккумулятора (+ через делитель 300к/1Mb и диод)

const uint8_t sensorPIN[] = {34, 35}; // список PIN к которым подключены датчики влажности

#define PIN_5V 13 // датчик напряжения 5V, наличие сети

#define PIN_BUTTON 33 // пин сенсорной кнопки

#define TIMEZONE 2 // часовая зона по умолчанию - EET +2
#define DSTSHIFT 1 // летнее время (+1 час)
#define TELEGRAM_MAX_LENGTH 2000    // максимальный буфер для подготовки к отсылке сообщений (urlEncode)

#define TOUCH_THRESHOLD 60 // порог срабатывания сенсорной кнопки
#define PUMPS sizeof(pumpPIN) // количество помп
#define SENSORS sizeof(sensorPIN) // количество сенсоров

#define SCHEDULERS 8 // количество ячеек расписания полива

/*** ускорение опросов телеграм-бота или временная приостановка запросов ***/

#define TELEGRAM_ACCELERATE 900 // время на которое запросы к телеграм идут чаще, в секундах
#define TELEGRAM_ACCELERATED 10 // периодичность ускоренного опроса телеграм, в секундах 
#define TELEGRAM_BAN 1800 // время на которое прекращаются запросы к телеграм после ошибки, в секундах
#define TELEGRAM_MAX_LENGTH 2000 // максимальный размер сообщений отсылаемых в Телеграм

//----------------------------------------------------
#include "define_vars.h"

#endif
