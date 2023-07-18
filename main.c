// Based upon:
//   http://cortex-m.com/hc-sr04-ultrasonic-module/
// Using TI Tiva C TM4C123GH6PM:
//   https://www.ti.com/product/TM4C123GH6PM

#include "TM4C123.h"        // Device header

#define ECHO_PIN    (1U<<6) // PB6 input
#define TRIGGER_PIN (1U<<4) // PA4 output

#define RED_LED     (1U<<1) // PF1 red LED
#define BLUE_LED    (1U<<2) // PF2 blue LED
#define GREEN_LED   (1U<<3) // PF3 green LED

// Must override (disable) clock setup in startup.c
#define CLOCK_SPEED 16000000UL

#define SPEED_OF_SOUND_FACTOR ((double)((1.0/(double)CLOCK_SPEED)*5882.0))

// Timer 0 used for edge capture on echo
void init_echo_timer(void);
void init_echo_timer(void)
{
	SYSCTL->RCGCTIMER |= (1U<<0); // enable clock to timer 0
	TIMER0->CTL = 0x00U;          // disable timer during configuration
	TIMER0->CFG = 0x04U;          // 16-bit timer
	TIMER0->TAMR = 0x17U;         // edge-time, capture, up counter mode
	TIMER0->CTL |= 0x0CU;         // set TAEVENT bit to capture both edges
	TIMER0->CTL |= 0x01U;         // enable timer
}

// Timer 1 used for usec delay
void init_delay_timer(void);
void init_delay_timer(void)
{
	uint32_t count = (CLOCK_SPEED/1000000UL)-1;
	SYSCTL->RCGCTIMER |= (1U<<1); // enable clock to timer 1
	TIMER1->CTL = 0x00U;          // disable timer during configuration
	TIMER1->CFG = 0x04U;          // 16-bit timer
	TIMER1->TAMR = 0x02U;         // periodic mode
	TIMER1->TAILR = count;        // for usec resolution
}

// Trigger pin on port A
void init_trigger_pin(void);
void init_trigger_pin(void)
{
	SYSCTL->RCGCGPIO |= (1U<<0); // enable clock on port A
	GPIOA->DIR |= TRIGGER_PIN;   // output pin
	GPIOA->DEN |= TRIGGER_PIN;   // enable
}

// Echo pin on port B
void init_echo_pin(void);
void init_echo_pin(void)
{
	SYSCTL->RCGCGPIO |= (1U<<1); // enable clock on port B
	GPIOB->DIR &= ~ECHO_PIN;     // input pin
	GPIOB->DEN |= ECHO_PIN;      // enable
	GPIOB->AFSEL |= ECHO_PIN;    // select alternate function
	GPIOB->PCTL &= ~0x0F000000U; // clear port control (PMC6/PB6)
	GPIOB->PCTL |= 0x07000000U;  // set 16/32-Bit timer 0 capture/compare/PWM 0
}

// Red LED on port F (PF1)
void init_red_led(void);
void init_red_led(void)
{
	SYSCTL->RCGCGPIO |= (1U<<5); // enable clock on port F
	GPIOF->DIR |= RED_LED;       // output pin
	GPIOF->DEN |= RED_LED;       // enable
}

// Blue LED on port F (PF2)
void init_blue_led(void);
void init_blue_led(void)
{
	SYSCTL->RCGCGPIO |= (1U<<5); // enable clock on port F
	GPIOF->DIR |= BLUE_LED;      // output pin
	GPIOF->DEN |= BLUE_LED;      // enable
}

// Green LED on port F (PF3)
void init_green_led(void);
void init_green_led(void)
{
	SYSCTL->RCGCGPIO |= (1U<<5); // enable clock on port F
	GPIOF->DIR |= GREEN_LED;      // output pin
	GPIOF->DEN |= GREEN_LED;      // enable
}

void delay_us(uint32_t us);
void delay_us(uint32_t us)
{
	TIMER1->CTL = 0x01U; // enable timer
	// Timer should be running at usec interval
	for (uint32_t i=0; i < us; i++) {
		while ((TIMER1->RIS & 0x01U) == 0x00U);
		TIMER1->ICR = 0x01U;
	}
	TIMER1->CTL = 0x00U; // disable timer
}

void delay_ms(uint32_t ms);
void delay_ms(uint32_t ms)
{
	uint32_t count = ms*1000; // convert ms to us
	TIMER1->CTL = 0x01U; // enable timer
	// Timer should be running at usec interval
	for (uint32_t i=0; i < count; i++) {
		while ((TIMER1->RIS & 0x01U) == 0x00U);
		TIMER1->ICR = 0x01U;
	}
	TIMER1->CTL = 0x00U; // disable timer
}

uint32_t measure_distance(void);
uint32_t measure_distance(void)
{
	uint32_t t0, t1;
	
	// Disable trigger pin to set known state
	// Wait min 10 usec
	// Enable trigger pin
	// Wait min 10 usec
	// Disable trigger pin
	GPIOA-> DATA &= ~TRIGGER_PIN; // low
	delay_us(12);
	GPIOA->DATA |= TRIGGER_PIN; // high
	delay_us(12);
	GPIOA->DATA &= ~TRIGGER_PIN; // low
	
	// Capture rising edge (after trigger)
	TIMER0->ICR = 0x04U; //clear timer capture flag (set CAECINT bit)
	while ((TIMER0->RIS & 0x04U) == 0x00U); // wait for signal (risging/falling edge)
	t0 = TIMER0->TAR; // time at which event took place

	// Capture falling edge (after rising edge)
	TIMER0->ICR = 0x04U; //clear timer capture flag (set CAECINT bit)
	while ((TIMER0->RIS & 0x04U) == 0x00U); // wait for signal (risging/falling edge)
	t1 = TIMER0->TAR; // time at which event took place

	// Calc distance given delta T and speed of sound factor (inches)
	return (uint32_t)((double)(t1 - t0) * SPEED_OF_SOUND_FACTOR);
}

int main(void)
{
	uint32_t d = 0;
	
	init_red_led();
	init_blue_led();
	init_green_led();
	init_echo_pin();
	init_trigger_pin();
	init_echo_timer();
	init_delay_timer();
	
	// Illuminate LEDs based upon distance to target: GREEN->BLUE->RED
	while (1)
	{
		// Distance is measured in inches
		d = measure_distance();
		if (d > 24)
		{
			GPIOF->DATA &= ~RED_LED;
			GPIOF->DATA &= ~BLUE_LED;
			GPIOF->DATA &= ~GREEN_LED;
		}
		else if (d > 12 && d <= 24 )
		{
			GPIOF->DATA &= ~RED_LED;
			GPIOF->DATA &= ~BLUE_LED;
			GPIOF->DATA |= GREEN_LED;
		}
		else if (d > 6 && d <= 12)
		{
			GPIOF->DATA &= ~RED_LED;
			GPIOF->DATA |= BLUE_LED;
			GPIOF->DATA &= ~GREEN_LED;
		}
		else if (d <= 6)
		{
			GPIOF->DATA |= RED_LED;
			GPIOF->DATA &= ~BLUE_LED;
			GPIOF->DATA &= ~GREEN_LED;
		}
		delay_ms(10); // throttle back some
	}
}
