#include "WiFiModule.h"
#include <FS.h>
#include <ArduinoJson.h>
#include <limits.h>
#include <string.h>
#include "BrowserServer.h"
#include "Board.h"

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

WiFiModuleClass::WiFiModuleClass(MyEEPROMStruct * value)
	: _value(value),
#ifdef MULTI_POINTS_CONNECT
	Task(value->settings.timeScan * 1000) {
#else
	Task(20000) {
#endif // MULTI_POINTS_CONNECT
	_hostName = String(_value->settings.hostName);
	WiFi.persistent(false);
	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(false);
	WiFi.setPhyMode(WIFI_PHY_MODE_11G);	
	WiFi.mode(WIFI_AP_STA);		
#ifdef MULTI_POINTS_CONNECT
	onRun(std::bind(&WiFiModuleClass::scan, this));
	if (!_value->settings.enable_scan){
		pause();
		_Scan = false;			
	}
	//onRun(std::bind(&WiFiModuleClass::connect, this));
#else
	onRun(std::bind(&WiFiModuleClass::connect, this));
	//setInterval(30000);
	if (!_value->settings.dnip) {
		if (_lanIp.fromString(_value->settings.lanIp) && _gate.fromString(_value->settings.gate)&&netMsk.fromString(_value->settings.mask)) {
			WiFi.config(_lanIp, _gate, netMsk);     									// Надо сделать настройки ip адреса
		}
	}	
#endif // MULTI_POINTS_CONNECT
	WiFi.softAPConfig(apIP, apIP, netMsk);
	WiFi.softAP(_value->settings.hostName, SOFT_AP_PASSWORD);	
	_hostName.toLowerCase();
	WiFi.hostname(_hostName);
};

/**/void /*ICACHE_RAM_ATTR*/ WiFiModuleClass::connect() {	
#ifdef MULTI_POINTS_CONNECT
	if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
		return;	
	wl_status_t _status = WiFi.status();
	if (!_isUdate && !_Scan)
	{		
		if (_status != WL_DISCONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_IDLE_STATUS && _status != WL_CONNECT_FAILED) {
			//_connecting = false;
			return;
		}
		else {
			if (_time_connect > millis())
				return;
		}
		if (_EnableAP) {
			return;	
		}
	}	
	int scanResult = WiFi.scanComplete();
	if (scanResult == WIFI_SCAN_RUNNING) {
		// scan is running, do nothing yet
		_status = WL_NO_SSID_AVAIL;
		return;
	} 
		
	if (scanResult == 0) {
		WiFi.scanDelete();
		delay(0);
		WiFi.disconnect();
		// scan wifi async mode
		WiFi.scanNetworks(true);
	}
	else if (scanResult == WIFI_SCAN_FAILED) {
		//WiFi.disconnect();		
		WiFi.scanNetworks(true);
	}
	else if (scanResult > 0) {
		// scan done, analyze
		EntryWiFi bestNetwork;
		int bestNetworkDb = INT_MIN;
		unsigned int bestRSSI;
		unsigned int currRSSI;
		uint8 bestBSSID[6];
		int32_t bestChannel;

		delay(0);
	
		for (int8_t i = 0; i < scanResult; ++i) {

			String ssid_scan;
			int32_t rssi_scan;
			uint8_t sec_scan;
			uint8_t* BSSID_scan;
			int32_t chan_scan;
			bool hidden_scan;

			WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);			
			if (WiFi.SSID().equals(ssid_scan))
				currRSSI = min(max(2 * (rssi_scan + 100), 0), 100);
			for (auto &entry : _accessPoints) {
				if (ssid_scan == entry.ssid) {
					if (rssi_scan > bestNetworkDb) {
						// best network
					   if(sec_scan == ENC_TYPE_NONE || entry.passphrase) {
							// check for passphrase if not open wlan
							bestNetworkDb = rssi_scan;
							bestRSSI = min(max(2 * (rssi_scan + 100), 0), 100);
							bestChannel = chan_scan;
							bestNetwork = entry;
							memcpy((void*) &bestBSSID, (void*) BSSID_scan, sizeof(bestBSSID));
						}
					}
					break;
				}
			}		
			delay(0);
		}
		// clean up ram
		WiFi.scanDelete();

		delay(0);
		//wl_status_t _status;
		if(bestNetwork.ssid.length() > 0) {
			if (_isUdate) {
				if (_dnip != bestNetwork.dnip) {
					if (bestNetwork.dnip) {
						WiFi.disconnect();
					}
				}
				_isUdate = false;
				goto connect;
			}			
			
			if (_Scan) {
				_Scan = false;
				_status = WiFi.status();
				/*Serial.print("current :");
				Serial.print(WiFi.SSID());
				Serial.println(currRSSI);
				Serial.print("best :");
				Serial.print(bestNetwork.ssid);
				Serial.println(bestRSSI);*/
			
				if (_status == WL_CONNECTED /*|| _status == WL_IDLE_STATUS*/) {
					if (WiFi.SSID().equals(bestNetwork.ssid)) {				
						goto resume;
					}
					else {
						if (currRSSI > (bestRSSI - _value->settings.deltaRSSI)) {				
							goto resume;
						}
					}
				}
			}
			
					
connect:							
			if (!bestNetwork.dnip) {					
				if (_lanIp.fromString(bestNetwork.ip) && _gate.fromString(bestNetwork.gate) && netMsk.fromString(bestNetwork.mask)) {
					WiFi.config(_lanIp, _gate, netMsk);               									// Надо сделать настройки ip адреса
				}
				else {
					bestNetwork.dnip = true;	
				}
			}
			else {
				WiFi.config(0U, 0U, 0U);
			}					
			WiFi.begin(bestNetwork.ssid.c_str(), bestNetwork.passphrase.c_str(), bestChannel, bestBSSID);
			_status = WiFi.status();
			static const uint32_t connectTimeout = 5000;       //5s timeout
                
			auto startTime = millis();
			// wait for connection, fail, or timeout
			while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
				delay(10);
				_status = WiFi.status();
			}
			if (_status == WL_CONNECTED) {
				//WiFi.enableAP(false);
				NBNS.begin(WiFi.hostname().c_str());
				_EnableAP = false;
			}
			else {
				NBNS.end();
				_time_connect = millis() + 5000;
				webSocket.textAll("{\"cmd\":\"error\",\"ssid\":\"" + bestNetwork.ssid + "\",\"status\":\"" + String(_status) + "\"}");
				//Serial.println("{\"cmd\":\"error\",\"ssid\":\""+bestNetwork.ssid+"\",\"status\":\""+String(_status)+"\"}");
			}				
		}else {
			//WiFi.enableAP(true);
			_EnableAP = true;
			WiFi.disconnect();
		}
		if (_Scan)
			_Scan = false;	
		if (_isUdate)
			_isUdate = false;			
	}
