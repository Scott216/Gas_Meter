///
/// @file		Gas_Meter.ino
/// @brief		Main sketch
/// Project 	Gas_Meter Library
///
/// @details	<#details#>
/// @n @a		Developed with [embedXcode+](http://embedXcode.weebly.com)
/// 
/// @author		Scott Goldthwaite
/// @author		Scott Goldthwaite
///
/// @date		1/24/14 9:28 PM
/// @version	<#version#>
/// 
/// @copyright	(c) Scott Goldthwaite, 2014
/// @copyright	GNU General Public License
///
/// @see		ReadMe.txt for references
/// @n
///




// #define PRER3ETHERNETSHLD // comment out if using R3 or later shield.  Older shields need a hard reset after power up
// #define CRESTVIEW         // If at Crestview, change IP, Feed ID and use internal pull-up resistors

// Core library for code-sense
#include <Arduino.h>
#include <SPI.h>        // Required for Ethernet as of Arduino 0021, ref: http://arduino.cc/en/Reference/SPI
#include <Ethernet.h>   // Functions to control Ethernet shield Ref: http://www.arduino.cc/en/Reference/Ethernet
#include <HttpClient.h> // used by Cosm.h  http://github.com/amcewen/HttpClient
#include <EthernetUdp.h>// core library
#include <avr/eeprom.h> // Functions to read/write EEPROM
#include <Cosm.h>       // http://github.com/cosm/cosm-arduino
#include <Tokens.h>     // COSM API key
#include <Time.h>       // http://www.pjrc.com/teensy/td_libs_Time.html#ntp
#include <TimeAlarms.h> // http://code.google.com/p/arduino-time/source/browse/trunk/TimeAlarms/
#include <Wire.h>             // for I2C communication to LCD display
#include <Adafruit_GFX.h>     // For LCD display, supplies core graphics library http://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h> // For LCD display  http://github.com/adafruit/Adafruit_SSD1306


#define UPDATE_INTERVAL         20000   // If the connection is good wait 15 seconds before updating again - should not be less than 5 seconds per cosm requirements
#define EEPROM_ADDR_GASPULSE        0   // Address in EEPROM where gas pulse will be saved (0-512 bytes)

/*-------- NTP setup ----------*/
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
                                        // IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
                                        // IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov
const int timeZone = -5;  // Eastern Standard Time (USA)
EthernetUDP Udp;
const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets


uint8_t successes =             0;    // Cosm upload success counter, using a byte will make counter rollover at 255, which is what we want
uint8_t failures  =             0;    // Cosm upload failure counter

// Propane and pulses
uint32_t MeterReading =         0;    // Reading on gas meter
uint32_t Meter15MinAgo =        0;    // Meter reading at 15 minute intervals
uint32_t MeterStartDay =        0;    // Meter reading at begenning of day
float therms =                0.0;    // Propane usage converter to therms
float yesterdayGasCost =      0.0;    // Previous day's propane cost
int GasCuFtYesterday =          0;    // At midnight figure out the previous day's cubic feet
float PropanePrice =         1.63;    // Propane cost per gallon
byte pulseState_now =         LOW;    // current state of the gas meter pulse: HIGH or LOW
byte pulseState_last =        LOW;    // previous state of the gas meter pulse
byte ProtoShldBtnState_now =  LOW;    // current state of button on ProtoWireSheild connected to A5
byte ProtoShldBtnState_last = LOW;    // current state of button on ProtoWireSheild connected to A5


// I/O Pins
#define GasMeterPulsePin      2   // digital pin 2 input from pulse on gas meter, interrupt int.0
#define GasMeterLEDPin        3   // digital pin 3 to light LED when pulse is active
#define ledHeartbeatPin       4   // LED flashes quickly twice to indicate programming is not frozen
#define OLED_RESET            6   // Reset Pin on OLED Display
#define ResetEthernetPin      8   // Reset Ethernet shield
#define ProtoWireShieldBtn    5   // Analog Input connected to button on protoshield - decreases pulse meter count


// Define Cosm datastreem IDs.  cosm.h requires this to ba a string
#define THERM_H               "0"     // Therms/hour based on 15 minute intervals.  1 Therm = 100,000 BTU
#define PULSE_METER_COUNT     "1"     // Pulse meter counter
#define YESTERDAY_GAS_CUFT    "2"     // Yesterdays propane usage (in cu-ft)
#define YESTERDAY_GAS_COST    "3"     // Yesterdays propane cost (dollars)
#define CONNECT_SUCCESSES    "10"     // http connection successes
#define CONNECT_FAILURES     "11"     // http connection successes
#define GAS_PRICE_FEED       "12"     // Gas price, this is put in manually into COSM when you get a bill.  Sketch will read value for cost calculation
#define NUM_STREAMS            6      // Number of cosm streams


