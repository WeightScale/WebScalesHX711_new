#include "Memory.h"
#include <FS.h>
//#include "Battery.h"
//#include "Config.h"

/*void MemoryClass::init(){
	SPIFFS.begin();	
	begin(sizeof(MyEEPROMStruct));
	if (percentUsed() >= 0) {
		get(0, eeprom);
	}else{
		doDefault();
	}
}

bool MemoryClass::save(){
	put(0, eeprom);
	return commit();
}

bool MemoryClass::doDefault(){
	String u = F("admin");
	String p = F("1234");
	String apSsid = F("SCALES");
	u.toCharArray(eeprom.scales_value.user, u.length()+1);
	p.toCharArray(eeprom.scales_value.password, p.length()+1);
	u.toCharArray(eeprom.settings.scaleName, u.length()+1);
	p.toCharArray(eeprom.settings.scalePass, p.length()+1);
	apSsid.toCharArray(eeprom.settings.apSSID, apSsid.length() + 1);
	eeprom.settings.bat_max = MAX_CHG;
	eeprom.settings.bat_min = MIN_CHG;
	eeprom.settings.time_off = 600000;
	eeprom.settings.hostPin = 0;
	eeprom.settings.autoIp = true;	
	
	eeprom.scales_value.accuracy = 1;
	eeprom.scales_value.average = 1;
	eeprom.scales_value.filter = 100;
	eeprom.scales_value.max = 1000;
	eeprom.scales_value.step = 1;
	eeprom.scales_value.zero_man_range = 0.02;
	eeprom.scales_value.zero_on_range = 0.02;
	return save();
}

MemoryClass Memory;*/