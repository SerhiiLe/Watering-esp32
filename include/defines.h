#ifndef DEFINES_H
#define DEFINES_H

#define DEBUG

#if ESP32C3 == 1
    #include "defines_esp32c3.h"
#else
    #include "defines_esp32.h"
#endif

#define SCHEDULERS 8 // количество ячеек расписания полива

#define TIMEZONE 2 // часовая зона по умолчанию - EET +2
#define DSTSHIFT 1 // летнее время (+1 час)

/*** ускорение опросов телеграм-бота или временная приостановка запросов ***/

#define TELEGRAM_MAX_LENGTH 2000    // максимальный буфер для подготовки к отсылке сообщений (urlEncode)
#define TELEGRAM_ACCELERATE 900 // время на которое запросы к телеграм идут чаще, в секундах
#define TELEGRAM_ACCELERATED 10 // периодичность ускоренного опроса телеграм, в секундах 
#define TELEGRAM_BAN 1800 // время на которое прекращаются запросы к телеграм после ошибки, в секундах
#define TELEGRAM_MAX_LENGTH 2000 // максимальный размер сообщений отсылаемых в Телеграм

//----------------------------------------------------
#include "define_vars.h"

#endif
