#include "BrowserServer.h"
#include <ESPAsyncWebServer.h>
//#include "Core.h"
#include <SPIFFSEditor.h>
#include "HttpUpdater.h"
#include <functional>
#include "Battery.h"
#include "Board.h"
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "DateTime.h"
#include "tools.h"
#include "MultiPointsPage.h"
#include "CalibratePage.h"

/* Soft AP network parameters */
//IPAddress apIP(192,168,4,1);
//IPAddress netMsk(255, 255, 255, 0);

IPAddress lanIp;			// Надо сделать настройки ip адреса
IPAddress gateway;

BrowserServerClass server(80,"sa","343434");
AsyncWebSocket webSocket("/ws");
//AsyncEventSource events("/events");
AsyncDNSServer dnsServer;

BrowserServerClass::BrowserServerClass(uint16_t port, char * username, char * password)	: AsyncWebServer(port) {
	_httpAuth.wwwUsername = username;
	_httpAuth.wwwPassword = password;
}

BrowserServerClass::~BrowserServerClass(){}

void BrowserServerClass::begin(){
	/* Setup the DNS server redirecting all the domains to the apIP */
	dnsServer.setTTL(300);
	dnsServer.setErrorReplyCode(AsyncDNSReplyCode::ServerFailure);
	webSocket.onEvent(onWsEvent);
	addHandler(&webSocket);
	//addHandler(&events);
	//CORE = new CoreClass(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str());	
	//addHandler(CORE);
	addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
	addHandler(new SPIFFSEditor(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str()));	
	//addHandler(new HttpUpdaterClass("sa", "654321"));
	addHandler(CalibratePage);
#ifdef MULTI_POINTS_CONNECT
	addHandler(MultiPointsPage);
#endif // MULTI_POINTS_CONNECT
	init();
	dnsServer.start(DNS_PORT, "*", apIP);
	AsyncWebServer::begin(); // Web server start
}

void BrowserServerClass::init(){
	on("/settings.json", HTTP_ANY, std::bind(&MultiPointsPageClass::handleValue, MultiPointsPage, std::placeholders::_1));
	on("/cdate.json", HTTP_ANY, std::bind(&CalibratePageClass::handleValue, CalibratePage, std::placeholders::_1));
	on("/rc",[](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), "<meta http-equiv='refresh' content='10;URL=/'>RECONNECT...");
		response->addHeader(F("Connection"), F("close"));
		request->onDisconnect([]() {
			SPIFFS.end();
			ESP.reset();
		});
		request->send(response);
	});									/* Пересоединиться по WiFi. */
	//on("/sv", handleScaleProp);									/* Получить значения. */	
	on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
		String str = String("Heap: ");
		str += String(ESP.getFreeHeap());
		str += " client: ";
		str += String(webSocket.count());
		request->send(200, F("text/plain"), str);
	});
	on("/rst",HTTP_ANY,[this](AsyncWebServerRequest * request){
		if (!isAuthentified(request)){
			return request->requestAuthentication();
		}
		if(Board->doDefault())
			request->send(200,F("text/html"), F("Установлено!"));
		else
			request->send(400);
	});
	//on("/rssi", handleRSSI);
	//on("/binfo.html", std::bind(&BoardClass::handleBinfo, Board, std::placeholders::_1));
#ifdef HTML_PROGMEM
	on("/",[](AsyncWebServerRequest * reguest){	reguest->send_P(200,F("text/html"),index_html);});								/* Главная страница. */	 
	on("/global.css",[](AsyncWebServerRequest * reguest){	reguest->send_P(200,F("text/css"),global_css);});					/* Стили */
	/*on("/favicon.png",[](AsyncWebServerRequest * request){
		AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", favicon_png, favicon_png_len);
		request->send(response);
	});*/	
	on("/bat.png",handleBatteryPng);
	on("/scales.png",handleScalesPng);	
	on("/und.png",[](AsyncWebServerRequest * request) {
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/png"), und_png, und_png_len) ;
		request->send(response) ;
	});
	on("/set.png",[](AsyncWebServerRequest * request) {
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/png"), set_png, set_png_len);
		request->send(response);
	});
	on("/zerow.png",[](AsyncWebServerRequest * request) {
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/png"), zerow_png, zerow_png_len);
		request->send(response);
	});
	serveStatic("/", SPIFFS, "/");
