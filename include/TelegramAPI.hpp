#ifndef TELEGRAM_API_HPP
#define TELEGRAM_API_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SSLClient.h>

#define API_HOST "api.telegram.org"
#define API_PORT 443

struct TResult {
	long chatId;
	String text;
	String from;
};

class TelegramAPI {

	public:

	// Конструктор
	TelegramAPI(SSLClient& sslClient) : client(sslClient), apiHost(API_HOST)
	{}

	// установить токен, без него работать не будет	(String)
	void setBotToken(const String& token) {
		_botToken = token;
	}
	// установить токен, без него работать не будет
	void setBotToken(const char* token) {
		_botToken = String(token);
	}

	// установить ID чата по умолчанию. Именно в него будут отправляться уведомления. Отвечать бот будет тот чат из которого пришел запрос
	void setChatID(long chatID) {
		_chatId = chatID;
	}

	// период опроса в секундах. Каждая установка сбрасывает время отсчёта.
	void setInterval(int interval) {
		if ( interval > 1 ) updatePeriod = interval * 1000; // чаще, чем раз в секунду
		resetTimer();
	}
	
	// подключение обработчика сообщений
    void attachCallback(String (*handler)(TResult& tr)) {
        _callback = handler;
    }

    // отключение обработчика сообщений
    void detachCallback() {
        _callback = nullptr;
    }

	// подключение функции проверки состояния соединения
	void attachCheckConnection(bool (*handler)()) {
		_checkConnection = handler;
	}

	// отключение функции проверки состояния соединения
	void detachCheckConnection() {
		_checkConnection = nullptr;
	}

	// пропустить непрочитанные сообщения
	void skipUpdates() {
		lastUpdateId = -1;
	}

	// Отправка сообщения в указанный напрямую чат
	bool sendMessage(long chatId, const char* message) {
		if( !chatId ) return false; // не указан ID чата, куда надо отослать сообщение
		if ( _checkConnection && ! _checkConnection() ) return false; // канал связи недоступен

		JsonDocument doc;
		doc["chat_id"] = chatId;
		doc["text"] = message;

		String jsonPayload;
		serializeJson(doc, jsonPayload);

		ApiResult r = apiRequest("sendMessage", jsonPayload);
		return r.result; // при отправке в общем то и проверять нечего... Может потом придумаю.
	}
	// Отправка сообщения в указанный напрямую чат (String)
	bool sendMessage(long chatId, const String message) {
		return sendMessage(chatId, message.c_str());
	}
	// Отправка сообщения в указанный setChatID чат
	bool sendMessage(const char* message) {
		return _chatId > 0 ? sendMessage(_chatId, message): false;
	}
	// Отправка сообщения в указанный setChatID чат (String)
	bool sendMessage(const String message) {
		return _chatId > 0 ? sendMessage(_chatId, message.c_str()): false;
	}

	// Проверка новых сообщеий. К этотму моменту должена быть установлена функция для обработки сообщений attach
	// возвращает число полученных сообщений, или <0 если ошибка
	int checkMessage(bool force=false) {
		int result = 0;

		if ( !force && !isReady() ) return 0; // время запроса ещё не пришло
		if ( !_callback ) return -2; // если callback функция не установлена, то сразу выйти, проверка не имеет смысла
		if ( _checkConnection && ! _checkConnection() ) return -3; // канал связи недоступен

		JsonDocument doc;
		// doc["timeout"] = 2;
		doc["limit"] = 5;
		doc["offset"] = lastUpdateId + 1;

		String jsonPayload;
		serializeJson(doc, jsonPayload);

		ApiResult ar = apiRequest("getUpdates", jsonPayload);
		if ( ar.result )
		for (JsonObject upd : ar.json["result"].as<JsonArray>()) {
		    long uid = upd["update_id"].as<long>();
		    if (uid <= lastUpdateId) continue;
    		lastUpdateId = uid;

		    if (upd["message"].is<JsonObject>()) {
				JsonObject msg = upd["message"];
				TResult r;
				r.chatId  = msg["chat"]["id"].as<long>();
				r.text    = msg["text"]              | "";
				r.from    = msg["from"]["first_name"] | "User";

				Serial.printf("[MSG] %s (%ld): %s\n", r.from.c_str(), r.chatId, r.text.c_str());
				if (_callback) {
					vTaskDelay(10);
					String toSend = _callback(r);
					if (toSend.length() > 0) {
						vTaskDelay(30); // небольшая задержка между приёмом и отправкой сообщения (>30ms), за одно сброс watchdog
						sendMessage(r.chatId, toSend);
					}
				}
				result++;
			}
		}
		else return -1; 
		return result;
	}

	private:

	struct ApiResult {
		bool result;
		JsonDocument json;
	};