// setup Ethernet and COSM
byte mac[] = { 0x90, 0xA2, 0xDA, 0xEF, 0xFE, 0x82 }; // Assign MAC Address to ethernet shield
#ifdef CRESTVIEW
  byte ip[] = { 192, 168, 216, 53 };  // Crestview IP
#else
  byte ip[] = { 192, 168, 46, 94 };   // Vermont IP
#endif
EthernetClient client;
CosmClient cosmclient(client);

// Setup COSM data stream array
CosmDatastream gasMeterStreams[] =
{
  CosmDatastream(THERM_H,            strlen(THERM_H),            DATASTREAM_FLOAT),
  CosmDatastream(PULSE_METER_COUNT,  strlen(PULSE_METER_COUNT),  DATASTREAM_FLOAT),
  CosmDatastream(YESTERDAY_GAS_CUFT, strlen(YESTERDAY_GAS_CUFT), DATASTREAM_FLOAT),
  CosmDatastream(YESTERDAY_GAS_COST, strlen(YESTERDAY_GAS_COST), DATASTREAM_FLOAT),
  CosmDatastream(CONNECT_SUCCESSES,  strlen(CONNECT_SUCCESSES),  DATASTREAM_INT),
  CosmDatastream(CONNECT_FAILURES,   strlen(CONNECT_FAILURES),   DATASTREAM_INT)
};

CosmDatastream gasCostStream[] =
{
  CosmDatastream(GAS_PRICE_FEED, strlen(GAS_PRICE_FEED), DATASTREAM_FLOAT),
};

// Wrap the datastreams into a feed
#ifdef CRESTVIEW
  #define COSM_FEED_ID  4663   // Test feed http://cosm.com/feeds/4663
#else
  #define COSM_FEED_ID   4038  // Regular cosm feed ID, http://cosm.com/feeds/4038
#endif
CosmFeed GasMeterFeed(COSM_FEED_ID, gasMeterStreams, NUM_STREAMS);  // Uploads data to COSM
CosmFeed GasCostFeed(COSM_FEED_ID,  gasCostStream, 1);              // Download gas cost from COST


// Setup OLED Display
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 32)
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif  // SSD1306_LCDHEIGHT
char lcdBuff[22];        // character buffer to send text to LCD display

uint32_t next_connect;   // Timer for uploading to COSM

//Declare function prototypes (Reference: http://arduino.cc/forum/index.php/topic,87155.0.html)
void GetMeterReadingFromEEPROM(uint32_t InitializeMeterReading);
void reset_ethernet_shield(int ResettPin);
void ReadPulse();
void ReadDecrementPulseBtn();
void calcThermUsage();
void calcYesterdayStats();
time_t getNtpTime();
int freeRam(bool PrintRam);
void software_Reset(void);
float GetGasPrice();
void sendNTPpacket(IPAddress &address);
void setupOled();
void dispText(char textToDisplay[], byte line, bool clearDisp);


// ==============================================================================================================================================
//   Setup
// ==============================================================================================================================================
void setup()
{
  
  Serial.begin(9600);  // Need if using serial monitor
  Serial.println(F("Gas Meter v3 Setup"));
  delay(500);
  
  pinMode(GasMeterPulsePin, INPUT);       // Gas meter pulse, define digital as input
  pinMode(GasMeterLEDPin,   OUTPUT);        // LED that lights when gas meter pulse is on
  pinMode(ledHeartbeatPin,  OUTPUT);       // LED flashed every couple seconds to show sketch is running
  pinMode(ResetEthernetPin, OUTPUT);      // Resets ethernet shield
  
#ifdef CRESTVIEW
  digitalWrite(GasMeterPulsePin, HIGH); // turn on pullup resistors
#endif
  
#ifdef PRER3ETHERNETSHLD
  // Use Digital I/O to hard reset Ethernet shield
  reset_ethernet_shield(ResetEthernetPin);
#endif
  
  Ethernet.begin(mac, ip);
  
  unsigned int localPort = 8888;    // local port to listen for UDP packets
  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);
  
  // Set the time for the alarm functions
  time_t t = getNtpTime();
  setTime(hour(t),minute(t),second(t),month(t),day(t),year(t));
  
  char timebuf[16];
  sprintf(timebuf, "Time: %02d:%02d:%02d", hour(),minute(),second());
  Serial.println(timebuf);
  
  // create the time events
   Alarm.alarmRepeat(0,0,0, calcYesterdayStats);  // Midnight every day
   Alarm.timerRepeat(15*60, calcThermUsage);      // Every 15 minutes
  
  // Get meter reading from EEPROM, if you need sketch to use a new (higher) number, pass that to the function.
  // Function will compare EEPROM to number passed to it and use the higher one
  GetMeterReadingFromEEPROM(92458);
  
  setupOled();  // Initialize OLED display
  
  attachInterrupt(0, ReadPulse, FALLING);  // http://arduino.cc/en/Reference/AttachInterrupt
  dispText("End setup ", 2, true);
  sprintf(lcdBuff, "Free RAM = &d", freeRam(false));
  dispText(lcdBuff, 3, false);  // Printout free ram available
  
} // End Setup()


