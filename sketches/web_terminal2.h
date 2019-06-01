#pragma once


//using namespace std;

#define GPIO14			14							/* сигнал clock HX711 */
#define GPIO16			16							/* сигнал дата HX711 */
#define EN_NCP			12							/* сигнал включения питания  */
#define PWR_SW			13							/* сигнал от кнопки питания */
#define EN_MT			15							/* сигнал включения питание датчика*/
#define LED				2							/* индикатор работы */
#define SCL_RTC			5							/* сигнал clock часов */
#define SDA_RTC			4							/* сигнал дата часов */
#define V_BAT			A0							/* ацп для батареи */

#define R1_KOM					300.0f
#define R2_KOM					100.0f
#define VREF					1.0f
#define ADC						1023.0f



#define V_BAT_MAX				4.3f
#define V_BAT_MIN				3.3f