resume:		
	resume();
#else
	wl_status_t _status = WiFi.status();
	pause();
	if (String(_value->settings.wSSID).length() == 0) {
		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(false);
		return;
	}
	if (WiFi.SSID().equals(_value->settings.wSSID)) {
		WiFi.begin();		
		_status = WiFi.status();
		static const uint32_t connectTimeout = 5000;        //5s timeout                
		auto startTime = millis();
		// wait for connection, fail, or timeout
		while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
			delay(10);
			_status = WiFi.status();
		}
		return;
	}
	WiFi.disconnect(false);
	/* ! */
	int n = WiFi.scanComplete();
	if (n == -2) {
		WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
	}else if (n > 0) {
		scanResultForConnect(n);
	}
#endif // MULTI_POINTS_CONNECT

}

#ifdef MULTI_POINTS_CONNECT
void /*ICACHE_RAM_ATTR*/ WiFiModuleClass::scan() {	
	pause();
	_Scan = true;
};	
#else
void WiFiModuleClass::scanResultForConnect(int networksFound) {
	for (int i = 0; i < networksFound; ++i) {
		if (WiFi.SSID(i).equals(_value->settings.wSSID)) {
			WiFi.persistent(true);
			WiFi.begin(_value->settings.wSSID, _value->settings.wKey);
			return;
		}
	}
	WiFi.scanDelete();
	resume();
}
#endif // MULTI_POINTS_CONNECT
		
		

