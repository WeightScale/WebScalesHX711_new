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
			request->arg("host").toCharArray(_value->hostName, request->arg("host").length() + 1);
			_value->timeScan = request->arg("t_scan").toInt();
			Board->wifi()->setInterval(_value->timeScan * 1000);
			_value->deltaRSSI = request->arg("d_rssi").toInt();
			request->arg("nadmin").toCharArray(_value->user, request->arg("nadmin").length() + 1);
			request->arg("padmin").toCharArray(_value->password, request->arg("padmin").length() + 1);
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
	if (!request->authenticate(_value->user, _value->password)) {
		if (!server.checkAdminAuth(request)) {
			return request->requestAuthentication();
		}
	}
	
	AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	Board->doSettings(root);	
	root.printTo(*response);
	request->send(response);
}