// ==============================================================================================================================================
//   Main Loop
// ==============================================================================================================================================
void loop()
{
   Alarm.delay(0); //  Alarm.delay must be used instead of the usual arduino delay function
                  // because the alarms are serviced in the Alarm.delay method.
  
  // Turn on LED when pulse is on
  digitalWrite(GasMeterLEDPin, digitalRead(GasMeterPulsePin));
  
  ReadDecrementPulseBtn(); // read pulse from decriment pushbutton on shield
  
  //--------------------------------
  // Send data to Cosm
  //--------------------------------
  if ((long)(millis() - next_connect) >= 0 )
  {
    next_connect = millis() + UPDATE_INTERVAL;
    Serial.println(F("Uploading to Cosm..."));
    
    gasMeterStreams[0].setFloat(therms);
    gasMeterStreams[1].setFloat(MeterReading);
    gasMeterStreams[2].setFloat(GasCuFtYesterday);
    gasMeterStreams[3].setFloat(yesterdayGasCost);
    gasMeterStreams[4].setInt(successes);
    gasMeterStreams[5].setInt(failures);
    
    int status = cosmclient.put(GasMeterFeed, COSM_API_KEY);
    switch (status)
    {
      case 200:
        Serial.print(F("Upload succeeded, Success="));
        Serial.println(successes++);
        failures = 0; // Reset failures
        break;
      default:
        Serial.print(F("Upload failed, Error: = "));
        Serial.println(status);
        failures++;
    }
    
  } // end upload to cosm
  
  // if connection failures reach 10, reboot
  if(failures >= 10)
  {
    Serial.println(F("Over 10 Errors, software reset"));
    software_Reset();
  }
  
} // End loop()

// ==============================================================================================================================================
// Called by alarm library at midnight
// ==============================================================================================================================================
void calcYesterdayStats()
{
  Serial.println(F("New Day started"));
  
  PropanePrice = GetGasPrice();  // Get new gas price
  
  // Calculate gas usage and cost for previous day
  GasCuFtYesterday = MeterReading - MeterStartDay;
  MeterStartDay = MeterReading;
  if(GasCuFtYesterday > 1000 || GasCuFtYesterday < 0) {GasCuFtYesterday = 0;}
  yesterdayGasCost = GasCuFtYesterday * 0.02549 * PropanePrice;
  
}  // calcYesterdayStats()

// ==============================================================================================================================================
// Read cosm gas price stream.  This stream is updated manually when bills come in
// This function takes about a minute to process
// ==============================================================================================================================================
float GetGasPrice()
{
  float oldGasPrice = PropanePrice;
  int status = cosmclient.get(GasCostFeed, COSM_API_KEY);
  if (status == 200 )
  { return GasCostFeed[0].getFloat(); }
  else
  { return oldGasPrice; }
  
}  // end GetGasPrice()

// ==============================================================================================================================================
// Read gas meter pulses stored in EEPROM
// ==============================================================================================================================================
void GetMeterReadingFromEEPROM(uint32_t InitializeMeterReading)
{
  // Meter pulses stored in EEPROM memory
  uint32_t StoredMeterReading;
  
  // Read last gas meter value from EEPROM.  If unit loses power, this value is used as starting point
  StoredMeterReading = eeprom_read_dword((const uint32_t *)EEPROM_ADDR_GASPULSE);
  
  // If InitializeMeterReading is greater then what's stored in EEPROM, it means a
  // new value was loaded into EEPROM and the new value should be used
  if (InitializeMeterReading > StoredMeterReading)
  {
    eeprom_write_dword((uint32_t *)EEPROM_ADDR_GASPULSE, InitializeMeterReading);
    StoredMeterReading = InitializeMeterReading;
  }
  
  MeterStartDay = StoredMeterReading;
  MeterReading =  StoredMeterReading;
  Meter15MinAgo = StoredMeterReading;
  
}  // End GetMeterReadingFromEEPROM()

// ==============================================================================================================================================
//    Read the Gas Meter Pulse input pin
// ==============================================================================================================================================
void ReadPulse()
{
  pulseState_now = digitalRead(GasMeterPulsePin);
  
  // Compare the pulseState to its previous state
  if ( pulseState_now != pulseState_last )
  {
    // if the state has changed, check input one more time, then increment the counter
    if ( pulseState_now == HIGH )
    {
      delay(100);
      pulseState_now = digitalRead(GasMeterPulsePin); // Check input again after 100 mS debounce delay.  If it's still high then increment conter
      if (pulseState_now == HIGH)
      {
        // if the current state is still HIGH then the pulse went from off to on:
        MeterReading++;
        eeprom_write_dword((uint32_t *)EEPROM_ADDR_GASPULSE, MeterReading); // Save reading to EEPRROM
      }
    }
    pulseState_last = pulseState_now;  // save the current state as the last state, for next time through the loop
  } // End pulse state changed
  
  if( MeterReading  < 0 )
  { MeterReading  = 0; }
  
} // End ReadPulse()