/*void / *ICACHE_RAM_ATTR* / WiFiModuleClass::scanResultForConnect(int scanResult) {
#ifdef MULTI_POINTS_CONNECT
	// scan done, analyze
	EntryWiFi bestNetwork;
	//WifiAPEntry bestNetwork { NULL, NULL };
	int bestNetworkDb = INT_MIN;
	int32_t bestChannel;
	bool enableAP = false;
	_enableSSID = false;

	//delay(0);
	for (int8_t i = 0; i < scanResult; ++i) {

		String ssid_scan;
		int32_t rssi_scan;
		uint8_t sec_scan;
		uint8_t* BSSID_scan;
		int32_t chan_scan;
		bool hidden_scan;

		WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
		if (WiFi.SSID().equals(ssid_scan))
			enableAP = true;		/ * если текущий ssid есть в эфире * /
		//bool known = false;
		for(auto entry : _accessPoints) {			
			if (ssid_scan == entry.ssid) {
				_enableSSID = true;				/ * в эфире есть сеть которая есть в списке * /
				if (rssi_scan > bestNetworkDb) {
					 // best network
				    if(sec_scan == ENC_TYPE_NONE || entry.passphrase) {
						 // check for passphrase if not open wlan
				        bestNetworkDb = rssi_scan;
						bestChannel = chan_scan;
						bestNetwork = entry;
					}
				}
				break;
			}
			delay(0);
		}		
		delay(0);
	}

	// clean up ram
	WiFi.scanDelete();

	delay(0);
	
	if (bestNetwork.ssid){
		if (!_isUdate){
			if (!_connecting)	{
				if (WiFi.SSID().equals(bestNetwork.ssid)) {				
					return;
				}else if (WiFi.RSSI() > (bestNetworkDb - 20)) {
					return;
				}
				if (_dnip != bestNetwork.dnip) {
					if (bestNetwork.dnip) {
						WiFi.disconnect();
						return;
					}
				}
			}
		}
		WiFi.disconnect();	
		connectAp(bestNetwork); //сделать чтобы при коненткте _connecting устанавливал		
		_isUdate = false;
	}	
#else
	for (int i = 0; i < scanResult; ++i) {
		if (WiFi.SSID(i).equals(_value->wSSID)) {
			WiFi.persistent(true);
			WiFi.begin(_value->wSSID, _value->wKey);
			return;
		}
	}
	WiFi.scanDelete();
#endif // MULTI_POINT_CONNECT
	//resume();	
}*/

