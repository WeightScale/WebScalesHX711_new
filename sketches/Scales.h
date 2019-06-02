#pragma once
#include "HX711.h"
#include "BrowserServer.h"
//#include "CoreMemory.h"
#include <ArduinoJson.h>
#include "Task.h"

#define DEBUG_WEIGHT_RANDOM
//#define DEBUG_WEIGHT_MILLIS

#define DOUT_PIN		GPIO14							/* сигнал дата АЦП */
#define SCK_PIN			GPIO16							/* сигнал clock АЦП */

typedef struct {
	bool isSave;
	int stabNum;
	float value;
	unsigned long time;
}t_save_value;

class ScalesClass : public HX711, public Task {
private:
	scales_value_t * _value;
	float _weight;
	float _round; /* множитиль для округления */
	float _stable_step; /* шаг для стабилизации */
	bool _stableWeight = true;
	t_save_value _saveWeight = {0};
	long _offset_local;	
	float _weight_zero_auto; /* вес автосброса на ноль */
	float _tape;	/* Значение тары */

public:
	ScalesClass(byte, byte, scales_value_t * value);
	~ScalesClass() {};	
	void begin();
	void takeWeight();
	//void saveValueCalibratedHttp(AsyncWebServerRequest *);
	void fetchWeight();
	void mathScale(float referenceW, long calibrateW);
	void mathRound() {
		_round = pow(10.0, _value->accuracy) / _value->step;   // множитель для округления}
		_stable_step = 1 / _round;
		//_weight_zero_auto = round(_value->zero_auto * _round) / _round;
		_weight_zero_auto = (float)_value->zero_auto / _round;
	}
	void offset(long offset = 0){_offset_local = offset;};
	long offset(){return _offset_local;};		
	long readAverage();
	long getValue();
	void average(unsigned char);
	unsigned char average(){return _value->average;};
	void seal(int s){_value->seal = s; };
	int seal(){ return _value->seal;};	
	void detectStable(float);
	void detectAutoZero();
	bool overload() {return _weight > _value->max;};	
	float getUnits();
	float getWeight();
	void zero(float range);
		
	void formatValue(float value, char* string);
	float Round(){return _round;};
	bool isSave(){return _saveWeight.isSave;};
	float getSaveValue(){return _saveWeight.value;};
	void isSave(bool s){_saveWeight.isSave = s;};
	size_t doData(JsonObject& json);
	//size_t doDataRandom(JsonObject& json);
		
	char *user(){return _value->user;};
	char *password(){return _value->password;};
	int accuracy(){return _value->accuracy;};
};

//extern ScalesClass Scale;
//extern Task *taskWeight;
//extern void takeWeight();
void handleSeal(AsyncWebServerRequest*);