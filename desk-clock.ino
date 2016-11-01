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
#define beeperPin 6 //pwm, LOW on, HIGH off
#define dhtPin 4
  #define DHTTYPE DHT22
  DHT dht(dhtPin, DHTTYPE);
#define buttonPin3 5 //input, ardu pullup needed
#define buttonPin2 3 //interrupt 1, input, ardu pullup needed
#define enableOledPin 8 //LOW on, HIGH off
#define enableClockPin 9 //LOW on, HIGH off -- it seems that clock and oled have to be switched on/off in the same time because oled is not working with powered off clock module. i2c probably interfered by the clock module. I didnt try the clock without the oled. Pullups on i2c lines may solves the problem, I didnt test.
#define LEDPIN 13

//vcc livingRoom
// Measured Vcc by multimeter divided by reported Vcc
#define VCORR 3.5/3.5;
const float VccCorrection = VCORR; //todo why float
Vcc vcc(VccCorrection);





//LOWVOTLAGE STUFF
byte lowVoltageCounter = 0;
float volta = 0;
#define VOLTACHECKINTERVAL 60 //in seconds
byte lastSleepCounterVolta = 1;

//DHT STUFF
float tempe = 0;
float humid = 0;
#define DHTCHECKINTERVAL 30 //in seconds
byte lastSleepCounterDht = 1;

//CLOCK MODULE STUFF
byte secon = 0;
byte minut = 0;

//DEBOUNCE BUTTONS
#define DEBOUNCEWAIT 100 //in milliseconds
volatile unsigned long buttonPushedMillis = 0 - DEBOUNCEWAIT;
volatile unsigned long button1PushedMillis = 0 - DEBOUNCEWAIT;
volatile unsigned long button2PushedMillis = 0 - DEBOUNCEWAIT;

//INTERRUPT DETECTION
volatile boolean buttonInterrupt = false;

//BUTTONS
volatile boolean button1Pushed = false;
volatile boolean button2Pushed = false;
volatile boolean button3Pushed = false;
volatile boolean needVolta = true;
#define FAULTYBUTTONWAIT 8000 //in milliseconds
#define LONGBUTTONWAIT 2000 //in milliseconds
boolean longbutton;

//SCREEN UPDATE BOOLEANS - current screen has to set it. If a value is true then the screen update part of the main loop will check the related variable for changes
boolean tempeChange = true;
boolean humidChange = true;
boolean voltaChange = true;
boolean minutChange = false;
boolean seconChange = false;
byte lastSecon = 99;
float lastHumid = -99;
float lastTempe = -99;
byte lastMinut = 99;
float lastVolta = -99;

//SLEEP RELATED
#define SLEEPTIME 2 //in seconds, in the main sleep section
byte sleepCounter = 0;

//MENU STUFF
#define MENUTIMEOUT 15000 //in milliseconds //has to be bigger then FAULTYBUTTONWAIT
boolean inMenu = false;




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
  attachInterrupt(buttonPin1-2, button1Interrupt, FALLING); //2-2 = 0 means digital pin 2
  attachInterrupt(buttonPin2-2, button2Interrupt, FALLING); //3-2 = 1 means digital pin 3

}


