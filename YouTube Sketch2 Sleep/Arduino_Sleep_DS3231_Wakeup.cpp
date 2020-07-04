// Do not remove the include below
#include "Arduino_Sleep_DS3231_Wakeup.h"

/*
 Low Power SLEEP modes for Arduino UNO/Nano
 using Atmel328P microcontroller chip.

 For full details see my video #115
 at https://www.youtube.com/ralphbacon
 (Direct link to video: https://TBA)

This sketch will wake up out of Deep Sleep when the RTC alarm goes off

 All details can be found at https://github.com/ralphbacon

 */
#include "Arduino.h"
#include <avr/sleep.h>
#include <wire.h>
#include <DS3231.h>

#define sleepPin 9  // When low, makes 328P go to sleep
#define wakePin 3   // when low, makes 328P wake up, must be an interrupt pin (2 or 3 on ATMEGA328P)
#define ledPin 2    // output pin for the LED (to show it is awake)

// DS3231 alarm time
uint8_t wake_HOUR;
uint8_t wake_MINUTE;
uint8_t wake_SECOND;
#define BUFF_MAX 256

///* A struct is a structure of logical variables used as a complete unit
// struct ts {
//    uint8_t sec;         /* seconds */
//    uint8_t min;         /* minutes */
//    uint8_t hour;        /* hours */
//    uint8_t mday;        /* day of the month */
//    uint8_t mon;         /* month */
//    int16_t year;        /* year */
//    uint8_t wday;        /* day of the week */
//    uint8_t yday;        /* day in the year */
//    uint8_t isdst;       /* daylight saving time */
//    uint8_t year_s;      /* year in short notation*/
//#ifdef CONFIG_UNIXTIME
//    uint32_t unixtime;   /* seconds since 01.01.1970 00:00:00 UTC*/
//#endif
//};
struct ts t;

// Standard setup( ) function
void setup() {
	Serial.begin(9600);

	// Keep pins high until we ground them
	pinMode(sleepPin, INPUT_PULLUP);
	pinMode(wakePin, INPUT_PULLUP);

	// Flashing LED just to show the µController is running
	digitalWrite(ledPin, LOW);
	pinMode(ledPin, OUTPUT);

	// Clear the current alarm (puts DS3231 INT high)
	Wire.begin();
	DS3231_init(DS3231_CONTROL_INTCN);
	DS3231_clear_a1f();

	Serial.println("Setup completed.");
}

// The loop blinks an LED when not in sleep mode
void loop() {
	static uint8_t oldSec = 99;
	char buff[BUFF_MAX];

	// Just blink LED twice to show we're running
	doBlink();

	// Is the "go to sleep" pin now LOW?
	if (digitalRead(sleepPin) == LOW)
	{
		// Set the DS3231 alarm to wake up in X seconds
		setNextAlarm();

		// Disable the ADC (Analog to digital converter, pins A0 [14] to A5 [19])
		static byte prevADCSRA = ADCSRA;
		ADCSRA = 0;

		/* Set the type of sleep mode we want. Can be one of (in order of power saving):

		 SLEEP_MODE_IDLE (Timer 0 will wake up every millisecond to keep millis running)
		 SLEEP_MODE_ADC
		 SLEEP_MODE_PWR_SAVE (TIMER 2 keeps running)
		 SLEEP_MODE_EXT_STANDBY
		 SLEEP_MODE_STANDBY (Oscillator keeps running, makes for faster wake-up)
		 SLEEP_MODE_PWR_DOWN (Deep sleep)
		 */
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		sleep_enable()
		;

		// Turn of Brown Out Detection (low voltage)
		// Thanks to Nick Gammon for how to do this (temporarily) in software rather than
		// permanently using an avrdude command line.
		//
		// Note: Microchip state: BODS and BODSE only available for picoPower devices ATmega48PA/88PA/168PA/328P
		//
		// BODS must be set to one and BODSE must be set to zero within four clock cycles. This sets
		// the MCU Control Register (MCUCR)
		MCUCR = bit (BODS) | bit(BODSE);

		// The BODS bit is automatically cleared after three clock cycles so we better get on with it
		MCUCR = bit(BODS);

		// Ensure we can wake up again by first disabling interupts (temporarily) so
		// the wakeISR does not run before we are asleep and then prevent interrupts,
		// and then defining the ISR (Interrupt Service Routine) to run when poked awake
		noInterrupts();
		attachInterrupt(digitalPinToInterrupt(wakePin), sleepISR, LOW);

		// Send a message just to show we are about to sleep
		Serial.println("Good night!");
		Serial.flush();

		// Allow interrupts now
		interrupts();

		// And enter sleep mode as set above
		sleep_cpu()
		;

		// --------------------------------------------------------
		// µController is now asleep until woken up by an interrupt
		// --------------------------------------------------------

		// Wakes up at this point when wakePin is brought LOW - interrupt routine is run first
		Serial.println("I'm awake!");

		// Clear existing alarm so int pin goes high again
		DS3231_clear_a1f();

		// Re-enable ADC if it was previously running
		ADCSRA = prevADCSRA;
	}
	else
	{
		// Get the time
		DS3231_get(&t);

		// If the seconds has changed, display the (new) time
		if (t.sec != oldSec)
		{
			// display current time
			snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d\n", t.year,
					t.mon, t.mday, t.hour, t.min, t.sec);
			Serial.print(buff);
			oldSec = t.sec;
		}
	}
}

// When wakePin is brought LOW this interrupt is triggered FIRST (even in PWR_DOWN sleep)
void sleepISR() {
	// Prevent sleep mode, so we don't enter it again, except deliberately, by code
	sleep_disable()
	;

	// Detach the interrupt that brought us out of sleep
	detachInterrupt(digitalPinToInterrupt(wakePin));

	// Now we continue running the main Loop() just after we went to sleep
}

// Double blink just to show we are running. Note that we do NOT
// use the delay for the final delay here, this is done by checking
// millis instead (non-blocking)
void doBlink() {
	static unsigned long lastMillis = 0;

	if (millis() > lastMillis + 1000)
	{
		digitalWrite(ledPin, HIGH);
		delay(10);
		digitalWrite(ledPin, LOW);
		delay(200);
		digitalWrite(ledPin, HIGH);
		delay(10);
		digitalWrite(ledPin, LOW);
		lastMillis = millis();
	}
}

// Set the next alarm
void setNextAlarm(void)
		{
	// flags define what calendar component to be checked against the current time in order
	// to trigger the alarm - see datasheet
	// A1M1 (seconds) (0 to enable, 1 to disable)
	// A1M2 (minutes) (0 to enable, 1 to disable)
	// A1M3 (hour)    (0 to enable, 1 to disable)
	// A1M4 (day)     (0 to enable, 1 to disable)
	// DY/DT          (dayofweek == 1/dayofmonth == 0)
	uint8_t flags[5] = { 0, 0, 0, 1, 1 };

	// get current time so we can calc the next alarm
	DS3231_get(&t);

	wake_SECOND = t.sec;
	wake_MINUTE = t.min;
	wake_HOUR = t.hour;

	// Add a some seconds to current time. If overflow increment minutes etc.
	wake_SECOND = wake_SECOND + 10;
	if (wake_SECOND > 59)
	{
		wake_MINUTE++;
		wake_SECOND = wake_SECOND - 60;

		if (wake_MINUTE > 59)
		{
			wake_HOUR++;
			wake_MINUTE -= 60;
		}
	}

	// Set the alarm time (but not yet activated)
	DS3231_set_a1(wake_SECOND, wake_MINUTE, wake_HOUR, 0, flags);

	// Turn the alarm on
	DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A1IE);
}
