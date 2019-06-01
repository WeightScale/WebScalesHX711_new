#pragma once
#include <Arduino.h>
#include "TaskController.h"
#include <ESPAsyncWebServer.h>
//#include "CoreMemory.h"
#include "Config.h"



const char netIndex[] PROGMEM = R"(<html lang='en'><meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1'/><body><form method='POST'><input name='ssid'><br/><input type='password' name='key'><br/><input type='submit' value='СОХРАНИТЬ'></form></body></html>)";

#define SETTINGS_FILE "/settings.json"

#define SCALE_JSON		"scale"
#define SERVER_JSON		"server"
//#define DATE_JSON		"date"
#define EVENTS_JSON		"events"

extern TaskController taskController;/*  */
extern Task taskBlink;/*  */
extern Task taskConnectWiFi;
extern void connectWifi();

class CoreClass : public AsyncWebHandler {
private:
	settings_t * _settings;

	String _hostname;
	String _username;
	String _password;
	bool _authenticated;


public:
	CoreClass(const String& username, const String& password);
	~CoreClass();
	void begin();
	char* getNameAdmin(){return _settings->scaleName;};
	char* getPassAdmin(){return _settings->scalePass;};
	char* getSSID(){return _settings->wSSID;};
	char* getLanIp(){return _settings->scaleLanIp;};
	char* getGateway(){return _settings->scaleGateway;};
	char* getPASS(){return _settings->wKey;};
	char* getApSSID() { return _settings->apSSID; };
	bool saveEvent(const String&, const String&);
	bool eventToServer(const String&, const String&, const String&);
	String getHash(const int, const String&, const String&, const String&);
	int getPin(){return _settings->hostPin;};
	String& getHostname() { return _hostname; };
	bool isAuto(){return _settings->autoIp;};
	virtual bool canHandle(AsyncWebServerRequest *request) override final;
	virtual void handleRequest(AsyncWebServerRequest *request) override final;
	virtual bool isRequestHandlerTrivial() override final {return false;};
};




void reconnectWifi(AsyncWebServerRequest*);
extern CoreClass * CORE;
//extern BlinkClass * BLINK;