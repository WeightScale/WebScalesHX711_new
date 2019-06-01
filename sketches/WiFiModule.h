#pragma once
#include <ESP8266WiFi.h>
#include <ESP8266NetBIOS.h>
#include "Task.h"
#include <functional>
#include <StringArray.h>
#include <ArduinoJson.h>
#include "Config.h"

#define MAX_POINTS				10

struct EntryWiFi {
	String ssid;
	String passphrase;
	bool dnip;
	String ip;
	String gate;
	String mask;
};

typedef std::vector<EntryWiFi> Wifilist;

class WiFiModuleClass : /*public ESP8266WiFiClass,*/ public Task {
private:	
	IPAddress _lanIp;
	IPAddress _gate;
#ifdef MULTI_POINTS_CONNECT
	Wifilist _accessPoints;
	int cached_size;
	bool _enableSSID = true; /* если нет сети в списке*/
	bool _downloadValue();
	//bool _uploadValue();
	bool _dnip;
	bool _isUdate = false;
	bool _Scan = false;
	bool _EnableAP = false;
	unsigned long _time_connect;
#else
	//EntryWiFi _entryWiFi;
#endif // MULTI_POINTS_CONNECT
	//EntryWiFi _currentEntryWiFi;
	MyEEPROMStruct * _value;
	String _hostName;		
public:	
	WiFiModuleClass(MyEEPROMStruct * value);
	WiFiModuleClass(char *host);
#ifdef MULTI_POINTS_CONNECT
	~WiFiModuleClass() {_accessPoints.clear(); };
	Wifilist points() {return _accessPoints;};
	bool EnableAP() {return _EnableAP;};
#else
	~WiFiModuleClass() {free(_value);};
	void scanResultForConnect(int networksFound);
#endif // MULTI_POINTS_CONNECT
	//wl_status_t connect();
	//void connectAp(const EntryWiFi &entry);
	void connect();		
#ifdef MULTI_POINTS_CONNECT
	bool savePoint(EntryWiFi point);
	bool removePoint(const String &name);	
	void scan();
	bool addPoint(const String &ssid, const String &pass) {
		const char *s = ssid.c_str();
		const char *p = pass.c_str();
		return addPoints(s, p);
		//return true;
	};
	bool /*ICACHE_RAM_ATTR*/ addPoints(const char* ssid, const char *passphrase = NULL) {
		EntryWiFi newAP;

		if (!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
			// fail SSID to long or missing!
			return false;
		}

		//for passphrase, max is 63 ascii + null. For psk, 64hex + null.
		if(passphrase && strlen(passphrase) > 64) {
			// fail passphrase to long!
			return false;
		}

		newAP.ssid = ssid;

		if (!newAP.ssid) {
			return false;
		}

		if (passphrase) {
			newAP.passphrase = passphrase;
		}
		else {
			newAP.passphrase = "";
		}

		if (!newAP.passphrase) {			
			return false;
		}
		//_accessPoints.push_back(newAP);
		//APlist.push_back(newAP);
		//return true;
		return addPoint(newAP);
	};
	bool /*ICACHE_RAM_ATTR*/ addPoint(EntryWiFi _point) {		
		// Check if the Thread already exists on the array
		for(auto p : _accessPoints) {
			if (p.ssid.equals(_point.ssid))
				return true;					
		}
		if (_accessPoints.size() < MAX_POINTS) {
			_accessPoints.push_back(_point);
			cached_size++;
			return true;
		}		
		// Array is full
		return false;
	} 
	void loadPoints() {		
		_accessPoints.clear();
		_downloadValue();
	};	
	bool isUpdate() {return _isUdate;};
	void isUpdate(bool u) {_isUdate = u; };
#endif // MULTI_POINTS_CONNECT	
	void printScanResult(int networksFound);
	String hostName() {return _hostName;};
};
extern IPAddress apIP;
extern IPAddress netMsk;