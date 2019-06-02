#pragma once
#include <FS.h>
#include "TaskController.h"
#include "Config.h"
#include <functional>
#include <ESP_EEPROM.h>
#include "Battery.h"
#include "BrowserServer.h"
#include "WiFiModule.h"
#include "Scales.h"

#define SCALE_JSON		"scale"
#define SERVER_JSON		"server"

template <typename T>class MemoryClass : protected EEPROMClass {
public:
	T * _value;

public:
	MemoryClass(T *mem) {
		_value = mem;
	};
	~MemoryClass() {
		close();
	};
	bool init() {
		SPIFFS.begin();	
		begin(sizeof(T));
		if (percentUsed() < 0)
			return false;
		get(0, *_value);
		return true;
	};
	bool save() {
		put(0, *_value);
		return commit();
	};
	void close() {
		end();
		SPIFFS.end();
	};
	bool doDefault();	
};

class PowerClass : public Task {
private:
	typedef std::function<void(void)> PowerFunction;
	byte _switch;
	byte _signal;
	PowerFunction onCallback;
	//void(*onCallback)(void);
	//bool _time_enable = false;
	//unsigned int _time;
	
public:
	PowerClass(byte swch, byte button_signal,PowerFunction function) : Task(2400000), _switch(swch), _signal(button_signal) {
		pinMode(_switch, OUTPUT);
		pinMode(_signal, INPUT);
		onCallback = function;
		on();
		onRun(std::bind(&PowerClass::off, this));
		//POWER.enabled = _settings->power_time_enable;	
		//POWER.setInterval(_settings->time_off);
	};
	~PowerClass() {};
	void begin(unsigned int *time, bool *off) {
		enabled = *off;
		setInterval(*time);
	};
	void on() {digitalWrite(_switch, HIGH); };
	void off() {
		onCallback();
		digitalWrite(_switch, LOW); 
	};
	
	void powerSwitchInterrupt() {
		if (digitalRead(_signal) == HIGH) {
			unsigned long t = millis();
			digitalWrite(LED, HIGH);
			while (digitalRead(_signal) == HIGH) {
				// 
			   delay(100);	
				if (t + 4000 < millis()) {
					// 
				   digitalWrite(LED, HIGH);
					while (digitalRead(_signal) == HIGH){delay(10); };// 
					off();			
					break;
				}
				digitalWrite(LED, !digitalRead(LED));
			}
		}
	}
};

class BlinkClass : public Task {
public:
	unsigned int _flash = 500;
public:
	BlinkClass() : Task(500) {
		pinMode(LED, OUTPUT);
		ledOn();
		onRun(std::bind(&BlinkClass::blinkAP, this));
	};
	void blinkSTA() {
		static unsigned char clk;
		bool led = !digitalRead(LED);
		digitalWrite(LED, led);
		if (clk < 6) {
			led ? _flash = 70 : _flash = 40;
			clk++;
		}else {
			_flash = 2000;
			digitalWrite(LED, HIGH);
			clk = 0;
		}
		setInterval(_flash);
	}
	void blinkAP() {
		ledTogle();
		setInterval(500);
	}
	void ledOn() {digitalWrite(LED, LOW); };
	void ledOff() {digitalWrite(LED, HIGH); };
	void ledTogle() {digitalWrite(LED, !digitalRead(LED)); };
};

class BoardClass : public TaskController {
private:
	struct MyEEPROMStruct _eeprom;
	PowerClass *_power;
	BlinkClass *_blink;
	BatteryClass *_battery;
	WiFiModuleClass * _wifi;
	WiFiEventHandler stationConnected;
	WiFiEventHandler stationDisconnected;
	WiFiEventHandler STAGotIP;
	MemoryClass<MyEEPROMStruct> *_memory;
	BrowserServerClass *_server;
	ScalesClass *_scales;
public:
	BoardClass();
	~BoardClass() {
		delete _power;
		delete _blink;
		delete _memory;
		delete _battery;
		delete _scales;
	};
	void init();
	void handle() {
		run();
#if !defined(POWER_DEBUG) && defined(INTERNAL_POWER)
		_power->powerSwitchInterrupt(); 		  
#endif // !POWER_DEBUG		
	};
	void onSTA() {_blink->onRun(std::bind(&BlinkClass::blinkSTA, _blink)); };
	void offSTA() {_blink->onRun(std::bind(&BlinkClass::blinkAP, _blink)); };
	void onStationConnected(const WiFiEventStationModeConnected& evt);
	void onStationDisconnected(const WiFiEventStationModeDisconnected& evt);
	void onSTAGotIP(const WiFiEventStationModeGotIP& evt);
	BatteryClass * battery() {return _battery;};
	MemoryClass<MyEEPROMStruct> *memory(){return _memory ;};
	PowerClass * power() {return _power;};
	WiFiModuleClass * wifi() {return _wifi;};
	ScalesClass * scales() {return _scales;};
	//void handleBinfo(AsyncWebServerRequest *request);
	bool saveEvent(const String& event, float value);
	bool doDefault();
	//size_t doSettings(JsonObject &root);
	void powerOff() {
		//ws.closeAll();
		//delay(2000);
		//browserServer.stop();
		//SPIFFS.end();
		//Scale.power_down();  /// Выключаем ацп
		//_power->off();
		//digitalWrite(EN_NCP, LOW);  /// Выключаем стабилизатор
		
		//ESP.reset();
		_memory->close();
	}
};

extern BoardClass * Board;