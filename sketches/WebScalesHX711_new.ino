//Comment out the definition below if you don't want to use the ESP8266 gdb stub.
#define ESP8266_USE_GDB_STUB

#ifdef ESP8266_USE_GDB_STUB
#include <GDBStub.h>
	extern "C" int gdbstub_init();
	extern "C" int gdbstub_do_break();
#endif

#include "Board.h"

//using namespace std;
//using namespace std::placeholders;


void shutDown();


void setup() {
	
#ifdef ESP8266_USE_GDB_STUB
	Serial.begin(921600);
	gdbstub_init();
	gdbstub_do_break();
#endif
	Board = new BoardClass();
	Board->init();
	server.begin();
}

void loop() {
	Board->handle();
#ifdef MULTI_POINTS_CONNECT
	Board->wifi()->connect();
	delay(1);
#endif // MULTI_POINTS_CONNECT
}

void shutDown() {	
	ESP.deepSleep(0, WAKE_RF_DISABLED);
}