void loop() {
  if (sleepCounter - lastSleepCounterVolta >= VOLTACHECKINTERVAL / SLEEPTIME) {
    lastSleepCounterVolta = sleepCounter;
    while (true) {// while loop will be breaked if the voltage is OK. We need this while loop to remeasure voltage after shutdown/power on
      delay(10); //needed because of the voltage meas
      volta = vcc.Read_Volts();
      Serial.println("reading volta");
  
      if (volta < 3.5 ) {
        lowVoltageCounter = lowVoltageCounter < 250 ? lowVoltageCounter + 1 : lowVoltageCounter; //only increase the counter if value less then 250
        if (lowVoltageCounter > 3 || volta < 3 ) { //we need three measurement under 3.5 V OR one measurement under 3V to shut down
          blink2(); //two fast blink as low voltage   //todo to change OLED warning
          delay(1000); //some time for the user to read the display, then shutdown. We need delay here and not sleep, because sleep is interruptable
          digitalWrite(enableOledPin, HIGH);
          digitalWrite(enableClockPin, HIGH);
          LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); //low voltage lithium battery protection --> interrupt button will wake up and continue from here
          //someone pushed the button. exit from the while loop only if the voltage is above 3.5V
          digitalWrite(enableOledPin, LOW);
          digitalWrite(enableClockPin, LOW);
          u8g.begin(); //back to the beginning of the while to re-measure the voltage
        }
        else {
          break; //exit while
        }
      }
      else {
        lowVoltageCounter = 0;
        break; //exit while
      }
    }
  }
  //-----low voltage detection 


  if (sleepCounter - lastSleepCounterDht >= DHTCHECKINTERVAL / SLEEPTIME) {
    lastSleepCounterDht = sleepCounter;
    if (dht.read()) { //read success
      humid = dht.getHumidity();
      tempe = dht.getTemperature();
      Serial.println("reading dht");
    }
  }


  /*if ((millis() - clockMillis) >= CLOCKWAIT) {
    clockMillis = millis();
    setSyncProvider(RTC.get); //get the time from the RTC
    if(timeStatus() == timeSet) { //read success
      
    }   
  }*/


  if (
    (!inMenu) && (
      (seconChange && lastSecon != secon) ||
      (humidChange && lastHumid != humid) ||
      (tempeChange && lastTempe != tempe) ||
      (minutChange && lastMinut != minut) ||
      (voltaChange && lastVolta != volta)
    )
  )
  {    
    UpdateDisplay();
    if (humidChange) lastSecon = secon;
    else lastSecon = 99; //trigger the next update if screen changed
    if (seconChange) lastHumid = humid;
    else lastHumid = -99;
    if (tempeChange) lastTempe = tempe;
    else lastTempe = -99;
    if (minutChange) lastMinut = minut;
    else lastMinut = 99;
    if (voltaChange) lastVolta = volta;
    else lastVolta = -99;
  }



  if ((millis() - buttonPushedMillis) >= MENUTIMEOUT) {
    //we want to sleep as much as we can
    //this code is needed to update the sleepCounter, which will trigger to update the temperature, time etc. values
    inMenu = false; //this throw us out from the menu
    digitalWrite(enableClockPin, HIGH);
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    digitalWrite(enableClockPin, LOW);
    if (buttonInterrupt) {//not to increase the sleepcounter because sleep may have been interrupted
      buttonInterrupt = false;
    }
    else {
      sleepCounter++;
    }
  }
  
  if (inMenu) {
    //we are in the menu!
    
  }
  else if (button1Pushed || button2Pushed) { //button detection outside of the menu = standby, with oled on or off does not matter
    if (digitalRead(buttonPin3) == LOW) { //we cannot detect button3 alone, just with together an interruptbutton (1 or 2)
      button3Pushed = true;
    }
    if ((millis() - buttonPushedMillis) > DEBOUNCEWAIT) { //no action until debounce time elapsed, because we have to wait for the long button pushes (debouncing rising edge)
      if ( //no action until all the detected buttons are released OR failty button timeout runs out
      (button1Pushed && digitalRead(buttonPin1) == HIGH) ||
      (button2Pushed && digitalRead(buttonPin2) == HIGH) ||
      (button3Pushed && digitalRead(buttonPin3) == HIGH) ||
      ((millis() - buttonPushedMillis) > FAULTYBUTTONWAIT)
      ) { //button booleans will be cleared in the end of this section, as we want to enter here only once
        if ( (millis() - buttonPushedMillis) > FAULTYBUTTONWAIT ) {
          //do nothing
          //todo print some faulty button message, maybe disable button
        }
        else { // ACTION!
          if ( (millis() - buttonPushedMillis) > LONGBUTTONWAIT ) longbutton = true;
          else longbutton = false;

          if (button1Pushed) {
            if (button2Pushed) {
              if (button3Pushed) { // all 3 button pushed
                if (longbutton) { // LONG
                  Serial.println("all long");
                }
                else { // SHORT
                  Serial.println("all short");
                }
              }
              else { // button 1 and 2 pushed
                if (longbutton) { // LONG
                  Serial.println("1 2 long");
                }
                else { // SHORT
                  Serial.println("1 2 short");
                }
              }
            }
            else {
              if (button3Pushed) { // button 1 and 3 pushed
                if (longbutton) { // LONG
                  Serial.println("1 3 long");
                }
                else { // SHORT
                  Serial.println("1 3 short");
                }
              }
              else { // only 1 pushed
                if (longbutton) { // LONG
                  Serial.println("1 long");
                }
                else { // SHORT
                  Serial.println("1 short");
                }
              }
            }
          }
          else if (button2Pushed) {
            if (button3Pushed) { // button 2 and 3 pushed
              if (longbutton) { // LONG
                Serial.println("2 3 long");
              }
              else { // SHORT
                Serial.println("2 3 short");
              }
            }
            else { // only 2 pushed
              if (longbutton) { // LONG
                Serial.println("2 long");
              }
              else { // SHORT
                Serial.println("2 short");
              }
            }
          }
        }
        button1Pushed = false;
        button2Pushed = false;
        button3Pushed = false;
        button1PushedMillis = millis(); //not accepting new commands for debounce time (debouncing the release of the button)
        button2PushedMillis = millis();
      }
    }
  }  



  
} //end of main loop

