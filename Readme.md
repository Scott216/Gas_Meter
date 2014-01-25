Vermont Gas Meter

Arduino monitors pulse from propane gas meter does some calculations and sends to cosm.com
1 pulse = 1 cu-ft propane

Added code for Adafruit 128x32 OLED display: http://www.adafruit.com/products/931
Arduino will use IC2 to communicate with display.
WIth display library, code will not fit on an Uno, I'll need to use a mega.
If cosm upload is eventually moved to a separate Arduino, then code will easily fit on Uno 
