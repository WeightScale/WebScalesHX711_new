#include "Battery.h"
#include "Config.h"
#include "BrowserServer.h"
#include "tools.h"
#include "Board.h"

//BatteryClass* BATTERY;

BatteryClass::BatteryClass(settings_t * value)	: Task(20000), _value(value) {
	/* 20 Обновляем заряд батареи */
	onRun(std::bind(&BatteryClass::fetchCharge, this));
	_max = &_value->bat_max;
	_min = &_value->bat_min;	
	fetchCharge();
#ifdef DEBUG_BATTERY
	_isDischarged = false;
#endif // DEBUG_BATTERY
}

int BatteryClass::fetchCharge() {
	_charge = _get_adc(1);
	//_charge = Constrain(_charge, this->_min, this->_max);
	_charge = constrain(_charge, *_min, *_max);
	_charge = map(_charge, *_min, *_max, 0, 100);
	//_charge = Map(_charge, _min, _max, 0, 100);
	_isDischarged = _charge <= 5;
	/*if (_isDischarged) {
		ws.textAll("{\"cmd\":\"dchg\"}");
	}*/
	return _charge;
}

int BatteryClass::_get_adc(byte times) {
	long sum = 0;
#ifdef DEBUG_BATTERY
	for (byte i = 0; i < times; i++) {
		sum += random(ADC);
	}	
#else
	for (byte i = 0; i < times; i++) {
		sum += analogRead(V_BAT);
	}
#endif // DEBUG_BATTERY	
	return times == 0 ? sum : sum / times;	
}

size_t BatteryClass::doInfo(JsonObject& json){
	json["id_min"] = CALCULATE_VIN(*_min);
	json["id_max"] = CALCULATE_VIN(*_max);
	json["id_cvl"] = CALCULATE_VIN(_get_adc(1));
	return json.measureLength();	
}

size_t BatteryClass::doData(JsonObject& json) {
	json["c"] = _charge;
	return json.measureLength();
};

void BatteryClass::handleBinfo(AsyncWebServerRequest *request) {
	if (!request->authenticate(_value->user, _value->password))
		if (!server.checkAdminAuth(request)) {
			return request->requestAuthentication();
		}
	if (request->args() > 0) {
		bool flag = false;
		if (request->hasArg("bmax")) {
			float t = request->arg("bmax").toFloat();
			_value->bat_max = CONVERT_V_TO_ADC(t);
			//CoreMemory.eeprom.settings.bat_max = CONVERT_V_TO_ADC(t);
			flag = true;
		}
		if (flag) {
			if (request->hasArg("bmin")) {
				float t = request->arg("bmin").toFloat();
				_value->bat_min = CONVERT_V_TO_ADC(t);
				//CoreMemory.eeprom.settings.bat_min = CONVERT_V_TO_ADC(t);
			}
			else {
				flag = false;
			}				
		}
		if (flag && Board->memory()->save()) {
			goto url;
		}
		return request->send(400);
	}
url:
	request->send(SPIFFS, request->url());
}
