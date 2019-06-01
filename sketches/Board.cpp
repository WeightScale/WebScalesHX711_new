#include "Board.h"
#include "MultiPointsPage.h"
#include "CalibratePage.h"

BoardClass * Board;

BoardClass::BoardClass() {
#ifdef INTERNAL_POWER
	_power = new PowerClass(EN_NCP, PWR_SW, std::bind(&BoardClass::powerOff, this));
	add(_power);
#endif	
	_blink = new BlinkClass();
	_memory = new MemoryClass<MyEEPROMStruct>(&_eeprom);
	if (!_memory->init()) {
		doDefault();
	}
	_battery = new BatteryClass(&_eeprom.settings.bat_min, &_eeprom.settings.bat_max);	
	_wifi = new WiFiModuleClass(&_eeprom);
	_scales = new ScalesClass(DOUT_PIN, SCK_PIN, &_eeprom.scales_value);
	CalibratePage = new CalibratePageClass(&_eeprom.scales_value);
#ifdef MULTI_POINTS_CONNECT
	MultiPointsPage = new MultiPointsPageClass(&_eeprom.settings);	
	_wifi->loadPoints();
#endif // MULTI_POINTS_CONNECT
	STAGotIP = WiFi.onStationModeGotIP(std::bind(&BoardClass::onSTAGotIP, this, std::placeholders::_1));	
	stationDisconnected = WiFi.onStationModeDisconnected(std::bind(&BoardClass::onStationDisconnected, this, std::placeholders::_1));
	stationConnected = WiFi.onStationModeConnected(std::bind(&BoardClass::onStationConnected, this, std::placeholders::_1));
};

void BoardClass::init() {
#ifdef INTERNAL_POWER
	_power->begin(&_eeprom.settings.time_off, &_eeprom.settings.power_time_enable);
#endif	
	add(_blink);
	add(_battery);
	add(_wifi);
	add(_scales);
	_scales->begin();
};

void /*ICACHE_RAM_ATTR*/ BoardClass::onSTAGotIP(const WiFiEventStationModeGotIP& evt) {
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	NBNS.begin(WiFi.hostname().c_str());
	onSTA();
}

void BoardClass::onStationConnected(const WiFiEventStationModeConnected& evt) {	
	WiFi.softAP(_eeprom.settings.hostName, SOFT_AP_PASSWORD, evt.channel);   //Устанавливаем канал как роутера
};
void BoardClass::onStationDisconnected(const WiFiEventStationModeDisconnected& evt) {	
	offSTA();
	NBNS.end();	
};

void BoardClass::handleBinfo(AsyncWebServerRequest *request) {
	if (!request->authenticate(_eeprom.scales_value.user, _eeprom.scales_value.password))
		if (!server.checkAdminAuth(request)) {
			return request->requestAuthentication();
		}
	if (request->args() > 0) {
		bool flag = false;
		if (request->hasArg("bmax")) {
			float t = request->arg("bmax").toFloat();
			_eeprom.settings.bat_max = CONVERT_V_TO_ADC(t);
			//CoreMemory.eeprom.settings.bat_max = CONVERT_V_TO_ADC(t);
			flag = true;
		}
		if (flag) {
			if (request->hasArg("bmin")) {
				float t = request->arg("bmin").toFloat();
				_eeprom.settings.bat_min = CONVERT_V_TO_ADC(t);
				//CoreMemory.eeprom.settings.bat_min = CONVERT_V_TO_ADC(t);
			}
			else {
				flag = false;
			}				
		}
		if (flag && _memory->save()) {
			goto url;
		}
		return request->send(400);
	}
url:
	request->send(SPIFFS, request->url());
}

bool BoardClass::saveEvent(const String& event, float value) {
	//String date = getDateTime();
	String str = "";
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	root["cmd"] = "swt";
	root["d"] = "";
	root["v"] = value;
	root["a"] = _scales->accuracy();
	
	root.printTo(str);
	webSocket.textAll(str);
	//Serial.flush();		
	return true;	
}

bool BoardClass::doDefault() {
	String u = F("admin");
	String p = F("1234");
	String apSsid = F("SCALES");
	u.toCharArray(_eeprom.scales_value.user, u.length() + 1);
	p.toCharArray(_eeprom.scales_value.password, p.length() + 1);
	u.toCharArray(_eeprom.settings.user, u.length() + 1);
	p.toCharArray(_eeprom.settings.password, p.length() + 1);
	apSsid.toCharArray(_eeprom.settings.hostName, apSsid.length() + 1);
	_eeprom.settings.bat_max = MAX_CHG;
	_eeprom.settings.bat_min = MIN_CHG;
#ifdef INTERNAL_POWER
	_eeprom.settings.time_off = 600000;
#endif
	_eeprom.settings.hostPin = 0;
#ifndef MULTI_POINTS_CONNECT
	_eeprom.settings.dnip = true;
#else
	_eeprom.settings.timeScan = 20;
	_eeprom.settings.deltaRSSI = 20;
#endif // MULTI_POINTS_CONNECT
	_eeprom.scales_value.accuracy = 1;
	_eeprom.scales_value.average = 1;
	_eeprom.scales_value.filter = 100;
	_eeprom.scales_value.max = 1000;
	_eeprom.scales_value.step = 1;
	_eeprom.scales_value.scale = 1;
	_eeprom.scales_value.zero_man_range = 0.02;
	_eeprom.scales_value.zero_on_range = 0.02;
	_eeprom.scales_value.zero_auto = 4;
	return _memory->save();
}

size_t BoardClass::doSettings(JsonObject &root) {
	JsonObject& scale = root.createNestedObject(SCALE_JSON);
#ifndef MULTI_POINTS_CONNECT
	scale["id_auto"] = _eeprom.settings.dnip;
	scale["id_lan_ip"] = _eeprom.settings.lanIp;
	scale["id_gateway"] = _eeprom.settings.gate;
	scale["id_subnet"] = _eeprom.settings.mask;
	scale["id_ssid"] = String(_eeprom.settings.wSSID);
	scale["id_key"] = String(_eeprom.settings.wKey);
#else	
	root["id_t_scan"] = _eeprom.settings.timeScan;
	root["id_d_rssi"] = _eeprom.settings.deltaRSSI;
	root["id_escan"] = _eeprom.settings.enable_scan;
#endif
#ifdef INTERNAL_POWER
	scale["id_pe"] = _eeprom.settings.power_time_enable;
	scale["id_pt"] = _eeprom.settings.time_off;
#endif
	root["id_host"] = _eeprom.settings.hostName;
	root["id_nadmin"] = _eeprom.settings.user;
	root["id_padmin"] = _eeprom.settings.password;	
	
	JsonObject& server = root.createNestedObject(SERVER_JSON);
	server["id_host"] = String(_eeprom.settings.hostUrl);
	server["id_pin"] = _eeprom.settings.hostPin;
	
	JsonObject& info = root.createNestedObject("info");
	
	info["id_net"] = WiFi.SSID();
	info["id_sta_ip"] = WiFi.localIP().toString();
	info["id_rssi"] = String(min(max(2 * (WiFi.RSSI() + 100), 0), 100)) + "%"; 
	info["id_vr"] = SKETCH_VERSION;
	return root.measureLength();
};