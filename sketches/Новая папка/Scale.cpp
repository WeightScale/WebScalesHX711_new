#include "Scale.h"
#include "Battery.h"
#include "Core.h"
#include "Config.h"

ScaleClass Scale(DOUT_PIN,SCK_PIN);	
Task* taskWeight = new Task(takeWeight, 200);

ScaleClass::ScaleClass(byte dout, byte pd_sck) : HX711(dout, pd_sck) /*, ExponentialFilter<long>()*/{
	_server = NULL;	
	_authenticated = false;	
	_saveWeight.isSave = false;
	_saveWeight.value = 0.0;
}

ScaleClass::~ScaleClass(){}
	
void ScaleClass::setup(BrowserServerClass *server){
	init();
	_server = server;
	_server->on("/wt",HTTP_GET, [this](AsyncWebServerRequest * request){						/* Получить вес и заряд. */
		AsyncResponseStream *response = request->beginResponseStream("text/json");
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();
		Scale.doData(json);
		Board->battery()->doData(json);
		//BATTERY->doData(json);
		json.printTo(*response);
		#if POWER_PLAN
			POWER.updateCache();
		#endif
		request->send(response);
	});
	_server->on("/wtr",
		HTTP_GET,
		[this](AsyncWebServerRequest * request) {
			/* Получить вес и заряд. */
			AsyncResponseStream *response = request->beginResponseStream("text/json");
			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.createObject();
			Scale.doDataRandom(json);
			json.printTo(*response);
			request->send(response);
		});
	_server->on(PAGE_FILE, [this](AsyncWebServerRequest * request) {							/* Открыть страницу калибровки.*/
		if(!request->authenticate(_scales_value->user, _scales_value->password))
			if (!_server->checkAdminAuth(request)){
				return request->requestAuthentication();
			}
		_authenticated = true;
		saveValueCalibratedHttp(request);
	});
	_server->on("/av", [this](AsyncWebServerRequest * request){									/* Получить значение АЦП усредненное. */
		request->send(200, F("text/html"), String(readAverage()));
	});
	_server->on("/tp", [this](AsyncWebServerRequest * request){									/* Установить тару. */
		#if !defined(DEBUG_WEIGHT_RANDOM)  && !defined(DEBUG_WEIGHT_MILLIS)
			zero(CoreMemory.eeprom.scales_value.zero_man_range);
		#endif 					/* Установить тару. */
		request->send(204, F("text/html"), "");
	});
	_server->on("/sl", handleSeal);																/* Опломбировать */
	_server->on("/cdate.json",HTTP_ANY, [this](AsyncWebServerRequest * request){									/* Получить настройки. */
		if(!request->authenticate(_scales_value->user, _scales_value->password))
			if (!browserServer.checkAdminAuth(request)){
				return request->requestAuthentication();
			}
		AsyncResponseStream *response = request->beginResponseStream(F("application/json"));
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();
		
		json[POWER_5V] = _scales_value->power5v;
		json[STEP_JSON] = _scales_value->step;
		json[AVERAGE_JSON] = _scales_value->average;
		json[WEIGHT_MAX_JSON] = _scales_value->max;
		json[ZERO_MAN_RANGE_JSON] = _scales_value->zero_man_range;
		json[ZERO_ON_RANGE_JSON] = _scales_value->zero_on_range;
		json[OFFSET_JSON] = _scales_value->offset;
		json[ACCURACY_JSON] = _scales_value->accuracy;
		json[SCALE_JSON] = _scales_value->scale;
		json[FILTER_JSON] = GetFilterWeight();
		json[SEAL_JSON] = _scales_value->seal;
		json[USER_JSON] = _scales_value->user;
		json[PASS_JSON] = _scales_value->password;
		
		json.printTo(*response);
		request->send(response);
	});
}

void ScaleClass::init(){
	pinMode(EN_MT, OUTPUT);
	
	reset();
	_scales_value = &CoreMemory.eeprom.scales_value;
	digitalWrite(EN_MT, _scales_value->power5v);
	if (_scales_value->scale != _scales_value->scale){
		_scales_value->scale = 0.0f;	
	}
	_offset_local = _scales_value->offset;
	mathRound();
	SetFilterWeight(_scales_value->filter);
#if !defined(DEBUG_WEIGHT_RANDOM)  && !defined(DEBUG_WEIGHT_MILLIS)
	readAverage();
	
	//_weight_zero_range = _scales_value->max * _scales_value->zero_man_range; /* Диапазон нуля */
	_weight_zero_auto = round(_scales_value->zero_auto * _round) / _round; /* Вес автообнуления */
	
	zero(_scales_value->zero_on_range);			  
#endif // !DEBUG_WEIGHT_RANDOM && DEBUG_WEIGHT_MILLIS
	
}

void ScaleClass::mathRound(){
	_round = pow(10.0, _scales_value->accuracy) / _scales_value->step; // множитель для округления}
	_stable_step = 1/_round;
}

