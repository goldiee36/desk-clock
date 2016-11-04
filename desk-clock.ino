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
#define VOLTACHECKINTERVAL 14 //in seconds
byte lastSleepCounterVolta = 1;

//DHT STUFF
float tempe = 0;
float humid = 0;
#define DHTCHECKINTERVAL 12 //in seconds
byte lastSleepCounterDht = 1;

//CLOCK MODULE STUFF
byte secon = 0;
byte minut = 0;

//MENU STUFF
#define MENUTIMEOUT 5000 //in milliseconds //has to be bigger then FAULTYBUTTONWAIT
boolean inMenu = false;

//DEBOUNCE BUTTONS
#define DEBOUNCEWAIT 100 //in milliseconds
volatile unsigned long buttonPushedMillis = 0 - MENUTIMEOUT;
volatile unsigned long button1PushedMillis = 0 - DEBOUNCEWAIT;
volatile unsigned long button2PushedMillis = 0 - DEBOUNCEWAIT;

//INTERRUPT DETECTION
volatile boolean buttonInterrupt = false;

//BUTTONS
volatile boolean button1Pushed = false;
volatile boolean button2Pushed = false;
volatile boolean button3Pushed = false;
#define FAULTYBUTTONWAIT 8000 //in milliseconds
#define LONGBUTTONWAIT 2000 //in milliseconds
boolean longbutton = false;

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
#define SLEEPTIME 2 //in seconds, have to be the same as in the main sleep section!!
byte sleepCounter = 0;


//SCREENS
byte numberOfScreens = 3;
byte currentScreen = 2; //starts from 0
byte scProps[3][12] = {
  {1, 1, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0},
  {3, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4}
};

//OLED
boolean oledNeeded = true;


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
  
  Serial.begin(57600);

  //button1 interrupt handler
  attachInterrupt(buttonPin1-2, button1Interrupt, FALLING); //2-2 = 0 means digital pin 2
  attachInterrupt(buttonPin2-2, button2Interrupt, FALLING); //3-2 = 1 means digital pin 3

  nextScreen();
}


