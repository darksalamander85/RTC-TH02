/**
 * MySensors_RTC+TH02_sensor
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0: GUILLOTON Laurent
 * Version 1.1 - 2017-07-17: Création du sketch initial
 * Version 1.2 - 2017-11-09: Ajout de la reconnaissance heure d'hivers/heure d'été
 *
 * DESCRIPTION
 *
 * MySensors_RTC+TH02_sensor est un sketch d'un thermomètre connecté avec affichage de la date, de l'heure et
 * de la température.
 *
 * Il contient un module RTC I2C (DS3232), un capteur de T° et d'H I2C (TH02) et un écran OLED
 *
 */

// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69

#include "Arduino.h"
#include <Wire.h>
#include <SPI.h>
#include <MySensors.h>
#include <TimeLib.h>
#include <DS3232RTC.h>  // A  DS3231/DS3232 library
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <TH02_dev.h>

bool timeReceived = false;
unsigned long lastUpdate=0, lastRequest=0, lastMeasure = 0;


// initialisation des constantes
unsigned long SLEEP_TIME = 60000; // Sleep time between reads (in milliseconds)
unsigned long UPDATE_TIME = 1000;

float Temperature;
float Humidity;
float lastTemp;
float lastHum;

boolean saison = false; //0 pour hiver
int GMT_FRANCE=1;

// Initialize display. Google the correct settings for your display.
// The follwoing setting should work for the recommended display in the MySensors "shop".
//LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
#define I2C_ADDRESS_OLED 0x3C
SSD1306AsciiWire oled;

#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1


MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);

void setup()
{
	TH02.begin();
	// the function to get the time from the RTC
	setSyncProvider(RTC.get);

	// Request latest time from controller at startup
	requestTime();

	// initialize the lcd for 16 chars 2 lines and turn on backlight
	Wire.begin();
	oled.begin(&Adafruit128x64, I2C_ADDRESS_OLED);
	oled.setFont(System5x7);
	oled.clear();
}

void presentation()  {
	// Send the sketch version information to the gateway and Controller
	sendSketchInfo("MySensors_RTC+TH02_sensor", "1.20");

	// Register all sensors to gw (they will be created as child devices)
	present(CHILD_ID_HUM, S_HUM);
	present(CHILD_ID_TEMP, S_TEMP);
}

// This is called when a new time value was received
void receiveTime(unsigned long controllerTime) {
	// Ok, set incoming time
	Serial.print("Time value received: ");
	Serial.println(controllerTime);
	tmElements_t tm;
	breakTime(controllerTime, tm);

	// last sunday of march
	int beginDSTDate=  (31 - (5* tm.Year /4 + 4) % 7);
	Serial.println(beginDSTDate);
	int beginDSTMonth=3;
	//last sunday of october
	int endDSTDate= (31 - (5 * tm.Year /4 + 1) % 7);
	Serial.println(endDSTDate);
	int endDSTMonth=10;
	// DST is valid as:
	if (((tm.Month > beginDSTMonth) && (tm.Month < endDSTMonth))
			|| ((tm.Month == beginDSTMonth) && (tm.Day >= beginDSTDate))
			|| ((tm.Month == endDSTMonth) && (tm.Day <= endDSTDate)))
		saison=1;      // DST europe = utc +2 hour (summer time)
	else saison=0; // nonDST europe = utc +1 hour (winter time)
	tm.Hour=tm.Hour+GMT_FRANCE+saison*1;
	RTC.write(tm); // this sets the RTC to the time from controller - which we do want periodically
	timeReceived = true;
	//	Serial.println("Time :");
}

void loop()
{
	unsigned long now = millis();
	boolean needMeasureRefresh = (now - lastMeasure) > SLEEP_TIME;
	if (needMeasureRefresh)
	{
		lastMeasure = now;
		Temperature = TH02.ReadTemperature();
		if (isnan(Temperature)) {
			Serial.println("Failed reading temperature from sensor");
		} else if (Temperature != lastTemp) {
			lastTemp = Temperature;
			send(msgTemp.set(Temperature, 1));
		}
		Humidity = TH02.ReadHumidity();
		if (isnan(Humidity)) {
			Serial.println("Failed reading humidity from sensor");
		} else if (Humidity != lastHum) {
			lastHum = Humidity;
			send(msgHum.set(Humidity, 1));
		}
	}

	// If no time has been received yet, request it every 10 second from controller
	// When time has been received, request update every hour
	if ((!timeReceived && (now-lastRequest) > (10UL*1000UL))
			|| (timeReceived && (now-lastRequest) > (60UL*1000UL*60UL))) {
		// Request time from controller.
		requestTime();
		Serial.println("Time request");
		lastRequest = now;
	}

	// Update display every second
	if (now-lastUpdate > UPDATE_TIME) {
		updateDisplay();
		lastUpdate = now;
	}
}


void updateDisplay(){
	tmElements_t tm;
	RTC.read(tm);

	// Print date and time
	oled.home();
	oled.set2X();
	oled.print("    ");
	oled.print(tm.Day);
	oled.print("/");
	oled.print(tm.Month);

	oled.setCursor ( 0, 3 );
	oled.print("  ");
	printDigits(tm.Hour);
	oled.print(":");
	printDigits(tm.Minute);
	oled.print(":");
	printDigits(tm.Second);

	float Temperature = TH02.ReadTemperature();
	float Humidity = TH02.ReadHumidity();
	send(msgTemp.set(Temperature, 1));
	send(msgHum.set(Humidity, 1));

	// Go to next line and print temperature
	oled.setCursor ( 0, 5 );
	oled.print("T: ");
	oled.print(Temperature,1);
}


void printDigits(int digits){
	if(digits < 10)
		oled.print('0');
	oled.print(digits);
}
