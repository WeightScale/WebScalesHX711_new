#pragma once
#include "Task.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"

//#define DEBUG_BATTERY		/*Для теста*/

#define CONVERT_V_TO_ADC(v)		(((v * (R2_KOM /(R1_KOM + R2_KOM)))*ADC)/VREF)
#define MAX_CHG					CONVERT_V_TO_ADC(V_BAT_MAX)		/**/
#define MIN_CHG					CONVERT_V_TO_ADC(V_BAT_MIN)		/**/
#define CALCULATE_VIN(v)		(((v/ADC)/(R2_KOM /(R1_KOM + R2_KOM)))*VREF)


class BatteryClass : public Task {	
protected:
	bool _isDischarged = false;
	unsigned int _charge;
	settings_t * _value;
	unsigned int * _max; /*Значение ацп максимального заряд*/
	unsigned int * _min; /*Значение ацп минимального заряд*/
	unsigned int _get_adc(byte times = 1);	
public:
	//BatteryClass();
	BatteryClass(settings_t * value);
	~BatteryClass() {};	
	unsigned int fetchCharge();		
	void charge(unsigned int ch){_charge = ch; };
	unsigned int charge(){return _charge;};
	void max(unsigned int *m){_max = m; };	
	void min(unsigned int *m){_min = m; };	
	unsigned int *max(){return _max;};
	unsigned int *min(){return _min;};
	size_t doInfo(JsonObject& json);
	size_t doData(JsonObject& json);
	void handleBinfo(AsyncWebServerRequest *request);
	bool isDischarged(){return _isDischarged;};
};

//extern BatteryClass* BATTERY;