void loop() {
  if ((byte)(sleepCounter - lastSleepCounterVolta) >= VOLTACHECKINTERVAL / SLEEPTIME) {
    lastSleepCounterVolta = sleepCounter;
    while (true) {// while loop will be breaked if the voltage is OK. We need this while loop to remeasure voltage after shutdown/power on
      delay(10); //needed because of the voltage meas
      volta = vcc.Read_Volts();
      //Serial.print("reading volta "); Serial.println(volta); delay(10);
  
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


  if ((byte)(sleepCounter - lastSleepCounterDht) >= DHTCHECKINTERVAL / SLEEPTIME) {
    lastSleepCounterDht = sleepCounter;
    if (dht.read()) { //read success
      humid = dht.getHumidity();
      tempe = dht.getTemperature();
      //Serial.print("r dht "); Serial.print(tempe); Serial.print(" "); Serial.println(humid); delay(10);
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
    if (seconChange) lastSecon = secon;
    else lastSecon = 99; //trigger the next update if screen changed
    if (humidChange) lastHumid = humid;
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
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    if (buttonInterrupt) {//not to increase the sleepcounter because sleep may have been interrupted
      buttonInterrupt = false;
    }
    else {
      sleepCounter++;
      //Serial.println(sleepCounter); delay(10);
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
      //Serial.println("cheking buttons"); delay(10);
      if (longbutton) { //here we have to go in until the user release all the buttons
        //Serial.println("waiting to release buttons"); delay(10);
        if (digitalRead(buttonPin1) == HIGH && digitalRead(buttonPin2) == HIGH && digitalRead(buttonPin3) == HIGH) {
          //Serial.println("all released"); delay(10);
          longbutton = false;
          button1Pushed = false;
          button2Pushed = false;
          button3Pushed = false;
          button1PushedMillis = millis(); //not accepting new commands for debounce time (debouncing the release of the button)
          button2PushedMillis = millis();
        }
      }
      else if ( //we need some actions but no action until all the buttons are released OR longbutton timeout runs out
        (
          digitalRead(buttonPin1) == HIGH &&
          digitalRead(buttonPin2) == HIGH &&
          digitalRead(buttonPin3) == HIGH
        ) ||
        ( (millis() - buttonPushedMillis) > LONGBUTTONWAIT )
      ) { // ACTION!
        //Serial.println("ACTION"); delay(10);
        if ( (millis() - buttonPushedMillis) > LONGBUTTONWAIT ) {
          //Serial.println("LONG"); delay(10);
          longbutton = true; //we have to watch the button release debounce to not to trigger button push again, as the user release the button(s) after the action executed
        }
        else { //buttons released so we can clear these booleans
          //Serial.println("SHORT"); delay(10);
          longbutton = false;          
        }

        if (button1Pushed) {
          if (button2Pushed) {
            if (button3Pushed) { // all 3 button pushed
              if (longbutton) { // LONG
              }
              else { // SHORT
              }
            }
            else { // button 1 and 2 pushed
              if (longbutton) { // LONG
              }
              else { // SHORT //this one not so stable so dont use :)
              }
            }
          }
          else {
            if (button3Pushed) { // button 1 and 3 pushed
              if (longbutton) { // LONG
              }
              else { // SHORT
              }
            }
            else { // only 1 pushed
              if (longbutton) { // LONG
              }
              else { // SHORT
                oledNeeded = !oledNeeded;
              }
            }
          }
        }
        else if (button2Pushed) {
          if (button3Pushed) { // button 2 and 3 pushed
            if (longbutton) { // LONG
            }
            else { // SHORT
            }
          }
          else { // only 2 pushed
            if (longbutton) { // LONG
            }
            else { // SHORT
              nextScreen();
            }
          }
        }
        //ACTION ENDS
        if (longbutton == false) { //if short clear booleans
          button1Pushed = false;
          button2Pushed = false;
          button3Pushed = false;
          button1PushedMillis = millis(); //not accepting new commands for debounce time (debouncing the release of the button)
          button2PushedMillis = millis();
        }
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
  }
  button2PushedMillis = millis();
  buttonPushedMillis = millis();
  buttonInterrupt = true;
}

void UpdateDisplay() {
  // picture loop
  digitalWrite(enableClockPin, LOW);
  u8g.firstPage(); 
  do {
    drawScreen();
  }
  while(u8g.nextPage());
  digitalWrite(enableClockPin, HIGH);
  //Serial.println("display updated"); delay(10);
}

byte getXbyPosi(byte posi, boolean shortNumber, byte unitLength, boolean forUnit) {
  byte returny = 0;
  byte plusForUnit = 42;
  switch (posi) {
    case 1:
    case 4:
    case 7:
      plusForUnit -= shortNumber ? 12 : 0 ; //full small char left
      returny = 0;
      break;
    case 2:
    case 5:
    case 8:
      plusForUnit -= shortNumber ? 6 : 0 ; //half small char left
      returny = shortNumber ? 39 + ((20 - unitLength) / 2) : 33 + ((20 - unitLength) / 2) ; //half small char right
      break;
    case 3:
    case 6:
    case 9:
      returny = shortNumber ? 78 + (20 - unitLength) : 66 + (20 - unitLength) ; //full small char right
      break;
    case 11:
    case 12:
      plusForUnit -= shortNumber ? 20 : 0 ; //half big char left
      returny = shortNumber ? 14 + ((40 - unitLength) / 2) : 0 + ((40 - unitLength) / 2) ; //half big char right //todo big unit length is really 40?
      break;
  }
  return forUnit ? returny + plusForUnit : returny ;
}

byte getYbyPosi(byte posi) {
  switch (posi) {
    case 1:
    case 2:
    case 3:
      return 18;
      break;
    case 4:
    case 5:
    case 6:
    case 11:
      return 41;
      break;
    case 7:
    case 8:
    case 9:
    case 12:
      return 64;
      break;
    default:
      return 41;
    break;
  }
}

void drawFloat(byte posi, float value, byte res, byte unit) {
  // posi: 1-9 small blocks: 1-top-left, 2-top-center, 3-top-right, 4-middle-left etc. 11-12 big blocks: 11-top, 12-bottom
  // res: 1: 12.3 (temper, humid), 2: 1.23 (volta)
  // unit: 1-C° (alt+0176), 2-F°, 3-%, 4-V, 5-$, 6-€ (alt+0128) which is an equal and a ( or C
  byte unitLen = 0;
  byte curXpos = 0;
  
  switch (unit) {
    case 1: {
      u8g.setFont(u8g_font_timR14);      
      char unitstr[3] = "C ";
      unitstr[1] = 176;
      unitLen = 19; //getStrWidth(unitstr) is 20 but last pixel column is empty
      u8g.setPrintPos(getXbyPosi(posi, res == 1 && value < 9.95 && value >= 0, unitLen, true), getYbyPosi(posi));    
      u8g.print(unitstr);
      }
      break;
    case 3: {
      u8g.setFont(u8g_font_timR14);      
      char unitstr[2] = "%";
      unitLen = 14; //getStrWidth(unitstr) is 15 but last pixel column is empty
      u8g.setPrintPos(getXbyPosi(posi, res == 1 && value < 9.95 && value >= 0, unitLen, true), getYbyPosi(posi));    
      u8g.print(unitstr);
      }
      break;
    case 4: {
    u8g.setFont(u8g_font_timR14);      
      char unitstr[2] = "V";
      unitLen = 13; //getStrWidth(unitstr) is 14 but last pixel column is empty
      u8g.setPrintPos(getXbyPosi(posi, res == 1 && value < 9.95 && value >= 0, unitLen, true), getYbyPosi(posi));    
      u8g.print(unitstr);  
      //Serial.print("len "); Serial.println(u8g.getStrWidth(unitstr)); delay(10);
      }
      break;
    default:
    break;
  }
  
  u8g.setFont(u8g_font_timR18r);  
  u8g.setPrintPos(getXbyPosi(posi, res == 1 && value < 9.95 && value >= 0, unitLen, false), getYbyPosi(posi));
  u8g.print(value, res);

  
}

void drawTime(byte posi, byte hours, byte minutes) {
  
}

void drawDate(byte posi, byte months, byte days, boolean dayfirst) {
  
}

void drawWeekDay(byte posi, byte weeks, byte weekdays, boolean weekalso) {
  //if weekalso true then show weeknumber and 2 letter weekdaycode, if false then only weekday, 2 letters big, rest is small
  
}

void nextScreen() {
  if (currentScreen == numberOfScreens - 1) {
    currentScreen = 0;
  }
  else {
    currentScreen++;
  }
  seconChange = false;
  humidChange = false;
  tempeChange = false;
  minutChange = false;
  voltaChange = false;
  for (byte i=0 ; i < 12 ; i=i+2){
    switch (scProps[currentScreen][i]) {
      case 1:
        tempeChange = true;
        break;
      case 2:
        humidChange = true;
        break;
      case 3:
        voltaChange = true;
        break;
    }
  }
}

void drawScreen(void) {
  //speedo numbers if using u8g_font_osb35n then 1 number is 28 p wide
  //the dot is 14 p wide, so 1.2 = 70p 12.3 = 98p
  //u8g_font_timR18r 12p per number (11p + 1 spaceing), dot is 6p wide --> 3 number, 1 dot, 20p unitofmeas: 62 + 4 pixel space + 62 = 128
  //timR10 C: 10, C°: 16
  //timR12 C: 11, C°: 18
  //timR14 C: 13, C°: 20
  //u8g_font_helvB18r 13p per number (12p + 1 spaceing), 6p per space

  for (byte i=0 ; i < 12 ; i=i+2){
    switch (scProps[currentScreen][i]) {
      case 1:
        drawFloat(scProps[currentScreen][i+1], tempe, 1, 1);
        break;
      case 2:
        drawFloat(scProps[currentScreen][i+1], humid, 1, 3);
        break;
      case 3:
        drawFloat(scProps[currentScreen][i+1], volta, 2, 4);
        break;
    }
  }

  /*drawFloat(i+1, tempe, 1, 1);
  drawFloat(i+1, humid, 1, 3);
  drawFloat(i+1, volta, 2, 4);*/

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

