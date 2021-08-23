//battery will be detected using the ADC whose pin is defined in the macros
//window compare mode is enables in the below threshold mode which will detect if the battery is low or not 
//if the battery is low then the WCM will be disabled so that the WC_ISR is not triggered again and again
//when the charger is re connected the WCM will again be enabled, and the charger is detected by input sensing
//to define the battery level battery level voltages are defined in the macros
//TO DETECT THE BATTERY VOLTAGE WHEN CHARGER IS CONNECTED WE HAVE USED A TRNASISTOR SWITCHING CIRCUIT ON BETWEEN CHARGER AND BATTERY CONNECTION
//THERE FOR AFETR EVERY 30 SEC WE DISCONNECT THE CHARGER WITH BATTERY AND READ BATTERY VOLTAGE ONCE
// /*
//  * battery_supply_test.c
//  *
//  * Created: 8/5/2021 10:42:12 AM
//  *
//  *  Author : RAJAT,SHIVAM_GUPTA
//  */
// #define  F_CPU 24000000UL




#define F_CPU 24000000UL
#include <avr/io.h>
#include <math.h>
#include <util/delay.h>
#include <stdbool.h>
#include "ADC_AVR128DA64 (1).h"
#include "UART_1_AVR128DA64.h"
#include <avr/interrupt.h>
#include <stdbool.h>
#include <avr/xmega.h>

/************Voltage values required***************/
#define low_battery_warning_voltage 11.100					//below this voltage the low_battery_flag will enable and will give hazard warning to the user 
#define battery_25_voltage			11.325					//voltages at different battery levels 
#define battery_50_voltage			11.505
#define battery_75_voltage			11.775
#define full_battery_voltage		12.000			
//#define charging_voltage			12.000					//voltage of the charger connected
#define max_readable_voltage		13.343					//maximum value of the voltage readable by the ADC according to the divider used
#define battery						channel_0			//address of the ADC channels where battery or supply is connected


/*****************CONNECTED PINS******************/
#define supply_address_port			PORTD
#define supply_address_pin			PIN4_bm			
#define MOSFET_PORT					PORTB
#define MOSFET_PIN					PIN4_bm


float adc_multiplying_factor = ((max_readable_voltage/4096) * 0.9405);	//calculation of the multiplying factor for ADC   //* additional multiplying value must be deleted
uint16_t ADC0_read(char pin);
int ADC_read_count = 0, battery_level = 0, charger_disconnect_timer = 0;	//charger_disconnect_timmer is int type as it will be moved b/w 0-4
float temp_battery_voltage = 0;
float supply_voltage=0;
unsigned long battery_read_timmer = 0;
bool charger_connect_flag = false, battery_low_flag = false, read_battery = false, temp_charger_disconnect = false;
float battery_voltage = 0;
bool full_charge_flag = false;
void battery_init (void);
void MILLIS_TCB1_init(void);
float read_voltage(int); 
bool charger_detection(void);
void battery_read(void);


void battery_init (void)
{
	MOSFET_PORT.DIRSET |= MOSFET_PIN;						//pin for turning the transistor on and off and hence charger to battery connection
	supply_address_port.OUTCLR |= supply_address_pin;		//pin for detecting charger
}


/***************WINDOW COMPARE INITIALIZATION*****************************/
void ADC0_window_compare_init()
{
	ADC0.CTRLB = 0x4;	//for 16 consecutive sampling result
	//ADC0.WINHT = 0xFFFF; //4027(dec), for 11.8volts
	ADC0.WINLT = 45500; //3789(dec), for 11.1volts
	ADC0.CTRLE = 0x1; //setting the window comparator is below the window mode
	ADC0.INTCTRL |= PIN1_bm; //enabling window compare interrupt
}


int main (void)
{  	_PROTECTED_WRITE (CLKCTRL.OSCHFCTRLA, ((CLKCTRL_FREQSEL_24M_gc)|(CLKCTRL_AUTOTUNE_bm)));  //Oscillator frequency is 24MHz // Auto tuning enabled
	sei();
	USART1_init(9600);
	battery_init();
	ADC0_init();  //Initialize ADC
	ADC0_window_compare_init();
	

	while(1)
	{
		battery_read();
		_delay_ms(50);
	}
}

