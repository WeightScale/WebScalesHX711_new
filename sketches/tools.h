#pragma once
#include <Arduino.h>
#include <IPAddress.h>

/*template<typename T>
T Constrain(T amt, T *low, T *high) {
	if (amt < *low) {
		return *low;
	}
	else if (amt > *high) {
		return *high;
	}
	return amt;
	//return ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)));	
}

template<typename T>
	T Map(T x, T *in_min, T *in_max, T out_min, T out_max) {
		long divisor = (in_max - in_min);
		if (divisor == 0) {
			return -1; //AVR returns -1, SAM returns 0
		}
		return (x - *in_min) * (out_max - out_min) / divisor + out_min;
	}*/

/** Is this an IP? */
boolean isIp(String str);
/** IP to String? */
String toStringIp(IPAddress ip);
int StringSplit(String sInput, char cDelim, String sParams[], int iMaxParams);