void ScaleClass::saveValueCalibratedHttp(AsyncWebServerRequest * request) {
	if (request->args() > 0) {
		if (request->hasArg("update")){
			_scales_value->accuracy = request->arg(F("weightAccuracy")).toInt();
			_scales_value->step = request->arg(F("weightStep")).toInt();
			average(request->arg(F("weightAverage")).toInt());
			SetFilterWeight(request->arg(F("weightFilter")).toInt());
			_scales_value->filter = GetFilterWeight();
			_scales_value->max = request->arg("weightMax").toInt();
			_scales_value->zero_man_range = request->arg(F("zm_range")).toFloat();
			_scales_value->zero_on_range = request->arg(F("zo_range")).toFloat();
			//_weight_zero_range = _scales_value->max * _scales_value->zero_range; /* Диапазон нуля */
			_scales_value->zero_auto = request->arg(F("z_auto")).toInt();
			_weight_zero_auto = round(_scales_value->zero_auto * _round) / _round; /* Вес автообнуления */
			if (request->hasArg("p5v"))
				_scales_value->power5v = true;
			else
				_scales_value->power5v = false;
			digitalWrite(EN_MT, _scales_value->power5v);
			mathRound();
			if (CoreMemory.save()){
				goto ok;
			}
			goto err;
		}
		
		if (request->hasArg("zero")){
			SetCurrent(read());
			_scales_value->offset = _offset_local = Current();
			//offset(Current());
		}
		
		if (request->hasArg(F("weightCal"))){
			float rw = request->arg(F("weightCal")).toFloat();
			SetCurrent(read());
			long cw = Current();
			//long cw = readAverage();
			mathScale(rw,cw);
		}
		
		if (request->hasArg("user")){
			request->arg("user").toCharArray(_scales_value->user,request->arg("user").length()+1);
			request->arg("pass").toCharArray(_scales_value->password,request->arg("pass").length()+1);
			if (CoreMemory.save()){
				goto url;
			}
			goto err;
		}
		
		ok:
			return request->send(200, F("text/html"), "");
		err:
			return request->send(204);	
	}
	url:
	#ifdef HTML_PROGMEM
		request->send_P(200, F("text/html"), calibr_html);
	#else
		request->send(SPIFFS, request->url());
	#endif
}

void ScaleClass::fetchWeight(){
	_weight = getWeight();
	detectAutoZero();
	detectStable(_weight);
}

long ScaleClass::readAverage() {
	long long sum = 0;
#ifdef DEBUG_WEIGHT_RANDOM	
	for (byte i = 0; i < _scales_value->average; i++) {
		sum += analogRead(A0);
	}
	//randomSeed(1000);
	sum = random(8000000);
	Filter(_scales_value->average == 0 ? sum / 1 : sum / _scales_value->average);
#else
	for (byte i = 0; i < _scales_value->average; i++) {
		sum += read();
	}
	Filter(_scales_value->average == 0?sum / 1:sum / _scales_value->average);
#endif // DEBUG_WEIGHT_RANDOM
	
	return Current();
}

long ScaleClass::getValue() {
	int i = readAverage();
	return i - _offset_local;
}

float ScaleClass::getUnits() {
#ifdef DEBUG_WEIGHT_MILLIS
	static long long time = 0;
	float v = (micros() - time);
	time  = micros();
	return v;
#else
	return getValue() * _scales_value->scale;
#endif // DEBUG_WEIGHT_MILLIS
}

float ScaleClass::getWeight(){
	return round(getUnits() * _round) / _round; 
}

void ScaleClass::zero(float range) {
	_weight = getWeight();
	if (fabs(_weight) > (_scales_value->max * range)) //если текущий вес больше диапазона нуля тогда не сбрасываем
		return;
	SetCurrent(read());
	offset(Current());
}

void ScaleClass::average(unsigned char a){
	_scales_value->average = constrain(a, 1, 5);
}

void ScaleClass::mathScale(float referenceW, long calibrateW){
	_scales_value->scale = referenceW / float(calibrateW - _scales_value->offset);
}

/*! Функция для форматирования значения веса
	value - Форматируемое значение
	digits - Количество знаков после запятой
	accuracy - Точновть шага значений (1, 2, 5, ...)
	string - Выходная строка отфармотронова значение 
*/
void ScaleClass::formatValue(float value, char* string){
	dtostrf(value, 6-_scales_value->accuracy, _scales_value->accuracy, string);
}

/* */
void ScaleClass::detectStable(float w){	
	if (_saveWeight.value != w){
		_saveWeight.stabNum = STABLE_NUM_MAX;
		_stableWeight = false;
		_saveWeight.value = w;
		return;
	}
	
	if (_saveWeight.stabNum) {
		_saveWeight.stabNum--;
		return;
	}	
	if (_stableWeight) {
		return;
	}
	_stableWeight = true;
	if (millis() < _saveWeight.time)
		return;
	if (fabs(_saveWeight.value) > _stable_step) {
		_saveWeight.isSave = true;
		_saveWeight.time = millis() + 10000;
	}
}

void ScaleClass::detectAutoZero(){
	static long time;
	static bool flag;
	if (_weight_zero_auto < _weight)
		return;
	if (!flag){
		flag = true;
		time = millis() + 60000;
	}
	if (time > millis())
		return;
	zero(_scales_value->zero_man_range);
	flag = false;	
}

size_t ScaleClass::doData(JsonObject& json ){
	//json["w"]= String(_buffer);
	if(_weight > _scales_value->max){
		json["cmd"] = "ovl";	//перегруз
	}else{
		json["w"] = _weight;
		json["a"] = _scales_value->accuracy;
		json["s"] = _saveWeight.stabNum;	
	}		
	return json.measureLength();
}

size_t ScaleClass::doDataRandom(JsonObject& json) {
	long sum = random(500,5010);	
	json["w"] = sum;
	json["t"] = millis();
	json["m"] = ESP.getFreeHeap();
	json["c"] = ws.count();	
	return json.measureLength();
}

void handleSeal(AsyncWebServerRequest * request){
	randomSeed(Scale.readAverage());
	Scale.seal(random(1000));
	
	if (CoreMemory.save()){
		return request->send(200, F("text/html"), String(Scale.seal()));
	}
	return request->send(400, F("text/html"), F("Ошибка!"));
}





