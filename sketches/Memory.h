#pragma once
#include <Arduino.h>
#include <ESP_EEPROM.h>
#include "Config.h"

template <size_t ArrayLength, typename SomeValueType>
class MemoryClass : protected EEPROMClass {
public:
	

public:
	void init();
	bool save();
	bool doDefault();	
};

//extern MemoryClass CoreMemory;