// ==============================================================================================================================================
//    Read ProtoWireShield button on A5, if it's zero, decrement pulse counter
// ==============================================================================================================================================
void ReadDecrementPulseBtn()
{
  // Check for button on ProtoWireShield - when pressed decrease pulse counter by one
  if (analogRead(ProtoWireShieldBtn) == 0)
  { ProtoShldBtnState_now = HIGH; }
  else
  { ProtoShldBtnState_now = LOW; }
  
  // Compare the current button state to its previous state
  if (ProtoShldBtnState_now != ProtoShldBtnState_last)
  {
    // if the state has changed, decrement the counter
    if (ProtoShldBtnState_now == HIGH)
    { // if the current state is HIGH then the button went from off to on:
      MeterReading--;
      eeprom_write_dword((uint32_t *)EEPROM_ADDR_GASPULSE, MeterReading); // Save reading to EEPRROM
    }
    ProtoShldBtnState_last = ProtoShldBtnState_now;  // save the current state as the last state, for next time through the loop
  } // End protowireshield button pulse state changed
  
  if(MeterReading  < 0)
  { MeterReading  = 0; }
  
}  // end ReadDecrementPulseBtn()



// ==============================================================================================================================================
//  Calculate therms used in the last 15 minutes
// ==============================================================================================================================================
void calcThermUsage()
{
  //------------------------------------
  // Check for new 15 minute interval
  // Calculate BHU/hour and avg temperatures
  //------------------------------------
  //  static uint32_t FifteenMinTimer;
  
  //  if (long(millis() - FifteenMinTimer) >=0)
  //  {
  //    FifteenMinTimer = millis() + 900000;  // add 15 minutes to timer
  therms = (MeterReading - Meter15MinAgo) * 2328.0 * 4.0 / 100000.0;   // convert cu-ft used to BTU (2328) then multiply by 4 to convert from 15 minutes to hours
  
  if(therms >  100 || therms < 0) {therms = 0;}
  
  Serial.println(F("--- New 15 Minute Interval ---"));
  Serial.print(F("Cu-Ft Used in last 15 minutes = "));
  Serial.println(MeterReading - Meter15MinAgo);
  Serial.print(F("Therms/Hour = "));
  Serial.println(therms);
  
  Meter15MinAgo = MeterReading;
  //  }
  
} // End calcThermUsage()


// ==============================================================================================================================================
// ==============================================================================================================================================
time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0; // return 0 if unable to get the time
}  // end getNtpTime()

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
                           // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
} // end sendNTPpacket()


void setupOled()
{
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();   // clears the screen and buffer
} // end setupOled()


// Display text on LCD
void dispText(char textToDisplay[], byte line, bool clearDisp)
{
  if (clearDisp)
  { display.clearDisplay(); }
  
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(line,0);
  display.println(textToDisplay);
  display.display();
  
} // end dispText()


// ==============================================================================================================================================
// The ethernet shield can be reset with one of the digital I/O pins
// ==============================================================================================================================================
void reset_ethernet_shield(int ResettPin)
{
  // Connect Ethernet shield reset to Pin 8 on arduino, code below will reset
  Serial.println(F("Reset ethernet shield with output pin"));
  delay(500);
  digitalWrite(ResettPin, HIGH);
  delay(50);
  digitalWrite(ResettPin, LOW);  // This is the reset
  delay(50);
  digitalWrite(ResettPin, HIGH);
  delay(100);
  
}  // End reset_ethernet_shield()


//==========================================================================================================================
// Restarts program from beginning but does not reset the peripherals and registers
// Reference: http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1241733710
// Reboot the arduino
//==========================================================================================================================
void software_Reset(void)
{
  asm volatile ("  jmp 0");
} // End software_Reset()


//==========================================================================================================================
// Return the amount of free SROM
// Parameters - true: Print out RAM, false: Don't print
// http://www.controllerprojects.com/2011/05/23/determining-sram-usage-on-arduino/
//==========================================================================================================================
int freeRam(bool PrintRam)
{
  int freeSRAM;
  extern int __heap_start, *__brkval;
  int v;
  
  freeSRAM =  (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  
  if(PrintRam)
  {
    Serial.print(F("RAM "));
    Serial.println(freeSRAM);
  }
  return freeSRAM;
  
} // freeRam()





