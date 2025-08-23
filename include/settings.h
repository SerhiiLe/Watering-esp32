#ifndef settings_h
#define settings_h

bool load_config_main();
void save_config_main();
bool load_moisture_calibration();
void save_moisture_calibration();
bool load_pump_calibration();
void save_pump_calibration();
bool load_schedulers();
void save_schedulers();
bool load_pump_state();
void save_pump_state();
bool load_config_log();
void save_config_log();
void save_log_file(const char* text);
void save_log_file(String text);
String read_log_file(int16_t cnt);

#define LOG_MAX 50		// максимальная строка одной записи лога (45 + символы склейки "%0A" + конец строки \0)
#define LOG_SIZE 4096	// максимальный размер файла, после которого запись будет во второй файл. (запись по кругу)
#define LOG_COUNT 3		// число файлов
const char LOG_FILE[] PROGMEM = "/log%u.txt"; // шаблон имени файла

#endif