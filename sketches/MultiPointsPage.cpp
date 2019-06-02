#include "MultiPointsPage.h"
#include "Board.h"
#include "BrowserServer.h"

MultiPointsPageClass * MultiPointsPage;

bool MultiPointsPageClass::canHandle(AsyncWebServerRequest *request) {	
#ifdef MULTI_POINTS_CONNECT
	if (request->url().equalsIgnoreCase(F("/settings.html"))) {
#else
		if (request->url().equalsIgnoreCase(F("/soft.html"))) {
#endif // MULTI_POINTS_CONNECT	
			if (!request->authenticate(_user, _password)) {
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

void MultiPointsPageClass::handleRequest(AsyncWebServerRequest *request) {
	if (request->args() > 0) {
		String message = " ";
#ifdef MULTI_POINTS_CONNECT	
		if (request->hasArg("delete")) {
			if (Board->wifi()->removePoint(request->arg("ssid"))) {
				//Board->wifi()->loadPoints();
				if(WiFi.SSID().equals(request->arg("ssid"))) {
					Board->wifi()->isUpdate(true);
				}								
				goto url;
			}
				
		}else if (request->hasArg("ssid")) {
			EntryWiFi p;
			p.ssid =  request->arg("ssid");
			p.passphrase = request->arg("key");
			/**/
			if (request->hasArg("dnip"))
				p.dnip = true;
			else
				p.dnip = false;
			p.ip = request->arg("lan_ip");
			p.gate = request->arg("gateway");
			p.mask = request->arg("subnet");						

			if (Board->wifi()->savePoint(p)) {
				Board->wifi()->loadPoints();
				Board->wifi()->isUpdate(true);
				goto url;
			}			
#endif // MULTI_POINTS_CONNECT
			
			goto err;
		}
		if (request->hasArg("host")) {	
			if (request->hasArg("escan"))
				_value->enable_scan = true;
			else
				_value->enable_scan = false;
			_value->enable_scan ? Board->wifi()->resume() : Board->wifi()->pause();	
			_value->timeScan = request->arg("t_scan").toInt();
			Board->wifi()->setInterval(_value->timeScan * 1000);
			_value->deltaRSSI = request->arg("d_rssi").toInt();			
			goto save;
		}
save:
		if (Board->memory()->save()) {
			goto url;
		}
err:
		return request->send(400);
	}
url:
#ifdef HTML_PROGMEM
	
#ifdef MULTI_POINTS_CONNECT
		request->send_P(200, F("text/html"), softm_html);
#else
		request->send_P(200, F("text/html"), soft_html);
#endif // MULTI_POINTS_CONNECT
#else
		request->send(SPIFFS, request->url());
#endif
}

void MultiPointsPageClass::handleValue(AsyncWebServerRequest * request) {
	if (!request->authenticate(_user, _password)) {
		if (!server.checkAdminAuth(request)) {
			return request->requestAuthentication();
		}
	}
	
	AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	doSettings(root);	
	root.printTo(*response);
	request->send(response);
}
	
size_t MultiPointsPageClass::doSettings(JsonObject &root) {
	JsonObject& scale = root.createNestedObject(SCALE_JSON);
#ifndef MULTI_POINTS_CONNECT
	scale["id_auto"] = _eeprom.settings.dnip;
	scale["id_lan_ip"] = _eeprom.settings.lanIp;
	scale["id_gateway"] = _eeprom.settings.gate;
	scale["id_subnet"] = _eeprom.settings.mask;
	scale["id_ssid"] = String(_eeprom.settings.wSSID);
	scale["id_key"] = String(_eeprom.settings.wKey);
#else	
	root["id_t_scan"] = _value->timeScan;
	root["id_d_rssi"] = _value->deltaRSSI;
	root["id_escan"] = _value->enable_scan;
#endif
	return root.measureLength();
};	