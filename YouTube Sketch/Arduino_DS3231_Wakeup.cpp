// Do not remove the include below
#include "Arduino_DS3231_Wakeup.h"
#include <wire.h>
#include <DS3231.h>

// DS3231 alarm time
uint8_t wake_HOUR;
uint8_t wake_MINUTE;
uint8_t wake_SECOND;
#define BUFF_MAX 256

// A struct is a structure of logical variables used as a complete unit
struct ts t;

void setup()
{
	Serial.begin(9600);
	Serial.println("Setup started");

	// Latch the power
	pinMode(13, OUTPUT);
	digitalWrite(13, HIGH);

	// Clear the current alarm (puts DS3231 INT high)
	Wire.begin();
	DS3231_init(DS3231_CONTROL_INTCN);
	DS3231_clear_a1f();

	// Set date/time if not already set
//	DS3231_get(&t);
//
//	if (t.year < 2021)
//	{
//		t.sec = 0;
//		t.min = 19;
//		t.hour = 18;
//		t.mday = 13;
//		t.mon = 6;
//		t.year = 2020;
//		DS3231_set(t);
//	}

	// All done
	Serial.println("Setup completed");
}

void loop()
{
	static unsigned long oldMillis = millis();
	static uint8_t oldSec = 99;
	char buff[BUFF_MAX];

	// Switch off after 5 seconds
	if (millis() > oldMillis + 10000)
	{
		Serial.println("Setting alarm, switching off");
		set_next_alarm();
		delay(2000);
		digitalWrite(13, LOW);
	}

	DS3231_get(&t);

	if (t.sec != oldSec)
	{
		// display current time
		snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d\n", t.year,
				t.mon, t.mday, t.hour, t.min, t.sec);
		Serial.print(buff);

		oldSec = t.sec;
	}
}

void set_next_alarm(void)
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

	// Add a some seconds to current time. If overflow increment minutes.
	wake_SECOND = wake_SECOND + 30;
	if (wake_SECOND > 59)
	{
		wake_MINUTE++;
		wake_SECOND = wake_SECOND - 60;
	}

	// set Alarm1
	DS3231_set_a1(wake_SECOND, wake_MINUTE, wake_HOUR, 0, flags);

	// activate Alarm1
	DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A1IE);
}