/*void / ** / WiFiModuleClass::connect() {
	wl_status_t _status = WiFi.status();
#ifdef MULTI_POINTS_CONNECT
	int scanResult = WiFi.scanComplete();
	if (scanResult == WIFI_SCAN_RUNNING){
		// scan is running, do nothing yet
		_status = WL_NO_SSID_AVAIL;
		return;
	} 
		
	if (scanResult == 0){
		WiFi.scanDelete();
		delay(0);
		//WiFi.disconnect();
		// scan wifi async mode
		//WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
		WiFi.scanNetworks(true);
		return;
	}
		
	if (scanResult == WIFI_SCAN_FAILED)	{
		//WiFi.disconnect();
		//WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
		WiFi.scanNetworks(true);
	}else if (scanResult > 0){
		//scanResultForConnect(scanResult);
		// scan done, analyze
		EntryWiFi bestNetwork;
		int bestNetworkDb = INT_MIN;
		//uint8 bestBSSID[6];
		int32_t bestChannel;
		//_enableSSID = false;

		delay(0);
	
		for (int8_t i = 0; i < scanResult; ++i)	{

			String ssid_scan;
			int32_t rssi_scan;
			uint8_t sec_scan;
			uint8_t* BSSID_scan;
			int32_t chan_scan;
			bool hidden_scan;

			WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
			//Serial.print("best:");
			//Serial.println(bestNetwork.ssid);
			//bool known = false;
			for(auto &entry : _accessPoints)	{
				if (ssid_scan == entry.ssid) {
					//_enableSSID = true; / * в эфире есть сеть которая есть в списке * /
					if (rssi_scan > bestNetworkDb){
						// best network
						if(sec_scan == ENC_TYPE_NONE || entry.passphrase){
							// check for passphrase if not open wlan
							bestNetworkDb = rssi_scan;
							bestChannel = chan_scan;
							bestNetwork = entry;							 
							//memcpy((void*) &bestBSSID, (void*) BSSID_scan, sizeof(bestBSSID));
						}
					}
					break;
				}
			}			
			delay(0);
		}

		// clean up ram
		WiFi.scanDelete();

		delay(0);
		//wl_status_t _status;
			
		if(bestNetwork.ssid){
			if (_status == WL_DISCONNECTED || _status == WL_NO_SSID_AVAIL || _status == WL_IDLE_STATUS || _status == WL_CONNECT_FAILED) {
				connectAp(bestNetwork);
			}else if (!WiFi.SSID().equals(bestNetwork.ssid)) {
				if (WiFi.RSSI() < (bestNetworkDb - 20))
					//if(WiFi.disconnect()){
						connectAp(bestNetwork);
					//}
			}else if (_isUdate) {
				_isUdate = false;
				if (bestNetwork.ip != _currentEntryWiFi.ip || bestNetwork.gate != _currentEntryWiFi.gate ||  bestNetwork.mask != _currentEntryWiFi.mask || bestNetwork.passphrase != _currentEntryWiFi.passphrase) {
					//if (WiFi.disconnect()) {
						connectAp(bestNetwork);
					//}
				}else if (_dnip != bestNetwork.dnip) {
					if (bestNetwork.dnip)
						Board->restart();
					else
						connectAp(bestNetwork);
				}
			}
		}
	}
	
#else
	pause();
	if (String(_value->wSSID).length() == 0) {
		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(false);
		return _status;
	}
	if (WiFi.SSID().equals(_value->wSSID)) {
		WiFi.begin();		
		_status = WiFi.status();
		static const uint32_t connectTimeout = 5000;      //5s timeout                
		auto startTime = millis();
		// wait for connection, fail, or timeout
		while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
			delay(10);
			_status = WiFi.status();
		}
		return _status;
	}
	WiFi.disconnect(false);
	/ *!  * /
	int n = WiFi.scanComplete();
	if (n == -2) {
		WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
	}else if (n > 0) {
		scanResultForConnect(n);
	}
#endif // MULTI_POINTS_CONNECT	
};*/

/*void / *ICACHE_RAM_ATTR * / WiFiModuleClass::connectAp(const EntryWiFi &entry) {
	wl_status_t _status;	
	if (!entry.dnip) {					
		if (_lanIp.fromString(entry.ip) && _gate.fromString(entry.gate) && netMsk.fromString(entry.mask)) {
			WiFi.config(_lanIp, _gate, netMsk);         									// Надо сделать настройки ip адреса			
		}
	}
	_dnip = entry.dnip;
	WiFi.begin(entry.ssid.c_str(), entry.passphrase.c_str());
	//memcpy(&_currentEntryWiFi, bestNetwork, sizeof(_currentEntryWiFi));
	_status = WiFi.status();

	static const uint32_t connectTimeout = 5000;      //5s timeout
                
	auto startTime = millis();
	// wait for connection, fail, or timeout
	/ *while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
		delay(10);
		_status = WiFi.status();
	} * /
	//_currentEntryWiFi = entry;
};*/

