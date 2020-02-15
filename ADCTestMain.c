/*
ADCTestMain.c
Andrea Nguyen, Kenny Tang
Lab 2 Tester File
Lab 2
Bhagawat Vinay
Feb 7, 2020
*/

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// center of X-ohm potentiometer connected to PE3/AIN0
// bottom of X-ohm potentiometer connected to ground
// top of X-ohm potentiometer connected to +3.3V 
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "../inc/ADCSWTrigger.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/CortexM.h"
#include "../inc/UART.h"

struct pmf_entry {
	uint32_t ADCvalue;
	uint32_t instances;
};

volatile uint32_t ADCvalue;
volatile uint32_t ADCvalue_ready;
volatile uint32_t ADCvalues[1000];
volatile uint32_t ADCtimes[1000];
volatile struct pmf_entry pmf[1000];
volatile uint32_t jitter;
// This debug function initializes Timer0A to request interrupts
// at a 100 Hz frequency.  It is similar to FreqMeasure.c.
void Timer0A_Init100HzInt(void){
  volatile uint32_t delay;
  DisableInterrupts();
  // **** general initialization ****
  SYSCTL_RCGCTIMER_R |= 0x01;      // activate timer0
  delay = SYSCTL_RCGCTIMER_R;      // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER0_CFG_R = 0;                // configure for 32-bit timer mode
  // **** timer0A initialization ****
                                   // configure for periodic mode
  TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER0_TAILR_R = 799999;         // start value for 100 Hz interrupts
  TIMER0_IMR_R |= TIMER_IMR_TATOIM;// enable timeout (rollover) interrupt
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;// clear timer0A timeout flag
  TIMER0_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 32-b, periodic, interrupts
  // **** interrupt initialization ****
                                   // Timer0A=priority 2
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000; // top 3 bits
  NVIC_EN0_R = 1<<19;              // enable interrupt 19 in NVIC
}
void Timer0A_Handler(void){
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;    // acknowledge timer0A timeout
  PF2 ^= 0x04;                   // profile
  PF2 ^= 0x04;                   // profile
  ADCvalue = ADC0_InSeq3();
  PF2 ^= 0x04;                   // profile
	ADCvalue_ready = 1;
}

void Timer1_Init(void) { 
	volatile uint32_t delay;
	SYSCTL_RCGCTIMER_R |= 0x02;
	delay = SYSCTL_RCGCTIMER_R;
	TIMER1_CTL_R = 0x00000000;
	TIMER1_CFG_R = 0x00000000;
	TIMER1_TAMR_R = 0x00000002;
	TIMER1_TAILR_R = 0xFFFFFFFF;
	TIMER1_TAPR_R = 0;
	TIMER1_CTL_R = 0x00000001;
}

void add_pmf_entry(uint32_t ADCvalue) {
	for (uint32_t i = 0; i < 1000; i++) {
		if (pmf[i].ADCvalue == 0 && pmf[i].instances == 0) {
			pmf[i].ADCvalue = ADCvalue;
			pmf[i].instances++;
			return;
		}
		if (pmf[i].ADCvalue == ADCvalue) {
			pmf[i].instances++;
			return;
		}
	}
}

int main(void){
  PLL_Init(Bus80MHz);                   // 80 MHz
  LaunchPad_Init();                     // activate port F
  ADC0_InitSWTriggerSeq3_Ch9();         // allow time to finish activating
  Timer0A_Init100HzInt();               // set up Timer0A for 100 Hz interrupts
	Timer1_Init();
	UART_Init();
  PF2 = 0;                      // turn off LED
  EnableInterrupts();
	
	// Arrays for debugging dumps
	volatile uint32_t i = 0;
  while(1){
		if (ADCvalue_ready && i < 1000) {
			if (i % 100 == 0) {
				printf("i: %d\n", i);
			}
			ADCvalue_ready = 0;
			ADCvalues[i] = ADCvalue;
			ADCtimes[i] = TIMER1_TAR_R;
			i++;
		} else if (i == 1000) {
			// Calculate jitter
			uint32_t last_time = ADCtimes[0];
			uint32_t min_delta = UINT_MAX;
			uint32_t max_delta = 0;
			for (uint32_t j = 1; j < 1000; j++) {
				// PMF function
				add_pmf_entry(ADCvalues[j]);
				
				// Calculate max and min differences
				if (last_time - ADCtimes[j] > max_delta) {
					max_delta = last_time - ADCtimes[j];
				} else if (last_time - ADCtimes[j] < min_delta) {
					min_delta = last_time - ADCtimes[j];
				}
				last_time = ADCtimes[j];
			}
			jitter = max_delta - min_delta;
			
			printf("Jitter: %d\n", jitter);
			// Send pmf through uart
			for (uint32_t j = 0; j < 1000; j++) {
				if (pmf[j].ADCvalue == 0 && pmf[j].instances == 0) {
					break;
				}
				printf("%04d\t\t%d\n", pmf[j].ADCvalue, pmf[j].instances);
			}
			i++;
		}
    PF1 ^= 0x02;  // toggles when running in main
  }
}
