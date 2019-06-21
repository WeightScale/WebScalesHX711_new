#include "SettingsPage.h"
#include "Board.h"
#include "BrowserServer.h"

SettingsPageClass * SettingsPage;

bool SettingsPageClass::canHandle(AsyncWebServerRequest *request) {	
	if (request->url().equalsIgnoreCase(F("/settings.html"))) {
		goto auth;
	}
#ifndef HTML_PROGMEM
	else if (request->url().equalsIgnoreCase("/sn")) {
		goto auth;
	}
#endif
	else
		return false;
auth:
	if (!request->authenticate(_value->user, _value->password)) {
		if (!server.checkAdminAuth(request)) {
			request->requestAuthentication();
			return false;
		}
	}
	return true;
}

void SettingsPageClass::handleRequest(AsyncWebServerRequest *request) {
	if (request->args() > 0) {
		String message = " ";
		if (request->hasArg("host")) {	
			request->arg("host").toCharArray(_value->hostName, request->arg("host").length() + 1);
			request->arg("nadmin").toCharArray(_value->user, request->arg("nadmin").length() + 1);
			request->arg("padmin").toCharArray(_value->password, request->arg("padmin").length() + 1);
			
			if (Board->memory()->save())
				goto url;
			else
				return request->send(400);
		}		
		return request->send(204);
	}
url:
#ifdef HTML_PROGMEM
	request->send_P(200, F("text/html"), settings_html);
#else
	if (request->url().equalsIgnoreCase("/sn"))
		request->send_P(200, F("text/html"), netIndex);
	else
		request->send(SPIFFS, request->url());
#endif
}

void SettingsPageClass::handleValue(AsyncWebServerRequest * request) {
	if (!request->authenticate(_value->user, _value->password)) {
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

size_t SettingsPageClass::doSettings(JsonObject &root) {
	JsonObject& scale = root.createNestedObject(SCALE_JSON);
#ifdef INTERNAL_POWER
	root["id_pe"] = _value->power_time_enable;
	root["id_pt"] = _value->time_off;
#endif
	root["id_host"] = _value->hostName;
	root["id_nadmin"] = _value->user;
	root["id_padmin"] = _value->password;	
	
	JsonObject& server = root.createNestedObject(SERVER_JSON);
	server["id_host"] = String(_value->hostUrl);
	server["id_pin"] = _value->hostPin;
	
	JsonObject& info = root.createNestedObject("info");
	
	info["id_net"] = WiFi.SSID();
	info["id_sta_ip"] = WiFi.localIP().toString();
	info["id_ap_ip"] = WiFi.softAPIP().toString();
	info["id_rssi"] = String(min(max(2 * (WiFi.RSSI() + 100), 0), 100)) + "%"; 
	info["sl_id"] = String(Board->scales()->seal());
	info["id_mac"] = WiFi.macAddress();
	info["id_vr"] = SKETCH_VERSION;
	return root.measureLength();
};