/***************FUNCTION TO READ ADC OF AVR*****************************/
float read_voltage(int source)
{
	unsigned int ADC_value = (ADC0_read(source)/16);				//
	ADC_read_count++;																
// USART1_sendString("ADC value");
// USART1_sendInt(ADC_value*16);	
// 	USART1_sendFloat((ADC_value * adc_multiplying_factor), 2);								//
	return (ADC_value * adc_multiplying_factor);		//as we taking sample accumulation is of 16 in CTRL_B register therefore we divide the value by 16
	//return ((ADC0_read(source)/16) * adc_multiplying_factor);		//as we taking sample accumulation is of 16 in CTRL_B register therefore we divide the value by 16
}


/***************FUNCTIONS FOR RETURNING THE LEVEL OF BATTERY*****************************/
void battery_read(void)
{
	charger_connect_flag = charger_detection();							//CHEKING IF THE CHARGER IS CONNECTED OR NOT
	if (!charger_connect_flag)											//if charger is not connected then theADC will be continuously read
	{  		//if (battery_low_flag) is true prior the charger is connected then the WCM will again be turned ON
		
			ADC0.CTRLE = 0x1;						//setting the window comparator is below the window mode
			ADC0.INTCTRL |= PIN1_bm;				//enabling window compare interrupt
		
		temp_battery_voltage += read_voltage(battery);
		 battery_voltage = (temp_battery_voltage/ADC_read_count);
		 //  USART1_sendString("ADC read count");
		 //	USART1_sendInt(ADC_read_count);
		 USART1_sendString("battery voltage");
		 USART1_sendFloat(battery_voltage,2);					
	}
	
	//if charger is not connected, (temp_charger_disconnect) will be true therefore after every 10 sec battery level will be updated
																	//if charger is connected, then (temp_charger_disconnect) will be true once after every 30 sec, therefore when the charger is connected battery level will be updated once in every 30 sec
		
				
		if (battery_voltage <= low_battery_warning_voltage)
		{
			battery_level = 0;																							//battery empty symbol with ! mark in the middle 
		}
		else if ((low_battery_warning_voltage < battery_voltage) && (battery_voltage <= battery_25_voltage))
		{
			battery_level = 1;																							//battery will show 1 point inside it
		}
		else if ((battery_25_voltage < battery_voltage) && (battery_voltage <= battery_50_voltage))
		{
			battery_level = 2;																							//battery will show 2 points inside it
		}
		else if ((battery_50_voltage < battery_voltage) && (battery_voltage <= battery_75_voltage))
		{
			battery_level = 3;																							//battery will show 3 points inside it
		}
		else if ((battery_75_voltage < battery_voltage) && (battery_voltage <= full_battery_voltage))
		{
			battery_level = 4;																							//battery will show 4 points inside it i.e battery full
		}
				
		read_battery = 0;
		ADC_read_count = 0;				//resetting the ADC counter and result register
		temp_battery_voltage = 0;
				
		USART1_sendString("battery level indication");
		USART1_sendInt(battery_level);
	
}

/***************FUNCTION FOR CHARGER DETECTION*****************************/
bool charger_detection(void)				
{
	if (supply_address_port.IN & supply_address_pin)
	{   _delay_ms(100);
 		battery_low_flag = false;				//in case of low battery if the charger is connected then low battery flag will be off
 		USART1_sendString("CHARGER CONNECTED");
		USART1_sendInt(ADC0_read(channel_0));
		if((ADC0_read(channel_0))<500)
		{
			USART1_sendString("Charging complete");
			full_charge_flag = true;
		}
		else if ((ADC0_read(channel_0))>1000)
		{full_charge_flag = false;
		}
		
		return true;							//indicate that the charger is connected
	}
	else
	{
		temp_charger_disconnect = true;		
		return false;		//indicate that the charger is not! connected
	}
}

/***************WINDOW COMPARE INTERRUPRT*****************************/
ISR(ADC0_WCMP_vect)
{
	
	if (!battery_low_flag)
	{   
		ADC0.CTRLE = 0;						//setting the window comparator off when the low voltage is detected so that WCM should not do processing again and again
		ADC0.INTCTRL &= ~PIN1_bm;			//disabling window compare interrupt
		USART1_sendString("battery low");
		battery_low_flag = true;
	}
}





