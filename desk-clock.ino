#include "U8glib.h"
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK);
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);  // I2C / TWI 
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST); // Fast I2C / TWI 
#include "LowPower.h"
#include <Wire.h>
#include "DHT.h" //Modified version of the library!! Original library uses millis/delay to ensure 2 seconds elapses between the measures - with power down it is not working
#include <Vcc.h>

//SCREEN BASICS:
//big numbers: u8g_font_osr35n
//small numbers: u8g.setFont(u8g_font_timR18r
//X: 2 small blocks: 62 + 4 + 62, or large block can take all 128 -----OR, middle small block starts at 33
//Y: 3 small blocks: 18 + 5 + 18 + 5 + 18, or large box either: 41 + 5 + 18 or 18 + 5 + 41

  //numbers if using u8g_font_osb35n then 1 number is 28 p wide
  //the dot is 14 p wide, so 1.2 = 70p 12.3 = 98p  
  //u8g_font_timR18r 12p per number (11p + 1 spaceing), dot is 6p wide (5p + 1p space) --> 3 number (3*12p) + 1 dot (6p) + 20p unitofmeas = 62 -- two small block: 62 + 4 pixel space + 62 = 128
  //timR10 C: 10, C°: 16
  //timR12 C: 11, C°: 18
  //timR14 C: 13, C°: 20
  //u8g_font_helvB18r 13p per number (12p + 1 spaceing), 6p per space

//MENU: timR14 --> 3 row content, above this the history what the buttons do (eg: left:back, mid:down, right: enter), then above and below the 3 row content leave some space to indicate if there are more lines above or below or both
// numeric value change: left OK, middle minus, right plus
//edit screen: after selecting screen number a layout appears: left button accepts, middle change frame selection (posi 1,2, etc), right change content (value, including nothing)


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
const float VccCorrection = VCORR; //todo save into EEPROM, change from MENU
Vcc vcc(VccCorrection);





//LOWVOTLAGE STUFF
byte lowVoltageCounter = 0;
float volta = 0;
#define VOLTACHECKINTERVAL 85 //in seconds
unsigned int lastSleepCounterVolta = 1;
boolean shutDownHappened = false;

//DHT STUFF
float tempe = 0;
float humid = 0;
#define DHTCHECKINTERVAL 35 //in seconds
unsigned int lastSleepCounterDht = 1;

//CLOCK MODULE STUFF
int yeara = 1960;
byte monta = 88;
byte moday = 88;
byte weday = 88;
byte houra = 88;
byte minut = 88;
byte secon = 88;
#define CLOCKCHECKINTERVAL 90 //in seconds
unsigned int lastSleepCounterClock = 1;
unsigned long updateSeconMillis = 0 ;
byte lastAutoOnOffMinute = 100; // value 100 means enable

//MENU STUFF
#define MENUTIMEOUT 10000 //in milliseconds
boolean inMenu = false;

//DEBOUNCE BUTTONS
#define DEBOUNCEWAIT 100 //in milliseconds
volatile unsigned long buttonPushedMillis = 0 ; //- MENUTIMEOUT;
volatile unsigned long button1PushedMillis = 0 - DEBOUNCEWAIT;
volatile unsigned long button2PushedMillis = 0 - DEBOUNCEWAIT;

//INTERRUPT DETECTION
volatile boolean buttonInterrupt = false;

//BUTTONS
volatile boolean button1Pushed = false;
volatile boolean button2Pushed = false;
volatile boolean button3Pushed = false;
#define LONGBUTTONWAIT 2000 //in milliseconds
boolean longbutton = false;

//SCREEN UPDATE BOOLEANS - current screen has to set it. If a value is true then the screen update part of the main loop will check the related variable for changes
boolean tempeChange = true;
boolean humidChange = true;
boolean voltaChange = true;
boolean minutChange = false;
float lastHumid = -99;
float lastTempe = -99;
byte lastMinut = 99;
float lastVolta = -99;

//SLEEP RELATED
#define NONSLEEPTIMEOUT 3000 
#define SLEEPTIME 2 //in milliseconds
#if SLEEPTIME == 1
  #define SLEEPENUM SLEEP_1S
#elif SLEEPTIME == 2
  #define SLEEPENUM SLEEP_2S
