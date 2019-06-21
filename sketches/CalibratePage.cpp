#include "CalibratePage.h"
#include "Board.h"
#include "BrowserServer.h"

CalibratePageClass * CalibratePage;

CalibratePageClass::CalibratePageClass(scales_value_t * value)	: _value(value) {};

bool CalibratePageClass::canHandle(AsyncWebServerRequest *request) {	
	if (request->url().equalsIgnoreCase(F("/calibr.html"))) {
		if (!request->authenticate(_value->user, _value->password)) {
			if (!server.checkAdminAuth(request)) {
				request->requestAuthentication();
				return false;
			}
		}
		return true;
	}
	else
		return false;
}

void CalibratePageClass::handleRequest(AsyncWebServerRequest *request) {
	if (request->args() > 0) {
		if (request->hasArg("update")) {
			_value->accuracy = request->arg(F("weightAccuracy")).toInt();
			_value->step = request->arg(F("weightStep")).toInt();
			Board->scales()->average(request->arg(F("weightAverage")).toInt());
			Board->scales()->SetFilterWeight(request->arg(F("weightFilter")).toInt());
			_value->filter = Board->scales()->GetFilterWeight();
			_value->max = request->arg(F("weightMax")).toFloat();
			_value->zero_man_range = request->arg(F("zm_range")).toFloat();
			_value->zero_on_range = request->arg(F("zo_range")).toFloat();
			_value->zero_auto = request->arg(F("zd_auto")).toInt();
			//_weight_zero_range = _scales_value->max * _scales_value->zero_range; /* Диапазон нуля */
			if(request->hasArg("z_en_auto"))
				_value->enable_zero_auto = true;
			else
				_value->enable_zero_auto = false;
			
#if BOARD == WEB_TERMINAL2
			if (request->hasArg("p5v"))
				_value->power5v = true;
			else
				_value->power5v = false;
			digitalWrite(EN_MT, _value->power5v);
#endif

#if BOARD == WEB_TERMINAL_MINI
			if (request->hasArg("rate"))
				_value->rate = true;
			else
				_value->rate = false;
			digitalWrite(RATE, _value->rate);
#endif

			Board->scales()->mathRound();
			if (saveValue()) {				
				goto ok;
			}
			goto err;
		}
		
		if (request->hasArg("zero")) {
			//Board->scales()->tare();
			Board->scales()->SetCurrent(Board->scales()->read());
			_value->offset = Board->scales()->Current();
			Board->scales()->offset(_value->offset);
		}
		
		if (request->hasArg(F("weightCal"))) {
			float rw = request->arg(F("weightCal")).toFloat();
			Board->scales()->SetCurrent(Board->scales()->read());
			long cw = Board->scales()->Current();
			//long cw = readAverage();
			mathScale(rw, cw);
		}
		
		if (request->hasArg("user")) {
			request->arg("user").toCharArray(_value->user, request->arg("user").length() + 1);
			request->arg("pass").toCharArray(_value->password, request->arg("pass").length() + 1);
			if (saveValue()) {
				goto url;
			}
			goto err;
		}
		
ok:
		return request->send(200, F("text/html"), "");
err:
		return request->send(204);	
	}
url:
#ifdef HTML_PROGMEM
	request->send_P(200, F("text/html"), calibr_html);
#else
	request->send(SPIFFS, request->url());
#endif
}

void CalibratePageClass::handleValue(AsyncWebServerRequest * request) {
	if (!request->authenticate(_value->user, _value->password)) {
		if (!server.checkAdminAuth(request)) {
			return request->requestAuthentication();
		}
	}
		
	AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.createObject();
	doCalibrateValue(json);	
	json.printTo(*response);
	request->send(response);
}

size_t CalibratePageClass::doCalibrateValue(JsonObject& root) {
#if BOARD == WEB_TERMINAL_MINI
	root[RATE_JSON] = _value->rate;
#endif
#if BOARD == WEB_TERMINAL2
	root[POWER_5V] = _value->power5v;
#endif
	root[STEP_JSON] = _value->step;
	root[AVERAGE_JSON] = _value->average;
	root[WEIGHT_MAX_JSON] = _value->max;
	root["zm_id"] = _value->zero_man_range;
	root["zo_id"] = _value->zero_on_range;
	root["auto_en_id"] = _value->enable_zero_auto;
	root["zd_id"] = _value->zero_auto;
	root[OFFSET_JSON] = _value->offset;
	root[ACCURACY_JSON] = _value->accuracy;
	root[SCALE_JSON] = _value->scale;
	root[FILTER_JSON] = _value->filter;
	root[SEAL_JSON] = _value->seal;
	root[USER_JSON] = _value->user;
	root[PASS_JSON] = _value->password;
	return root.measureLength();
}

bool CalibratePageClass::saveValue() {
	return Board->memory()->save();
};