#else
	rewrite("/sn", "/settings.html");
	serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");	
#endif
	if (Board->battery()->isDischarged()){
		on("/ds",
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/balert.html");
				response->addHeader(F("Connection"), F("close"));
				request->onDisconnect([]() {
					ESP.deepSleep(20000);
				});
				request->send(response);	
			});
		rewrite("/", "/index.html");
		rewrite("/index.html", "/ds");
	}
	//serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");					
	onNotFound([](AsyncWebServerRequest *request){
		request->send(404);
	});
}

bool BrowserServerClass::checkAdminAuth(AsyncWebServerRequest * r) {	
	return r->authenticate(_httpAuth.wwwUsername.c_str(), _httpAuth.wwwPassword.c_str());
}

bool BrowserServerClass::isAuthentified(AsyncWebServerRequest * request){
	/*if (!request->authenticate(CORE->getNameAdmin(), CORE->getPassAdmin())){
		if (!checkAdminAuth(request)){
			return false;
		}
	}*/
	return true;
}

void handleSettings(AsyncWebServerRequest * request){
	if (!server.isAuthentified(request))
		return request->requestAuthentication();
	AsyncResponseStream *response = request->beginResponseStream("application/json");
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	Board->doSettings(root);
	/*JsonObject& scale = root.createNestedObject(SCALE_JSON);
	scale["id_auto"] = CoreMemory.eeprom.settings.autoIp;
	scale["id_pe"] = CoreMemory.eeprom.settings.power_time_enable;
	scale["id_pt"] = CoreMemory.eeprom.settings.time_off;
	scale["id_assid"] = CoreMemory.eeprom.settings.apSSID;
	scale["id_n_admin"] = CoreMemory.eeprom.settings.scaleName;
	scale["id_p_admin"] = CoreMemory.eeprom.settings.scalePass;
	scale["id_lan_ip"] = CoreMemory.eeprom.settings.scaleLanIp;
	scale["id_gateway"] = CoreMemory.eeprom.settings.scaleGateway;
	scale["id_subnet"] = CoreMemory.eeprom.settings.scaleSubnet;
	scale["id_ssid"] = String(CoreMemory.eeprom.settings.wSSID);
	scale["id_key"] = String(CoreMemory.eeprom.settings.wKey);
	
	JsonObject& server = root.createNestedObject(SERVER_JSON);
	server["id_host"] = String(CoreMemory.eeprom.settings.hostUrl);
	server["id_pin"] = CoreMemory.eeprom.settings.hostPin;*/
	
	root.printTo(*response);
	request->send(response);
}

void handleFileReadAuth(AsyncWebServerRequest * request){
	if (!server.isAuthentified(request)){
		return request->requestAuthentication();
	}
	request->send(SPIFFS, request->url());
}

void handleScaleProp(AsyncWebServerRequest * request){
	if (!server.isAuthentified(request))
		return request->requestAuthentication();
	AsyncJsonResponse * response = new AsyncJsonResponse();
	JsonObject& root = response->getRoot();
	root["id_date"] = getDateTime();
	root["id_local_host"] = WiFi.hostname();
	//root["id_ap_ssid"] = String(CORE->getApSSID());
	root["id_ap_ip"] = toStringIp(WiFi.softAPIP());
	root["id_ip"] = toStringIp(WiFi.localIP());
	root["sl_id"] = String(Board->scales()->seal());
	root["chip_v"] = String(ESP.getCpuFreqMHz());
	root["id_mac"] = WiFi.macAddress();
	root["id_vr"] = SKETCH_VERSION;
	response->setLength();
	request->send(response);
}