#elif SLEEPTIME == 4
  #define SLEEPENUM SLEEP_4S
#elif SLEEPTIME == 8
  #define SLEEPENUM SLEEP_8S
#else
  #error valid SLEEPTIME values: 1 2 4 8
#endif
unsigned int sleepCounter = 0;


//SCREENS - WHAT TYPE WHICH POSITION
byte currentScreen = 0; //starts from 0
byte scProps[][12] = { //TODO sanity check if displays are not covering each other
  {4, 11, 1, 7, 2, 9, 0, 0, 0, 0, 0, 0},  //this is one screen, with 6 possibilites of data printed. what-where, 1-1 means temperature to position 1; 2-9 means humidity to position 9 etc.
  {4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  //{99, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
byte numberOfScreens = sizeof(scProps)/sizeof(scProps[0]);

byte autoOnOff[][2] = {
  {88, 88},  //sunday on times, hour, minute
  {0, 1}, //sunday off times
  {8, 30}, //mon on
  {18, 0}, //mon off etc.
  {8, 30}, //thu
  {18, 0},
  {8, 30}, //wed
  {18, 0},
  {8, 30}, //thu
  {18, 0},
  {8, 30}, //fri
  {18, 0},
  {88, 88}, //sat
  {0, 1},
};

//OLED
boolean oledNeeded = true;
boolean forceUpdateDisplay = false;
#define CONTRAST 0


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

  delay(200);

  //dht22
  dht.begin();

  //u8g
  u8g.begin(); 
  u8g.setRot180();
  u8g.setContrast(CONTRAST);

  
  
  Serial.begin(115200);

  //RTC DS3231
  Wire.begin();  

  //button1 interrupt handler
  attachInterrupt(buttonPin1-2, button1Interrupt, FALLING); //2-2 = 0 means digital pin 2
  attachInterrupt(buttonPin2-2, button2Interrupt, FALLING); //3-2 = 1 means digital pin 3

  setCurrentScreen(currentScreen); //it is needed to set up the trigger types for refreshing the display for the actual screen
}


void loop() {

  //UPDATE MAIN VALUES //updating when sleepcounter increase.TODO maybe add some time based update also so update also when in menu

  //VOLTAGE
   
  if ( (sleepCounter - lastSleepCounterVolta) >= (float)(VOLTACHECKINTERVAL / SLEEPTIME)) {
    lastSleepCounterVolta = sleepCounter;

    while (true) {// while loop will be breaked if the voltage is OK. We need this while loop to remeasure voltage after shutdown/power on
      delay(10); //needed because of the voltage meas after sleep
      //Serial.println("volt"); delay(10);      
      float oldvolta = volta;
      volta = vcc.Read_Volts();
      if (shutDownHappened) { //display old voltage (why the shutdown happened) and the new one
        oledNeeded = true;
        digitalWrite(enableOledPin, LOW);
        digitalWrite(enableClockPin, LOW); //needed before the u8g.begin, warningDisplay will turn it off
        u8g.begin(); //prepare display
        u8g.setContrast(CONTRAST);
        printWarningDisplay("Battery was low", SLEEP_2S); //bit longer time to the user to read the display
        //depends on the new voltage we will shutdown again or stay alive and exit the while
      }
  
      if (volta < 3.4 ) {
        if (volta < 3) //shut down immediately
          lowVoltageCounter = 250;
        else
          lowVoltageCounter = lowVoltageCounter < 250 ? lowVoltageCounter + 1 : lowVoltageCounter; //only increase the counter if value less then 250

        if (lowVoltageCounter > 3) { //we need three measurement under 3.5 V OR one measurement under 3V to shut down
          //OLED warning - include some sleep time for the user to read the display - but probably wont notice here at that point
          if (shutDownHappened) printWarningDisplay("Batt. still low!", SLEEP_2S);
          else printWarningDisplay("Battery low!", SLEEP_1S);
          digitalWrite(enableOledPin, HIGH);
          digitalWrite(enableClockPin, HIGH);
          shutDownHappened = true;
          LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); //low voltage lithium battery protection --> interrupt button will wake up and continue from here
          //someone pushed the button. exit from the while loop only if the voltage is above 3.5V
          //back to the beginning of the while to re-measure the voltage
        }
        else { //only small voltage violation, just counter increased          
          break; //exit while
        }
      }
      else { //no voltage violation, clear lowVoltageCounter
        if (shutDownHappened) {
          //if we are leaving the while we need this to remeasure stuff before updating the display
          lastSleepCounterClock = sleepCounter - 10000; //force time update after leaving lowVoltage code
          lastSleepCounterDht = sleepCounter - 10000; //force dht updateafter leaving lowVoltage code
          forceUpdateDisplay = true; //after measurements the display will be updated/turned on (after leaving lowVoltage code)
          shutDownHappened = false;
        }        
        lowVoltageCounter = 0;
        break; //exit while
      }
    }
  }
  //-----low voltage detection 


  //TEMPERATURE AND HUMIDITY

  if ( (sleepCounter - lastSleepCounterDht) >= (float)(DHTCHECKINTERVAL / SLEEPTIME)) {
    lastSleepCounterDht = sleepCounter;
    //Serial.println("dht"); delay(10);
    
    if (dht.read()) { //read success
      humid = dht.getHumidity();
      tempe = dht.getTemperature();      
      //Serial.print(tempe); Serial.print(" "); Serial.println(humid); delay(10);
    }
    else {
      humid = -88;
      tempe = -88;
    }
  }
  // -------------------- TEMPERATURE AND HUMIDITY


  //TIME
  
  if ( (sleepCounter - lastSleepCounterClock) >= (float)(CLOCKCHECKINTERVAL / SLEEPTIME) ) {
    lastSleepCounterClock = sleepCounter;
    //Serial.println("time"); delay(10);

    if (!readTime(secon, minut, houra, weday, moday, monta)) {
      //Serial.print("T "); Serial.println(secon); delay(10);
      updateSeconMillis=millis()-500; //this will keep the time uptodate when not in sleep /-500: we are on a bit late anyway
    }
    else {
      yeara = 1960;
      monta = 88;
      moday = 88;
      weday = 88;
      houra = 88;
      minut = 88;
      secon = 88;      
    }
  }
  //-------------------------------------------


  //timekeeping when not in sleep
  if ((millis() - updateSeconMillis) > 1000) {
    increaseTime(1);
    //Serial.print("A "); Serial.println(secon); delay(10);
    updateSeconMillis=millis();
  }
  //-----------------------------------------



  //auto ON OFF
  //code needed to not to run the ON or OFF code again while we are in the same minute
  if (lastAutoOnOffMinute != 100 && lastAutoOnOffMinute != minut) { 
    lastAutoOnOffMinute = 100; //re-enable auto ON OFF
    //Serial.println("REenable"); delay(10);
  }

  //TODO what if in menu?  --> store an action, and after exiting menu ask for execution. default answer is yes
  if (oledNeeded) {
    if ( autoOnOff[weday*2-1][0] == houra && autoOnOff[weday*2-1][1] == minut && lastAutoOnOffMinute == 100 ) {
      lastAutoOnOffMinute = minut;
      //Serial.println("OFF"); delay(10);
      turnOLED(false);
    }
  }
  else {
    if ( autoOnOff[(weday-1)*2][0] == houra && autoOnOff[(weday-1)*2][1] == minut  && lastAutoOnOffMinute == 100 ) {
      lastAutoOnOffMinute = minut;
      //Serial.println("ON"); delay(10);
      turnOLED(true);   
    }
  }
  //--------------------------------------------

  // --------------------------- TIME



  







  








  
  
  if (!inMenu) { //NOT in the menu stuff

    //update display if needed
    if (
      forceUpdateDisplay || ((oledNeeded) && (!inMenu) && (
        (humidChange && lastHumid != humid) ||
        (tempeChange && lastTempe != tempe) ||
        (minutChange && lastMinut != minut) ||
        (voltaChange && lastVolta != volta)
      ))
    )
    {
      UpdateDisplay();
      forceUpdateDisplay = false;
    }
  
    //----------------- update DISPLAY
    

    // SLEEP
  
    if ( ((millis() - buttonPushedMillis) >= NONSLEEPTIMEOUT) ) {
      //we want to sleep as much as we can
      //this code is needed to update the sleepCounter, which will trigger to update the temperature, time etc. values
      buttonInterrupt = false;
      LowPower.powerDown(SLEEPENUM, ADC_OFF, BOD_OFF); //actually 2S sleep is 2097.152 mS
      
      if (buttonInterrupt) {//not to increase the sleepcounter because sleep has been interrupted
        //Serial.println("sl INT"); delay(10);
        lastSleepCounterClock = sleepCounter - 10000; //force time update if sleep has been interrupted
      }
      else {
        sleepCounter++;
        increaseTime(SLEEPTIME);
        updateSeconMillis=updateSeconMillis-(unsigned long)(SLEEPTIME*100); //difference between real sleep time and increaseTime
        //Serial.print("s "); Serial.println(secon); delay(10);
      }
    }
  
    //-------------------- SLEEP


    
    // BUTTON handling
      if (button1Pushed || button2Pushed) { //button detection outside of the menu = standby, with oled on or off does not matter
        if (digitalRead(buttonPin3) == LOW) { //we cannot detect button3 alone (wont break sleep), just with together an interruptbutton (1 or 2)
          button3Pushed = true;
        }
        if ((millis() - buttonPushedMillis) > DEBOUNCEWAIT) { //no action until debounce time elapsed, because we have to wait for the long button pushes (debouncing rising edge)
        //Serial.println("cheking buttons"); delay(10);
        if (longbutton) { //here we have to go in until the user release all the buttons - the action for the long button press is already happened at this time
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
                  //Serial.println("S1"); delay(10);
                  if (oledNeeded) turnOLED(false);
                  else turnOLED(true);
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
                //Serial.println("S2"); delay(10);
                if (oledNeeded) nextScreen();
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
  
    //------------------ BUTTON handling
    
  }  
  else { //MENU STUFF - we are in the menu!
    
    // we need an exit based on timeout
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
  if ((millis() - button2PushedMillis) >= DEBOUNCEWAIT) {
    button2Pushed = true;
  }
  button2PushedMillis = millis();
  buttonPushedMillis = millis();  
  buttonInterrupt = true;
}



// DISPLAY

void UpdateDisplay() { //assumes that display has been initalized (powered on and u8g.begin was issued once)
  lastHumid = humid;
  lastTempe = tempe;
  lastMinut = minut;
  lastVolta = volta;
  digitalWrite(enableClockPin, LOW);
  // picture loop
  u8g.firstPage(); 
  do {
    drawScreen();
  }
  while(u8g.nextPage());
  //Serial.println("display updated"); delay(10);
  digitalWrite(enableClockPin, HIGH);
}

void printWarningDisplay(char message[], period_t period) {  //assumes that display has been initalized (powered on and u8g.begin was issued once)
  //TODO prepare multi line print, or position parameter (overloaded function to support old code)
  digitalWrite(enableClockPin, LOW);
  u8g.firstPage(); 
  do {
    drawStrUnit(11, " ", message);
  }
  while(u8g.nextPage());
  digitalWrite(enableClockPin, HIGH);
  LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}


void turnOLED(boolean desStatus) { 
  if (desStatus) {
    //Serial.println("ON"); delay(10);
    oledNeeded = true;
    digitalWrite(enableOledPin, LOW);    
    digitalWrite(enableClockPin, LOW); //needed before the u8g.begin, UpdateDisplay will turn it off
    u8g.begin();
    u8g.setContrast(CONTRAST);
    UpdateDisplay(); //its needed because values which trigger the update may have not changed.
  }
  else {
    //Serial.println("OFF"); delay(10);
    oledNeeded = false;
    digitalWrite(enableOledPin, HIGH);
  }
}


byte getXbyPosi(byte posi, byte valueLength, byte unitLength, boolean forUnit) {
  //valueLength: eg.: float like 12.3 or 1.23 --> 42 (with trailing space pixel), or 1.2 --> 30
  //if forUnit is true we need the X position for printing the unit of meas.
  byte Xpos = 0;
  switch (posi) {
    case 1:
    case 4:
    case 7:
      Xpos = 0;
      break;
    case 2:
    case 5:
    case 8:
      Xpos = 33 + ((62 - (valueLength + unitLength)) / 2) ;
      break;
    case 3:
    case 6:
    case 9:
      Xpos = 66 + (62 - (valueLength + unitLength)) ;
      break;
    case 11:
    case 12:
      Xpos = ((128 - (valueLength + unitLength)) / 2) ;
      break;
  }
  return forUnit ? Xpos + valueLength : Xpos ;
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
      return 41;
      break;
    case 11:
      return 35; // the big block floor is at 41 as the second row of small blocks, but as we use 35p high characters in this case we position it UP
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

void drawStrUnit(byte posi, char str[], char unit[]) {
  // posi: 1-9 small blocks: 1-top-left, 2-top-center, 3-top-right, 4-middle-left etc. 11-12 big blocks: 11-top, 12-bottom
  // res: 1: 12.3 (temper, humid), 2: 1.23 (volta)
  // unit: 1-C° (alt+0176), 2-F°, 3-%, 4-V, anything else like 0 is nothing
  // 5-$, 6-€ (alt+0128) which is an = and a ( or C
  if (posi > 10 ) {
    u8g.setFont(u8g_font_osr35n);
  }
  else {
    u8g.setFont(u8g_font_timR18r);
  }
  byte valueLen = 0;
  if (str != " ") {
    valueLen = u8g.getStrWidth(str);
  }
  //Serial.println(valueLen); delay(10);

  byte unitLen = 0;  
  //draw the unit of meas.  
  if (unit != " ") {
    u8g.setFont(u8g_font_timR14);
    unitLen = u8g.getStrWidth(unit) - 1;  //without the space pixel at the end
    u8g.setPrintPos(getXbyPosi(posi, valueLen, unitLen, true), getYbyPosi(posi));    
    u8g.print(unit);
  }

  //draw the number (value) itself
  //we need the unit of meas. lenght to draw the number
  if (valueLen != 0) {
    if (posi > 10 ) {
      u8g.setFont(u8g_font_osr35n); //osb35 is good, TODO measure current!
    }
    else {
      u8g.setFont(u8g_font_timR18r);
    }
    u8g.setPrintPos(getXbyPosi(posi, valueLen, unitLen, false), getYbyPosi(posi));
    u8g.print(str);
  }
}

void drawFloatUnit(byte posi, float value, char unit[]) {
  char valueStr[5];
  dtostrf(value, 3, value < 9.95 && value >= 0 ? 2 : 1, valueStr); //TODO value >= 0 are you sure?
  drawStrUnit(posi, valueStr, unit);
}


void drawTime(byte posi, byte hours, byte minutes) {
  byte neededCharLength = snprintf(NULL, 0, "%i:%02i", hours, minutes);
  char valueStr[1 + neededCharLength]; //!!!THE TWO SPRINTF FORMATS HAVE TO BE THE SAME!!!
  //char valueStr[6];
  sprintf(valueStr, "%i:%02i", hours, minutes);
  drawStrUnit(posi, valueStr, " ");
}

void drawDate(byte posi, byte months, byte days, boolean dayfirst) {
  
}

void drawWeekDay(byte posi, byte weeks, byte weekdays, boolean weekalso) {
  //if weekalso true then show weeknumber and 2 letter weekdaycode, if false then only weekday, 2 letters big, rest is small
  
}


void nextScreen() {
  if (currentScreen == numberOfScreens - 1) {
    setCurrentScreen(0);
  }
  else {
    setCurrentScreen(currentScreen+1);
  }
}


//SCREEN TYPES, WHAT CAN BE USED IN THE DISPLAY ARRAY (scProps)
void setCurrentScreen(byte desiredSc) {
  if (desiredSc >= 0 && desiredSc < numberOfScreens) {
    currentScreen = desiredSc;
    humidChange = false;
    tempeChange = false;
    minutChange = false;
    voltaChange = false;
    for (byte i=0 ; i < 12 ; i=i+2){
      switch (scProps[currentScreen][i]) { //setting a new type of refresh type (*Change) to true will force the the updateing of the screen
        case 1:
          tempeChange = true;
          break;
        case 2:
          humidChange = true;
          break;
        case 3:
          voltaChange = true;
          break;
        case 4:
          minutChange = true;
          break;
      }
    }
    UpdateDisplay(); //force refresh
  }
}

void drawScreen(void) {
  for (byte i=0 ; i < 12 ; i=i+2){
    switch (scProps[currentScreen][i]) {
      case 1: {
        char unitStr[3] = " C";
        unitStr[0] = 176;
        drawFloatUnit(scProps[currentScreen][i+1], tempe, unitStr);
        }
        break;
      case 2: {
        drawFloatUnit(scProps[currentScreen][i+1], humid, "%");
        }
        break;
      case 3: { 
        drawFloatUnit(scProps[currentScreen][i+1], volta, "V");
        }
        break;
      case 4:
        drawTime(scProps[currentScreen][i+1], houra, minut);
        break;
      case 99:
        char unitStr[3] = " C";
        unitStr[0] = 176;
        drawFloatUnit(7, 22.3, unitStr);
        drawTime(11, 15, 46);
        drawFloatUnit(9, 52.6, "%");
        break;
    }
  }
}

// ----------------------------- DISPLAY

/*uint8_t dec2bcd(uint8_t n)
{
    return n + 6 * (n / 10);
}*/

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


void beep() {
  digitalWrite(LEDPIN, HIGH); LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, LOW); LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, HIGH); LowPower.powerDown(SLEEP_30MS, ADC_OFF, BOD_OFF);
  digitalWrite(LEDPIN, LOW);
}



byte readTime(byte &myS, byte &myM, byte &myH, byte &myWd, byte &myMd, byte &myMo) {
  digitalWrite(enableClockPin, LOW);  
  if (!oledNeeded) digitalWrite(enableOledPin, LOW);
  LowPower.powerDown(SLEEP_15MS, ADC_OFF, BOD_OFF); //clock module needs some time to wake up - with enabled OLED in same cases
  byte res = readTimePure(myS, myM, myH, myWd, myMd, myMo);
  if (!oledNeeded) digitalWrite(enableOledPin, HIGH);
  digitalWrite(enableClockPin, HIGH);
  return res;
}


byte readTimePure(byte &myS, byte &myM, byte &myH, byte &myWd, byte &myMd, byte &myMo) {
  // return values:
  //0: success
  //1: data too long to fit in transmit buffer
  //2: received NACK on transmit of address
  //3: received NACK on transmit of data
  //4: other error 
  #define RTC_ADDR 0x68

  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  if ( byte e = Wire.endTransmission() ) {
    return e;
  }
  Wire.requestFrom(RTC_ADDR, 6); //request X bytes
  myS = binToDec(Wire.read());
  myM = binToDec(Wire.read());
  myH = binToDec(Wire.read() & B00111111);   //assumes 24hr clock
  myWd = Wire.read();
  myMd = binToDec(Wire.read());
  myMo = binToDec(Wire.read() & B01111111);  //don't use the Century bit
  //myMd = Wire.read();
  //myMo = Wire.read();  //don't use the Century bit
  //myYe = binToDec(Wire.read());
  return 0;
}

byte binToDec(byte val)
{//bitwise: aaaabbbb --> aaaa * 10 + bbbb
  return (10 * (val >> 4) + (val & 15));
}

void increaseTime(byte incSeconds) {
  if (secon + incSeconds < 60) {
    secon = secon + incSeconds;
  }
  else {
    secon = secon + incSeconds - 60;
    if (minut + 1 < 60) {
      minut++;
    }
    else {
      minut = 0;
      if (houra + 1 < 24) {
        houra++;
      }
      else {
        houra = 0;
        lastSleepCounterClock = sleepCounter - 10000; //force update time to get valid day etc
      }
    }
  }  
}


/*current measurements:
3.962 on the display, with big numbers --> 17 mA
with arduino powered off --> 11 mA
3.962 on the display, with small numbers --> 9 mA
with arduino powered off --> 4 mA
with arduino and clock powered off --> 3.3 mA


3,46V on the display, with small numbers --> 6.5-7.5 mA
with arduino and clock powered off --> 3.6 mA
low bright: 2.45


23.4oC 42,6% and with big BOLD 16:52 on the display --> 17 mA
with arduino and clock powered off --> 12.9 mA


test display: 22.3oC 52.6% and with big 15:46 on the display --> 15mA
with arduino and clock powered off --> 11mA
low bright: 6 mA


13,35mA


powered off ardu and display (in low voltage battery protection mode or between sleeps) --> 11uA (slowly (10-20sec) reach from 30uA



*/