/*wl_status_t / ** /ICACHE_RAM_ATTR WiFiModuleClass::connect() {
	wl_status_t _status = WiFi.status();
#ifdef MULTI_POINTS_CONNECT		
	if (!_enableSSID) {	/ * если нет сети в списке не мешаем работать точке доступа * /
		return _status;		
	}
	if (_isUdate){
		scan();
	}		
	if (_status == WL_DISCONNECTED || _status == WL_NO_SSID_AVAIL || _status == WL_IDLE_STATUS || _status == WL_CONNECT_FAILED / *|| _isUdate* /) {
		int scanResult = WiFi.scanComplete();
		if (scanResult == WIFI_SCAN_RUNNING) {
			// scan is running, do nothing yet
			_status = WL_NO_SSID_AVAIL;
			return _status;
		} 
		
		if (scanResult == 0) {
			WiFi.scanDelete();
			delay(0);
			WiFi.disconnect();
			// scan wifi async mode
			//WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
			WiFi.scanNetworks(true);
			return _status;
		}
		
		if (scanResult == WIFI_SCAN_FAILED) {
			WiFi.disconnect();
			//WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
			WiFi.scanNetworks(true);
		}
		else if (scanResult > 0) {
			//scanResultForConnect(scanResult);
			// scan done, analyze
			EntryWiFi bestNetwork;
			int bestNetworkDb = INT_MIN;
			uint8 bestBSSID[6];
			int32_t bestChannel;

			delay(0);
	
			for (int8_t i = 0; i < scanResult; ++i) {

				String ssid_scan;
				int32_t rssi_scan;
				uint8_t sec_scan;
				uint8_t* BSSID_scan;
				int32_t chan_scan;
				bool hidden_scan;

				WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
				//Serial.print("best:");
				//Serial.println(bestNetwork.ssid);
				//bool known = false;
				for(auto &entry : _accessPoints) {
					if (ssid_scan == entry.ssid) {
						if (rssi_scan > bestNetworkDb) {
							// best network
						   if(sec_scan == ENC_TYPE_NONE || entry.passphrase) {
								// check for passphrase if not open wlan
								bestNetworkDb = rssi_scan;
								bestChannel = chan_scan;
								bestNetwork = entry;
								//_currentEntryWiFi = entry;
								memcpy((void*) &bestBSSID, (void*) BSSID_scan, sizeof(bestBSSID));
							}
						}
						break;
					}
				}		
				delay(0);
			}

			// clean up ram
			WiFi.scanDelete();

			delay(0);
			//wl_status_t _status;
			if(bestNetwork.ssid.length() > 0) {
				_dnip = bestNetwork.dnip;
				
				if (!bestNetwork.dnip)	{					
					if (_lanIp.fromString(bestNetwork.ip) && _gate.fromString(bestNetwork.gate) && netMsk.fromString(bestNetwork.mask))
					{
						WiFi.config(_lanIp, _gate, netMsk);            									// Надо сделать настройки ip адреса
					}else{
						bestNetwork.dnip = true;	
					}
				}
					
				WiFi.begin(bestNetwork.ssid.c_str(), bestNetwork.passphrase.c_str(), bestChannel, bestBSSID);
				//memcpy(&_currentEntryWiFi, bestNetwork, sizeof(_currentEntryWiFi));
				_status = WiFi.status();
				_connecting = true;
				static const uint32_t connectTimeout = 5000;      //5s timeout
                
				auto startTime = millis();
				// wait for connection, fail, or timeout
				while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
					delay(10);
					_status = WiFi.status();
				}
			}/ *else
				_enableSSID = false;* /
		}
	}else {
		//pause();
		//if (WiFi.scanComplete() != WIFI_SCAN_RUNNING)
			//WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);	
	}
	return _status;
#else
	pause();
	if (String(_value->wSSID).length() == 0) {
		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(false);
		return _status;
	}
	if (WiFi.SSID().equals(_value->wSSID)) {
		WiFi.begin();		
		_status = WiFi.status();
		static const uint32_t connectTimeout = 5000;       //5s timeout                
		auto startTime = millis();
		// wait for connection, fail, or timeout
		while(_status != WL_CONNECTED && _status != WL_NO_SSID_AVAIL && _status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
			delay(10);
			_status = WiFi.status();
		}
		return _status;
	}
	WiFi.disconnect(false);
	/ *!  * /
	int n = WiFi.scanComplete();
	if (n == -2) {
		WiFi.scanNetworksAsync(std::bind(&WiFiModuleClass::scanResultForConnect, this, std::placeholders::_1), true);
	}
	else if (n > 0) {
		scanResultForConnect(n);
	}
#endif // MULTI_POINTS_CONNECT	
	return _status;
};*/



