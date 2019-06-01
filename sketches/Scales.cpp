#include "Scales.h"
#include "Board.h"

ScalesClass::ScalesClass(byte dout, byte pd_sck, t_scales_value * value)
	: HX711(dout, pd_sck)
	, Task(300)
	, _value(value) {
	onRun(std::bind(&ScalesClass::takeWeight, this));
	_saveWeight.isSave = false;
	_saveWeight.value = 0.0;
}

void ScalesClass::begin() {	
	reset();
	if (_value->scale != _value->scale) {
		_value->scale = 0.0f;	
	}
	_offset_local = _value->offset;
#if BOARD == WEB_TERMINAL_MINI
	pinMode(RATE, OUTPUT);	
	digitalWrite(RATE, _value->rate);
#endif
	//_downloadValue();
	mathRound();
#if !defined(DEBUG_WEIGHT_RANDOM)  && !defined(DEBUG_WEIGHT_MILLIS)
	readAverage();
	//zero(_value->zero_on_range);		  
#endif // !DEBUG_WEIGHT_RANDOM && DEBUG_WEIGHT_MILLIS
	SetFilterWeight(_value->filter);
}

void ScalesClass::takeWeight() {
	fetchWeight();	
	if (_saveWeight.isSave) {
		Board->saveEvent("weight", _saveWeight.value);
		_saveWeight.isSave = false;
	}else {
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();
		if (overload()){
			json["cmd"] = "ovl";
		}else{			
			json["cmd"] = "wt";			
			doData(json);
			Board->battery()->doData(json);
		}
		String str = String();
		json.printTo(str);
		webSocket.textAll(str);
	}
	updateCache();
}

void ScalesClass::fetchWeight() {
	_weight = getWeight();
	detectAutoZero();
	detectStable(_weight);
}

long ScalesClass::readAverage() {
	long long sum = 0;
#ifdef DEBUG_WEIGHT_RANDOM	
	/*for (byte i = 0; i < _value->average; i++) {
		sum += analogRead(A0);
	}*/
	//randomSeed(1000);
	sum = random(8000000);
	Filter(_value->average == 0 ? sum / 1 : sum / _value->average);
#else
	for (byte i = 0; i < _value->average; i++) {
		sum += read();
	}
	Filter(_value->average == 0 ? sum / 1 : sum / _value->average);
#endif // DEBUG_WEIGHT_RANDOM
	
	return Current();
}

long ScalesClass::getValue() {
	int i = readAverage();
	return i - _offset_local;
}

float ScalesClass::getUnits() {
#ifdef DEBUG_WEIGHT_MILLIS
	static long long time = 0;
	float v = (micros() - time);
	time  = micros();
	return v;
#else
	float v = getValue();
	return (v * _value->scale);
#endif // DEBUG_WEIGHT_MILLIS
}

float ScalesClass::getWeight() {
	return round(getUnits() * _round) / _round; 
}

void ScalesClass::zero(float range) {
	_weight = getWeight();
	if (fabs(_weight) > (_value->max * range)) //если текущий вес больше диапазона нуля тогда не сбрасываем
		return;
	SetCurrent(read());
	offset(Current());
}

void ScalesClass::average(unsigned char a) {
	_value->average = constrain(a, 1, 5);
}

void ScalesClass::mathScale(float referenceW, long calibrateW) {
	_value->scale = referenceW / float(calibrateW - _value->offset);
}

/*! Функция для форматирования значения веса
	value - Форматируемое значение
	digits - Количество знаков после запятой
	accuracy - Точновть шага значений (1, 2, 5, ...)
	string - Выходная строка отфармотронова значение 
*/
void ScalesClass::formatValue(float value, char* string) {
	dtostrf(value, 6 - _value->accuracy, _value->accuracy, string);
}

/* */
void ScalesClass::detectStable(float w) {	
	if (_saveWeight.value != w) {
		_saveWeight.stabNum = STABLE_NUM_MAX;
		_stableWeight = false;
		_saveWeight.value = w;
		return;
	}
	
	if (_saveWeight.stabNum) {
		_saveWeight.stabNum--;
		return;
	}	
	if (_stableWeight) {
		return;
	}
	_stableWeight = true;
	if (millis() < _saveWeight.time)
		return;
	if (fabs(_saveWeight.value) > _weight_zero_auto) {
		_saveWeight.isSave = true;
		_saveWeight.time = millis() + 10000;
	}
}

void ScalesClass::detectAutoZero() {
	static long time;
	static bool flag;
	if (!_value->enable_zero_auto)
		return;
	if (fabs(_weight) > _weight_zero_auto)
		return;
	if (!flag) {
		flag = true;
		time = millis() + 60000;
	}
	if (time > millis())
		return;
	zero(_value->zero_man_range);
	flag = false;	
}

size_t ScalesClass::doData(JsonObject& json) {
	json["w"] = _weight;	
	json["a"] = _value->accuracy;
	json["s"] = _saveWeight.stabNum;
	return json.measureLength();	
}