	// запрос к API telegram. Всегда POST, в payload должна быть сформированная строка Json
	// в type должна бытьвызываемую функцию: sendMessage, getUpdates...
	ApiResult apiRequest(const char* type, const String payload) {
		int statusCode = -1;
		String response;
		JsonDocument responseDoc;

		if (_botToken.length()<38) return {false, responseDoc}; // нет токена - нет запроса

		String url = "/bot" + _botToken + "/" + type;
		String request = "POST " + url + " HTTP/1.1\r\n" +
						"Host: " + apiHost + "\r\n" +
						"Content-Type: application/json\r\n" +
						"Content-Length: " + String(payload.length()) + "\r\n" +
						"Connection: close\r\n\r\n" +
						payload;

		for (int attempt = 1; attempt <= maxRetries; attempt++) {
			if( !checkConnection() ) { // попытка соединится с сервером, если нет, то повтор
				Serial.printf("Connection failed, attempt %d/%d\n", attempt, maxRetries);
				vTaskDelay(retryDelay);
				continue;
			}

			client.print(request);

			unsigned long timeout = millis() + 10000L;
			bool body = false;
			// ожидание соединения с сервером
			while (client.connected() && !client.available() && (millis() < timeout))
				vTaskDelay(10);
			// ответ получен, чтение
			while (client.available() > 0) {
				if( body ) {
					// тупой и грязный способ формирования строки, но в конкретном случае он самый быстрый
					while (client.available()) {
						char c = client.read();
						response += c;
					}
				} else
					response = client.readStringUntil('\n');

				if ( response.startsWith("HTTP/1") ) statusCode = getHttpStatusCode(response); // поиск статуса во всех строках, но сработает только в первой
				if ( response.length() < 3 ) body = true; // пустая строка отделяет заголовок от тела. Может содержать пару символов '\r'
			}

			Serial.printf("status code: %d\n", statusCode);
			Serial.println("answer:");
			Serial.println(response);

			if (statusCode != 200) { // что-то пошло не так, показ сообщения и заход на вторую попытку
				Serial.printf("HTTP error: %d\n", statusCode);
				if (statusCode == 429) {
					JsonDocument errorDoc;
					deserializeJson(errorDoc, response);
					int retryAfter = errorDoc["parameters"]["retry_after"] | 60;
					Serial.printf("Rate limit exceeded, retry after %d seconds\n", retryAfter);
					vTaskDelay(retryAfter * 1000);
				}
				client.stop();
				continue;
			}

			DeserializationError error = deserializeJson(responseDoc, response.c_str());
			if (error) {
			    Serial.print(F("deserializeJson() failed: "));
    			Serial.println(error.f_str());
				client.stop();
				continue;
			}

			if (!responseDoc["ok"].as<bool>()) {
				int errorCode = responseDoc["error_code"] | 0;
				String description = responseDoc["description"] | "Unknown error";
				Serial.printf("Telegram API error %d: %s\n", errorCode, description.c_str());
				if (errorCode == 401) {
					Serial.println("Invalid bot token, stopping");
					client.stop();
					return {false, responseDoc}; // досрочное прерывание, нет смыла повторять запрос, если токен не принят
				}
				client.stop();
				continue;
			}

			client.stop();
			return {true, responseDoc};
		}
		// при ошибках запрос повторяется. Если дошло до этого блока, то все попытки завершились неудачей.
		return {false, responseDoc};
	}

	// Проверка соединения
	bool checkConnection() {
		if ( !client.connect(apiHost, apiPort) ) {
			Serial.println("No connection to Telegram server");
			client.stop();
			return false;
		}
		return true;
	}

	// Чтение HTTP-кода из ответа
	int getHttpStatusCode(const String& response) {
		if (response.startsWith("HTTP/1.1 ")) {
			int code = response.substring(9, 12).toInt();
			return code;
		}
		return -1;
	}
	
	// возвращает true, когда пришло время.
	bool isReady() {
		unsigned long time = millis();
		if(_overflow) { // попытка защититься от переполнения
			if(time < lastUpdate) // ждём переполнения, которое наступает каждые 49 дней
				_overflow = false;
			else
				return false;
		}
		if(time >= nextUpdate) {
			resetTimer();
			return true;
		} else {
			return false;
		}
	}
	// сброс таймера на установленный интервал
	void resetTimer() {
		lastUpdate = millis();
		nextUpdate = lastUpdate + updatePeriod;
		_overflow = lastUpdate > nextUpdate; 
	}

	SSLClient& client;    // Ссылка на SSLClient
	String _botToken;
	const int maxRetries = 3; // Максимум попыток при ошибке соединения
	const int retryDelay = 1000; // Задержка между попытками (мс)
	const int requestTimeout = 35000; // Таймаут для long polling (35 секунд, учитывая timeout=30)

	long lastUpdateId = 0;
	unsigned long _chatId = 0, lastUpdate = 0, nextUpdate = 0;
	unsigned int updatePeriod = 10000;
	bool _overflow = false;

	const char* apiHost; // = "api.telegram.org";
	const uint16_t apiPort = API_PORT;

	String (*_callback)(TResult& tr) = nullptr;
	bool (*_checkConnection)() = nullptr;

};

#endif