/*void WiFiModuleClass::scanResultForConnect(int scanResult) {
#ifdef MULTI_POINTS_CONNECT
	// scan done, analyze
	EntryWiFi bestNetwork;
	//WifiAPEntry bestNetwork { NULL, NULL };
	int bestNetworkDb = INT_MIN;
	int32_t bestChannel;
	bool enableAP = false;
	_enableSSID = false;

	//delay(0);
	for (int8_t i = 0; i < scanResult; ++i) {

		String ssid_scan;
		int32_t rssi_scan;
		uint8_t sec_scan;
		uint8_t* BSSID_scan;
		int32_t chan_scan;
		bool hidden_scan;

		WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
		if (WiFi.SSID().equals(ssid_scan))
			enableAP = true;		/ * если текущий ssid есть в эфире * /
		//bool known = false;
		for(auto entry : _accessPoints) {			
			if (ssid_scan == entry.ssid) {
				_enableSSID = true;				/ * в эфире есть сеть которая есть в списке * /
				if (rssi_scan > bestNetworkDb) {
					 // best network
				    if(sec_scan == ENC_TYPE_NONE || entry.passphrase) {
						 // check for passphrase if not open wlan
				        bestNetworkDb = rssi_scan;
						bestChannel = chan_scan;
						bestNetwork = entry;
					}
				}
				break;
			}
			delay(0);
		}		
		delay(0);
	}

	// clean up ram
	WiFi.scanDelete();

	delay(0);
	
	if (!enableAP) {
		WiFi.disconnect();
	}else if (bestNetwork.ssid.length() > 0) {
		if (_isUdate) {
			_isUdate = false;
			if (_dnip != bestNetwork.dnip) {
				if (bestNetwork.dnip) {
					Board->restart();
				}
			}			
			
			/ *if (bestNetwork.ip != _currentEntryWiFi.ip || bestNetwork.gate != _currentEntryWiFi.gate ||  bestNetwork.mask != _currentEntryWiFi.mask || bestNetwork.passphrase != _currentEntryWiFi.passphrase) {
				WiFi.disconnect();
			}else if (_currentEntryWiFi.dnip != bestNetwork.dnip && bestNetwork.dnip){
				Board->restart();
			}* /
			WiFi.disconnect();
		}else if (!WiFi.SSID().equals(bestNetwork.ssid)){
			if (WiFi.RSSI() < (bestNetworkDb - 20))
				WiFi.disconnect();
		}		
	}
#else
	for (int i = 0; i < scanResult; ++i) {
		if (WiFi.SSID(i).equals(_value->wSSID)) {
			WiFi.persistent(true);
			WiFi.begin(_value->wSSID, _value->wKey);
			return;
		}
	}
	WiFi.scanDelete();
#endif // MULTI_POINT_CONNECT
	resume();
}*/

