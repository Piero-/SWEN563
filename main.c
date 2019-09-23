#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*Vars*/
char post_failed_msg[] = "Try test again? (Y/N)\r\n";
uint32_t init_time; //Variable to store the time of a bucket
uint32_t lower_limit = 950; //Default value for the lower limit
uint8_t buckets[101]; //100 samples minus the first
uint16_t measurements[1000];
char buffer[100];


void Timer_init() { 
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; //enable gpio
	GPIOA->MODER &= ~GPIO_MODER_MODER0_0;
	GPIOA->AFR[0] &= ~0xF;	//not F so it only clears the first 4 bits
	GPIOA->AFR[0] |= 0x1;
	
	
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN; //enable timer 2 clock
	TIM2->PSC = 79; //Prescaler to slow down the clock of the timer
	TIM2->ARR = 0xFFFFFFFF; //how high the counter counts
	TIM2->EGR |= TIM_EGR_UG; //new prescaler
	TIM2->CCMR1 |= 0x1;
	TIM2->CCER &= ~0x3;
	TIM2->CCER |= TIM_CCER_CC1E;
	

	TIM2->DIER |= TIM_DIER_CC1IE;
	TIM2->DIER |= TIM_DIER_CC1DE;
	
	TIM2->CR1 |= TIM_CR1_CEN;
}

void post_failed() {
	char select_byte;
	USART_Write(USART2, (uint8_t *)post_failed_msg, strlen(post_failed_msg));	//print message if the post fails
	select_byte = USART_Read(USART2);	//reads the input
	if(select_byte == 'N' || select_byte == 'n'){
		USART_Write(USART2, (uint8_t *)"Exiting...\r\n", 16); //prints the exit message
		while(1);
	}
	else if(select_byte == 'Y' || select_byte == 'y') {
		init_time = (uint32_t)TIM2->CCR1;
	}
	
}

void POST() {
	uint32_t current_time = 0;
	
	
	USART_Write(USART2, (uint8_t *)"\r\n\r\nPower on self test...\r\n\r\n", 33);
	
	TIM2->CR1 = 0x1; //input capture mode
	
	while(1) {
		current_time = (uint32_t)TIM2->CNT;
		init_time = (uint32_t)TIM2->CNT;
		
		while(current_time-init_time<100000){
			if(TIM2->SR & 0x2){ //current_time - init_time <= 100000
				USART_Write(USART2, (uint8_t *)"POST Succeeded\r\n\r\n", 20);
				return;
			} 
			current_time = (uint32_t)TIM2->CNT;
		}
				post_failed();
	}
			
}

void pulses(){
	
	uint32_t past_read = 0;
	uint32_t current_time;
	uint32_t difference;
	uint32_t count = 0;

	while(count <= 1000){
		
		// check if input has been captured
		if(TIM2->SR & 0x02){

			// catch first reading for accurate first period
			if(!past_read){
				past_read = (uint32_t)TIM2->CCR1;
			} else {

				current_time = (uint32_t)TIM2->CCR1;
				difference = current_time - past_read;
				past_read = current_time;
				
				// if the reading is between our bounds
				if(difference >= lower_limit && difference <= (lower_limit + 100)){
					buckets[difference - lower_limit]++;					
				}
			}
			count++;
		}
	}
}

void histogram(){

	int initial_value = lower_limit;
	char buffer[100];
	int n=0;
	
	USART_Write(USART2, (uint8_t *)"\r\nHistogram:\r\n", 17);

	for(int i = 0; i < 100; i++){

		initial_value++;

		if(buckets[i] != 0){
			n = sprintf(buffer, "%d us -> %d occurance(s)\r\n", initial_value, buckets[i]);
			USART_Write(USART2,(uint8_t*) buffer, n);
			buckets[i] = 0;
		}

	}

	USART_Write(USART2, (uint8_t *)"\r\n", 4);
}

void getBound() {
	
	char buffer2[100];
	int lower_limit_allowed_range = 0;
	int a = 0;
	int i = 0;
	char choice2;
	
	
	while(!(lower_limit_allowed_range < 9950 && lower_limit_allowed_range > 50)){
	a = 0;
	i = 0;		
	choice2 = 0;
	char inputBucket[5] = {'\0', '\0', '\0', '\0', '\0'};
	
	USART_Write(USART2, (uint8_t *)"\r\nNew lower bound (50-9950):", 30);
	lower_limit_allowed_range = 0;
	
	while(i < 5 && (choice2 != 13)){ //13 is a return or an enter on the keyboard
		a = sprintf(buffer2, "%c", choice2);
		USART_Write(USART2, (uint8_t*)buffer2, a);
		choice2 = USART_Read(USART2);
		inputBucket[i] = choice2;
		i++;
		
	}
	lower_limit = atoi(inputBucket);
	lower_limit_allowed_range = lower_limit;
	}
	a = sprintf(buffer2, "\r\nNew lower limit: %d\r\n", lower_limit);
	USART_Write(USART2, (uint8_t*)buffer2, a);
}

void bounds() {
	char choice;
	
	sprintf(buffer, "Change your lower limit of %d ms? (Y/N)", lower_limit);
	USART_Write(USART2, (uint8_t*)buffer, sizeof(buffer));
	
	choice = USART_Read(USART2);
	
	if (choice == 'y' || choice == 'Y') {
		getBound();
	}
}



int main(void) {
	System_Clock_Init(); //Initialize system clock
	Timer_init(); //Initialize timer
	LED_Init();	//Initialize LEDs
	UART2_Init(); //Initialize UART2
//	test();
	POST();	//Initialize POST routine
	
	char choice;
	
	while(1){
		bounds();
		pulses();
		histogram();
		
		USART_Write(USART2, (uint8_t *)"Repeat everything? (Y/N)\r\n", 28);
		choice = USART_Read(USART2);
		
		if(choice == 'n' || choice == 'N'){
			USART_Write(USART2, (uint8_t *)"Exiting...\r\n", 16);	//exits the program
			break;
		}
	}
	
}