#ifdef HTML_PROGMEM
	void handleBatteryPng(AsyncWebServerRequest * request){
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/png"), bat_png, bat_png_len);
		request->send(response);
	}

	void handleScalesPng(AsyncWebServerRequest * request){	
		AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/png"), scales_png, scales_png_len);
		request->send(response);
	}
#endif // HTML_PROGMEM

void handleRSSI(AsyncWebServerRequest * request){
	request->send(200, F("text/html"), String(WiFi.RSSI()));
}

void /*ICACHE_FLASH_ATTR*/ printScanResult(int networksFound) {
	// sort by RSSI
	int n = networksFound;
	int indices[n];
	int skip[n];
	for (int i = 0; i < networksFound; i++) {
		indices[i] = i;
	}
	for (int i = 0; i < networksFound; i++) {
		for (int j = i + 1; j < networksFound; j++) {
			if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
				std::swap(indices[i], indices[j]);
				std::swap(skip[i], skip[j]);
			}
		}
	}
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	root["cmd"] = "ssl";
	JsonArray &scan = root.createNestedArray("list");
	for (int i = 0; i < 5 && i < networksFound; ++i) {
		JsonObject &item = scan.createNestedObject();
		item["ssid"] = WiFi.SSID(indices[i]);
		item["rssi"] = WiFi.RSSI(indices[i]);
	}
	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer *buffer = webSocket.makeBuffer(len);   //  creates a buffer (len + 1) for you.
	if(buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		webSocket.textAll(buffer);
	}
	WiFi.scanDelete();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
	if(type == WS_EVT_CONNECT){	
		if (server->count() > MAX_WEBSOCKET_CLIENT) {
			client->text("{\"cmd\":\"cls\",\"code\":1111}");
		}
	}else if(type == WS_EVT_DISCONNECT) {
		//client->close(true);
	}else if(type == WS_EVT_ERROR) {
		client->close(true);
	}else if (type == WS_EVT_DATA) {
		//String msg = "";
		//for(size_t i=0; i < len; i++) {
		//	msg += (char) data[i];
		//}
		DynamicJsonBuffer jsonBuffer(len);
		JsonObject &root = jsonBuffer.parseObject(data);
		if (!root.success()) {
			return;
		}
		const char *command = root["cmd"];			/* Получить показания датчика*/
		JsonObject& json = jsonBuffer.createObject();
		json["cmd"] = command;
#ifdef MULTI_POINTS_CONNECT
		if (strcmp(command, "gpoint") == 0) {
			JsonArray& points = json.createNestedArray("points");			
			for (auto point : Board->wifi()->points()) {
				JsonObject& p = jsonBuffer.createObject();
				p["ssid"] = point.ssid;
				p["pass"] = point.passphrase;
				p["dnip"] = point.dnip;
				p["ip"] = point.ip;
				p["gate"] = point.gate;
				p["mask"] = point.mask;
				points.add(p);
			}				
		}else 
#endif //MULTI_POINTS_CONNECT
		if (strcmp(command, "wt") == 0){
			Board->scales()->doData(json);
			Board->battery()->doData(json);				
			//BATTERY->doData(json);
			#ifdef INTERNAL_POWER
				Board->power()->updateCache();
			#endif
		}else if (strcmp(command, "tp") == 0){
			#if !defined(DEBUG_WEIGHT_RANDOM)  && !defined(DEBUG_WEIGHT_MILLIS)
				Scale.zero(CoreMemory.eeprom.scales_value.zero_man_range);
			#endif 
		}else if (strcmp(command, "scan") == 0) {
			WiFi.scanNetworksAsync(printScanResult, true);
			return;
		}else if (strcmp(command, "binfo") == 0) {
			Board->battery()->doInfo(json);
			//BATTERY->doInfo(json);
		}else {
			return;
		}
		size_t lengh = json.measureLength();
		AsyncWebSocketMessageBuffer * buffer = webSocket.makeBuffer(lengh);
		if (buffer) {
			json.printTo((char *)buffer->get(), lengh + 1);
			if (client) {
				client->text(buffer);
			}else{
				delete buffer;
			}
		}
	}
}