void ICACHE_FLASH_ATTR WiFiModuleClass::printScanResult(int networksFound) {
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
	AsyncWebSocketMessageBuffer *buffer = webSocket.makeBuffer(len);     //  creates a buffer (len + 1) for you.
	if(buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		webSocket.textAll(buffer);
	}
	WiFi.scanDelete();
}

#ifdef MULTI_POINTS_CONNECT
bool WiFiModuleClass::_downloadValue() {
	Dir dir = SPIFFS.openDir("/P/");
	while (dir.next()) {
		File f = SPIFFS.open(dir.fileName(), "r");
		size_t size = f.size();
		std::unique_ptr<char[]> buf(new char[size]);
		f.readBytes(buf.get(), size);
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.parseObject(buf.get());
		if (json.success()) {
			EntryWiFi ap;
			ap.ssid = json["ssid"].as<String>();
			ap.passphrase = json["pass"].as<String>();
			ap.dnip = json["dnip"].as<bool>();  
			ap.ip = json["ip"].as<String>();
			ap.gate = json["gate"].as<String>();
			ap.mask = json["mask"].as<String>();
			addPoint(ap);
			//addPoint(json["ssid"].as<String>().c_str(), json["pass"].as<String>().c_str());				
		}
	}
	return true;
	/*File list_ap = SPIFFS.open(AP_FILE_LIST, "r");
	if (!list_ap) {
		list_ap.close();
		return false;
	}
	size_t size = list_ap.size();
		
	std::unique_ptr<char[]> buf(new char[size]);
		
	list_ap.readBytes(buf.get(), size);
	list_ap.close();
	DynamicJsonBuffer jsonBuffer(size);
	JsonObject& json = jsonBuffer.parseObject(buf.get());
	if (json.containsKey("points")) {
		uint8_t count = json["points"].size();
		for (int i = 0; i < count; i++) {
			addPoint(json["points"][i]["ssid"].as<String>().c_str(), json["points"][i]["pass"].as<String>().c_str());			
		}		
		return true;
	}
	return true;*/
	
}	

/*bool WiFiModuleClass::_uploadValue() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json["cmd"] = "list_ap";
	JsonArray& points = json.createNestedArray("points");
	for (auto p : _accessPoints) {		
		JsonObject& point = jsonBuffer.createObject();
		point["ssid"] = p.ssid;
		points.add(point);
	}
	
	File list_ap = SPIFFS.open(AP_FILE_LIST, "w");
	if (!list_ap) {
		return false;
	}
	
	json.printTo(list_ap);
	list_ap.flush();
	list_ap.close();
	return true;
}*/

bool WiFiModuleClass::savePoint(EntryWiFi point) {
	Dir dir = SPIFFS.openDir("/P/");
	String filename = "/P/" + String(point.ssid) + ".json";
	//int i = 0;
	while(dir.next()) {
		if (dir.fileName().equals(filename)) {
			if (!SPIFFS.remove(filename)) {
				return false;
			}	
		}
		//i++;
	}
	File f = SPIFFS.open(filename, "w+");
	// Check if we created the file
	if(f) {
		DynamicJsonBuffer jsonBuffer;
		JsonObject &root = jsonBuffer.createObject();
		root["ssid"] = point.ssid;
		root["pass"] = point.passphrase;
		root["dnip"] = point.dnip;
		root["ip"] = point.ip;
		root["gate"] = point.gate;
		root["mask"] = point.mask;
		root.printTo(f);
		//_addClient(new AlarmClient(filename, phone, false, false, false));
		f.close();
		return addPoint(point);
	}
	f.close();
	return false;
}
;

bool /*ICACHE_RAM_ATTR*/ WiFiModuleClass::removePoint(const String &name) {
	String filename = "/P/" + name + ".json";
	if (SPIFFS.remove(filename)) {
		loadPoints();
		return true;
	}
	return false;
}
#endif // MULTI_POINTS_CONNECT
