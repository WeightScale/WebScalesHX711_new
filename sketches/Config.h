#pragma once
#include "Arduino.h"

#define WEB_TERMINAL2			1
#define WEB_TERMINAL_MINI		2
#define WEB_CRANE				3
#define SCALE_SERVER			4

//#define BOARD WEB_TERMINAL2
#define BOARD WEB_TERMINAL_MINI
//#define BOARD WEB_CRANE
//#define BOARD SCALE_SERVER

#define BATTERY_6V				6
#define BATTERY_4V				4

//#define EXTERNAL_POWER 1
//#define INTERNAL_POWER 2

//#define POWER_DEBUG
//#define HTML_PROGMEM			//Использовать веб страницы из flash памяти
#define MULTI_POINTS_CONNECT	/* Использовать для использования с несколькими точками доступа */

#ifdef HTML_PROGMEM
	#include "Page.h"
#endif

//#define MY_HOST_NAME "scl"
//#define SOFT_AP_SSID "SCALES"
#define SOFT_AP_PASSWORD "12345678"
#define HOST_URL "sdb.net.ua"		/** Адрес базы данных*/

#define TIMEOUT_HTTP 3000			/** Время ожидания ответа HTTP запраса*/
#define STABLE_NUM_MAX 10			/** Количество стабильных измерений*/
#define MAX_EVENTS 100				/** Количество записей событий*/


#if BOARD == WEB_TERMINAL2
	#include "web_terminal2.h"
	#define INTERNAL_POWER
	#define PLAN_BATTERY			BATTERY_4V
	#define SKETCH_VERSION			"web_terminal2.001"
	#define SPIFFS_VERSION			SKETCH_VERSION
#elif BOARD == WEB_TERMINAL_MINI
	#include "web_terminal_mini.h"
	#define EXTERNAL_POWER
	#define PLAN_BATTERY			BATTERY_6V	
	#define SKETCH_VERSION			"web_terminal_mini.001"
	#define SPIFFS_VERSION			SKETCH_VERSION
#elif BOARD == WEB_CRANE
	#include "web_crane.h"
	#define INTERNAL_POWER
	#define PLAN_BATTERY			BATTERY_4V
	#define SKETCH_VERSION			"web_crane.001"
	#define SPIFFS_VERSION			SKETCH_VERSION
#elif SCALE_SERVER
	#include "scale_server.h"
	#define INTERNAL_POWER
	#define PLAN_BATTERY			BATTERY_4V
	#define SKETCH_VERSION			"scale_server.001"
	#define SPIFFS_VERSION			SKETCH_VERSION
#endif // BOARD == WEB_TERMINAL2

typedef struct {
#ifdef INTERNAL_POWER
	bool power_time_enable;
	unsigned int time_off;
#endif
#ifdef MULTI_POINTS_CONNECT
	unsigned int timeScan; 		//Время период сканирования в секундах
	unsigned char deltaRSSI; 	//Дельта мощности сигнала при проверке в процентах
	bool enable_scan;			//Использовать сканирование лучшей сети из списка
#else
	bool dnip;	
	char lanIp[16];
	char gate[16];
	char mask[16];
	char wSSID[33];
	char wKey[33];
#endif // !MULTI_POINTS_CONNECT		
	char hostName[16];
	char hostUrl[0xff];
	int hostPin;
	char user[16];
	char password[16];
	int bat_max;
	int bat_min;
} settings_t;

typedef struct {
#if BOARD == WEB_TERMINAL_MINI
	bool rate;
#endif
	bool enable_zero_auto;
#ifdef INTERNAL_POWER
	bool power5v;
#endif
	long offset; /*  */
	unsigned char average; /*  */
	unsigned char step; /*  */
	int accuracy; /*  */
	float max; /*  */
	float scale;
	unsigned char filter;
	int seal;
	float zero_man_range;    			// 0,2,4,10,20,50,100% диапазон обнуления
	float zero_on_range;     			// 0,2,4,10,20,50,100% диапазон обнуления
	unsigned char zero_auto;    		// 0~4 дискрет авто обнуление
	char user[16];
	char password[16];
}t_scales_value;

struct MyEEPROMStruct {
	settings_t settings;
	t_scales_value scales_value;
};

//#define MAX_CHG 1013//980	//делитель U2=U*(R2/(R1+R2)) 0.25
//#define MIN_CHG 880			//ADC = (Vin * 1024)/Vref  Vref = 1V
//#define MIN_CHG 670			//ADC = (Vin * 1024)/Vref  Vref = 1V	Vin = 0.75 5.5v-6.5v
//#define MIN_CHG 500			//ADC = (Vin * 1024)/Vref  Vref = 1V  Vin = 0.49v  3.5v-4.3v

/*#if POWER_PLAN == INTERNAL_POWER
	#define MIN_CHG 500			//ADC = (Vin * 1024)/Vref  Vref = 1V  Vin = 0.49v  3.5v-4.3v
#else if POWER_PLAN == EXTERNAL_POWER
	#define MIN_CHG 820			//ADC = (Vin * 1024)/Vref  Vref = 1V	Vin = 0.75 5.5v-6.5v
#endif*/

/* для удаленного обновления
Формат файла

[имя].[версия].[тип].[расширение]


имя			- имя файла или мак адресс
версия		- версия программы число
тип			- тип файла ino или spiff
расширение	- bin

пример: weight_scale.001.spiff.bin

алгоритм проверки приблизительно такой

делаем split
если не bin false
если тип spiff или ino выбираем соответствуюшую папку типа spiffs sketch 
в папке ищем папку по имени и проверяем версию

хидеры которые отправляет код

    http.useHTTP10(true);
    http.setTimeout(8000);
    http.setUserAgent(F("ESP8266-http-Update"));
    http.addHeader(F("x-ESP8266-STA-MAC"), WiFi.macAddress());
    http.addHeader(F("x-ESP8266-AP-MAC"), WiFi.softAPmacAddress());
    http.addHeader(F("x-ESP8266-free-space"), String(ESP.getFreeSketchSpace()));
    http.addHeader(F("x-ESP8266-sketch-size"), String(ESP.getSketchSize()));
    http.addHeader(F("x-ESP8266-sketch-md5"), String(ESP.getSketchMD5()));
    http.addHeader(F("x-ESP8266-chip-size"), String(ESP.getFlashChipRealSize()));
    http.addHeader(F("x-ESP8266-sdk-version"), ESP.getSdkVersion());

    if(spiffs) {
	    http.addHeader(F("x-ESP8266-mode"), F("spiffs"));
	    } else {
	    http.addHeader(F("x-ESP8266-mode"), F("sketch"));
    }

    if(currentVersion && currentVersion[0] != 0x00) {
	    http.addHeader(F("x-ESP8266-version"), currentVersion);
    }

    const char * headerkeys[] = { "x-MD5" };
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);

    // track these headers
    http.collectHeaders(headerkeys, headerkeyssize);

*/
