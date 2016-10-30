#include "U8glib.h"
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK);
#include "LowPower.h"
#include <DS3232RTC.h>
#include <TimeLib.h>
#include <Wire.h>
#include "DHT.h" //Modified version of the library!! Original library uses millis/delay to ensure 2 seconds elapses between the measures - with power down it is not working
#include <Vcc.h>

//digital pins
#define buttonPin1 2 //interrupt 0, input, ardu pullup needed
#define beeperPin 3 //pwm, LOW on, HIGH off
#define dhtPin 4
  #define DHTTYPE DHT22
  DHT dht(dhtPin, DHTTYPE);
#define buttonPin3 5 //input, ardu pullup needed
#define buttonPin2 6 //inpuy, ardu pullup needed
#define enableOledPin 8 //LOW on, HIGH off
#define enableClockPin 9 //LOW on, HIGH off
#define LEDPIN 13

//vcc livingRoom
// Measured Vcc by multimeter divided by reported Vcc
#define VCORR 3.5/3.5;
const float VccCorrection = VCORR; //todo why float
Vcc vcc(VccCorrection);


void setup() {
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);

  pinMode(beeperPin, OUTPUT);
  pinMode(enableOledPin, OUTPUT);
  pinMode(enableClockPin, OUTPUT);
  digitalWrite(beeperPin, HIGH);
  digitalWrite(enableOledPin, LOW);
  digitalWrite(enableClockPin, LOW);

  //dht22
  dht.begin();

  //u8g
  u8g.begin(); 
  u8g.setRot180();
  
  Serial.begin(9600);

  //button1 interrupt handler
  attachInterrupt(buttonPin1-2, wakeUp, FALLING); //2-2 = 0 means digital pin 2

}

byte lowVoltageCounter = 0;
float volta = 0;
#define VOLTAWAIT 5000 //in milliseconds
unsigned long voltaMillis = millis() - VOLTAWAIT;

float temper = 0;
float humid = 0;
#define DHTWAIT 10000 //in milliseconds
unsigned long dhtMillis = millis() - DHTWAIT;

boolean needRefresh = true;
#define REFRESHWAIT 1000 //in milliseconds
unsigned long refreshMillis = millis() - REFRESHWAIT;

unsigned long sleepMillis = millis();

void loop() {
  //low voltage detection
  if ((millis() - voltaMillis) >= VOLTAWAIT) {
    voltaMillis = millis();
    volta = vcc.Read_Volts();
    
    if (volta < 3.5 ) {
      lowVoltageCounter = lowVoltageCounter < 250 ? lowVoltageCounter + 1 : lowVoltageCounter; //only increase the counter if value less then 250
      if (lowVoltageCounter > 3 || volta < 3 ) { //we need three measurement under 3.5 V OR one measurement under 3V to shut down
        blink2(); //two fast blink as low voltage   //todu to change OLED warning
        LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); //low voltage lithium battery protection --> interrupt button will wake up and continue from here
        voltaMillis = millis() - VOLTAWAIT; //interrupt button pushed, new voltage measurement needed if battery is charged or not
      }
    }
    else {
      lowVoltageCounter = 0;
    }
  }
  //-----low voltage detection 

  if ((millis() - dhtMillis) >= DHTWAIT) {
    dhtMillis = millis();
    if (dht.read()) {
      humid = dht.getHumidity();
      temper = dht.getTemperature();
    }
  }

  if ((millis() - sleepMillis) >= 12000) {
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  }


  /*if (needRefresh) {
    needRefresh = false;
    //Serial.println(countedMagnets); //for detecting rpm bounce
    UpdateDisplay();
  }*/
}

//called by the digital pin 2 interrupt on FALLING edge
void wakeUp()
{

}

void UpdateDisplay() {
  // picture loop
  u8g.firstPage(); 
  do {
    drawScreen();
  }
  while(u8g.nextPage());
}

void drawScreen(void) {
  //speedo numbers if using u8g_font_osb35n then 1 number is 28 p wide
  //the dot is 14 p wide, so 1.2 = 70p 12.3 = 98p
  
  u8g.setFont(u8g_font_osb35n);
  //if (volta < 9.95)
  //  u8g.setPrintPos(28, 35);
  //else
    u8g.setPrintPos(0, 35);
  u8g.print(volta, 3);  
  
  
  // km/h label
  /*u8g.setFont(u8g_font_timB14r);
  u8g.setPrintPos(100, 15);
  u8g.print("km");
  u8g.drawHLine(100,17,25);
  u8g.setPrintPos(109, 33);
  u8g.print("h");

  
  
  //daily counters  
  u8g.setFont(u8g_font_helvB18r);
  // 13p per number (12p + 1 spaceing), 6p per space
  char displayStr[5];
  dtostrf(dailyKm, 4, 1, displayStr);
  byte xPosOffset = 0;
  if (displayStr[0] == ' ')
    xPosOffset += 7;
  u8g.setPrintPos(xPosOffset, 63);
  u8g.print(displayStr[0]); u8g.print(displayStr[1]);
  u8g.setFont(u8g_font_helvB14r);  
  u8g.setPrintPos(26, 63);
  u8g.print("."); u8g.print(displayStr[3]);  //dot is 4p width (3p + 1p spaceing)
  
  //summary
  // 10p per number, 5p per space
  dtostrf(sumEEPROM + (countedMagnets*MAGNETDISTANCE)/100000, 4, 0, displayStr);
  u8g.setPrintPos(108-u8g.getStrWidth(displayStr), 63);
  u8g.print(displayStr);

  //km labels for daily and summary counters  
  u8g.setFont(u8g_font_timB10r);
  u8g.setPrintPos(40, 63);
  u8g.print("km");
  u8g.setPrintPos(108, 63);
  u8g.print("km");
  */
}

void blink2() {
  digitalWrite(LEDPIN, HIGH); LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, LOW); LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, HIGH); LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, LOW);
}