//called by the digital pin 2 interrupt on FALLING edge
void button1Interrupt()
{
  if ((millis() - button1PushedMillis) >= DEBOUNCEWAIT) { //debouncing falling edge
    button1Pushed = true;
  }
  button1PushedMillis = millis();
  buttonPushedMillis = millis();
  buttonInterrupt = true;
}

//called by the digital pin 3 interrupt on FALLING edge
void button2Interrupt()
{
  if ((millis() - button1PushedMillis) >= DEBOUNCEWAIT) {
    button2Pushed = true;
    needVolta = true;
  }
  button2PushedMillis = millis();
  buttonPushedMillis = millis();
  buttonInterrupt = true;
}

void UpdateDisplay() {
  // picture loop
  u8g.firstPage(); 
  do {
    drawScreen();
  }
  while(u8g.nextPage());
  needVolta = false;
  Serial.println("display updated");
}

void drawScreen(void) {
  //speedo numbers if using u8g_font_osb35n then 1 number is 28 p wide
  //the dot is 14 p wide, so 1.2 = 70p 12.3 = 98p
  //u8g_font_timR18r 12p per number (11p + 1 spaceing), dot is 6p wide --> 3 number, 1 dot, 20p unitofmeas: 62 + 4 pixel space
  
  u8g.setFont(u8g_font_timR18r);
  if (tempe < 9.95)
    u8g.setPrintPos(12, 18);
  else
    u8g.setPrintPos(0, 18);
  u8g.print(tempe, 1);

  u8g.setFont(u8g_font_timR18r);
  if (humid < 9.95)
    u8g.setPrintPos(76, 64);
  else
    u8g.setPrintPos(64, 64);
  u8g.print(humid, 1);

  u8g.setFont(u8g_font_timR10r);
  u8g.setPrintPos(42, 18);
  u8g.print("C*");
  u8g.setPrintPos(108, 63);
  u8g.print("%");

  if (needVolta) {
    u8g.setFont(u8g_font_timR18r);
    u8g.setPrintPos(30, 41);
    u8g.print(volta, 2);
  }

  /*u8g.setFont(u8g_font_timR18r);
  u8g.setPrintPos(0, 41);
  u8g.print(volta, 2);
  u8g.setFont(u8g_font_timR18r);
  u8g.setPrintPos(0, 64);
  u8g.print(volta, 2);

  u8g.setPrintPos(64, 18);
  u8g.print(volta, 2); 
  u8g.setPrintPos(64, 41);
  u8g.print(volta, 2); 
  u8g.setPrintPos(64, 64);
  u8g.print(volta, 2); */
  
  
  // km/h label
  /*u8g.setFont(u8g_font_timB14r);
  u8g.setPrintPos(100, 15);
  u8g.print("km");
  u8g.drawHLine(100,17,25);
  u8g.setPrintPos(109, 33);
  u8g.print("h");

  
  
  //daily counters  
  u8g.setFont(u8g_font_helvB18r);
  //u8g_font_helvB18r 13p per number (12p + 1 spaceing), 6p per space
  //u8g_font_timR18r 12p per number (11p + 1 spaceing), dot is 6p wide --> 3 number, 1 dot, 20p unitofmeas: 62 + 4 pixel space
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

/*measurements:
3.962 on the display, with big numbers --> 17 mA
same with arduino powered off --> 11 mA
3.962 on the display, with small numbers --> 9 mA
same with arduino powered off --> 4 mA
same with arduino and clock powered off --> 3.3 mA
powered off ardu and display --> 11uA
*/

