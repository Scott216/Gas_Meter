This version uses COSM.h library to upload data.
 Version 3
 
 To do:
 - File size is big now with TimeAlarm.h and OLED Display. Sketch won't fit on UNO.  
   TimeAlarm.h library uses about 5600 of memory, use something else instead, maybe just use NTP 
   time like you do with Leak detector
 - You could probably move COSM to a separate Arduino.  If you do, COSM arduino to get the time, 
   then you can get rid of ethernet shield


 === When you create PCB shield ===
 one button to increase, another to decrease
 Status LEDs: heartbeat (grn), pulse in (amber)
 Pin 8 for ethernet shield reset - use jumper so you can disable if using newer board
 Terminal block for I2C, Power, Pulse in
 two 3-position screw terminals incase you want to measure gas temp and pressure in the future.
 Just have some wire pads you can solder with jumpers if you decide to use
 Shield reset button
 Arduino reset button
 Power switch
 Header to plug LCD into
 
 
 Hardware:
 Arduino UNO v1 (Atmege 328)
 LadyAda Etheret shield v1.2 with Wiznet WIZ811MJ Rev 1.0 - Wire reset to pin 8, ref: http://forums.adafruit.com/viewtopic.php?f=19&p=104959
 ProtoScrewShield  http://www.sparkfun.com/products/9729
 1 10K ohm resistors (for inputs)
 2 560 ohm resistors (for LEDs)
 3 PCB Screw terminal block, 2-position Radio Shack Catalog # 276-1388
 2-conductor 22 gauge shielded wire McMaster 70045K84
 
 
 D2 - (I) Pulse meter input
 D3 - (O) Pulse On red LED - turns on when pulse relay is closed
 D4 - (O) Heartbeat LED, green
 D8 - Reset Ethernet Shield
 D10 - Used by ethernet shield
 D11 - Used by ethernet shield
 D12 - Used by ethernet shield
 D13 - Used by ethernet shield
 ADC5 - connected to ProtoScrewShield button and room temp thermistor.  Thermister is wired backwards from other thermistors because the ProtoScrewShield uses ADC5 for a pushbutton
 
 Converting cu-ft propane to gallons and BTU.  Assumes propane is 36 degrees at 10 PSI
 1 cu-ft = 0.02549 gallons = 2328 BTU
 100,070 Therms = 1 BTU
 
 
 COSM Datastreams:
 0 Therms/hour
 1 Pulse meter count
 2 Yesterday cu-ft gas used
 3 Yesterday Gas Cost
 4 Crawlspace Temp - depricated
 5 Basebaord temp  - depricated
 6 Outside Temp - depricated
 7 Heter supply temp - depricated
 8 Temp of resistor for heater sensor  - depricated
 9 Room temp - depricated
 10 Connection Successes
 11 Connection Failures
 
 
 REVISION HISTORY
 =========================
 02/25/10 - changed daily cost from cents to dollars.  Put TempCount & TempSum in correct routine, they were only in the old one
 03/02/10 - Set remoteSensor[0] = 21.6 to see if outside averaging works properly
 07/10/10 - change Pachube IP to new server: 173.203.98.29
 07/16/10 - save gas meter pulse to EEPROM.  Changed pulse variables from int to long
 01/16/11 - changed MeterReading from int to unsigned int
 02/06/11 - added InitializeMeterReading so if a new meter reading had to be set, sketch only has to be sent to pachube once
 02/20/11 - Renamed localclient to PachubeClient()
 11/27/11 - added watchdog timer (but it's commented out until I can verify bootload supports it).  Ref: http://tushev.org/articles/electronics/48-arduino-and-watchdog-timer
            There may be additional work to do to get watchdog timer to work, see this post: http://community.pachube.com/arduino/ethernet/watchdog
 11/27/11 - Swapped Duamelinavo for Uno because Macbook was getting errors trying to upload sketch
 12/04/11 - Updated code to work with Nanode.  Added #IF to set is code should be compiled for Nanode or Arduino
 12/12/11 - changed update interval from 10 seconds to 15 seconds.  Added code to calculate BTUH and feed to datastream 0,  Added BlinkLED function
 12/13/11 - Major rewrite
 12/14/11 - Changed Meter reading to long int
 12/17/11 - Uploaded new program to Arduino in Vermont
 12/19/11 - Took out all Pachube GET code.  I'll get outside temp from thermistor
 12/21/11 - Added 3 new temp sensors: Basebaord, Resistor, air supply, removed WDC code (even though it was commented out).  Removed avarage temps
 12/31/11 - Added code for A5 butotn on ProtoWireShield, when pressed it will decrase counter
 01/09/12 - added Pin 8 reset for Ethernet shield Ref: http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1258355934.  Alternatively, you can install R/C circuit ref: http://marco.guardigli.it/2010/11/arduino-wiznet-ethernet-shield-proper.html?m=1
 01/10/12 - Added code for to check if connection is okay, if not, change blinking of green LED.  Added code so ADC 05, which is connected to WireShield button, can also be used with a thermisor
 01/15/12 - Added restor ProtoShieldBoard
 01/21/12 - Added watchdog timer code, but it wasn't working properly, so I put it around #if
 01/22/12 - Added code to clear buffer after connection failure.  I saw this suggestion on the internet.
 01/31/12 - Added debounce code on pulse input.  Validated data before sent to pachube to make sure it's doesn't send unrealistic values
 03/04/12 - Added conditional #if for Arduino 100.  Reference http://blog.makezine.com/2011/12/01/arduino-1-0-is-out-heres-what-you-need-to-know/
 06/02/12 - Feed is freezing after about 2 hours.  Use ERxPachube to upload
            Moved a lot of code into specific functions.  Took out GMT Time code, not supported by ERxPachube library
 09/20/12 - Made success counter a byte so it rolls over at 255.  Failures reset when there is a successful upload. Put Serial.print() strings in flash wtih F()
            used dtostrf() to round first two streams to 1 decimal.  Added FreeRam function and software reset function. Instead of resetting Ethernet shield
            after 50 upload failures, code reboots after 10 failures.
 04/12/13 - Added token.h, replaced erxpachube.h with cosm.h, removed temperature sensors, Removed WDT
 04/13/13 - Sketch reads gas price from cosm stream.  Use interrupt for pulse input, use time libraries to get NTP time and set alarms for midnight stat calculation and 15 minute therm calculation
 04/15/13 - Added OLED display, uses I2c
 
