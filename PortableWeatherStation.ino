 /*****************************************************************************
    Arduino Portable Weather Station V1.0
    Display pressure, temperature, humidity on a 0.96" OLED display.
    Show forecast for the next hours (Zambretti algorithm).
    Designed to work with an Arduino Nano (Uno).
    Because of the huge amount of memory used (for Arduino Uno, Nano, etc.),
    the serial output is commented. To debug, uncomment the desired serial
    output strings and comment the very last section of the code, where symbols
    are drawn on the display. This will free up both flash and SRAM memories,
    leaving you with enough space to check variables on the serial monitor.

    Copyright (C) 2019 Danilo Ciliberti dancili@gmail.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>
 ******************************************************************************/

// Load libraries
#include <Arduino.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <SFE_BMP180.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Create the display object using the I2C protocol
// u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Define some useful constants
#define SUN 0
#define SUN_CLOUD 1
#define CLOUD 2
#define RAIN 3
#define THUNDER 4
#define HUMIDITY 5
#define HUMIDEX 6

// Create the DHT object (which reads temperature and humidity)
DHT dhtSensor(2, DHT22);

// Create the BMP180 object (which reads temperature and pressure)
SFE_BMP180 bmpSensor;
#define ALTITUDE 125.0 // My altitude in meters

// Variables declaration
char status;
double temperature, pressure, humidity, humidex;
unsigned long interval = 1000UL*60; // the time we need to wait to calculate time derivative of pressure
unsigned long previousMillis;       // millis() returns an unsigned
byte pressureDerivative;            // time derivative of the pressure
unsigned int previousPressure;      // pressure used to calculate its time derivative


// Define functions to draw weather graphics
void drawWeatherSymbol(u8g2_uint_t x, u8g2_uint_t y, uint8_t symbol)
{
  // fonts used:
  // u8g2_font_open_iconic_embedded_6x_t
  // u8g2_font_open_iconic_weather_6x_t
  // u8g2_font_open_iconic_thing_6x_t
  // encoding values, see: https://github.com/olikraus/u8g2/wiki/fntgrpiconic
  
  switch(symbol)
  {
    case SUN:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 69);  
      break;
    case SUN_CLOUD:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 65); 
      break;
    case CLOUD:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 64); 
      break;
    case RAIN:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 67); 
      break;
    case THUNDER:
      u8g2.setFont(u8g2_font_open_iconic_embedded_6x_t);
      u8g2.drawGlyph(x, y, 67);
      break;
    case HUMIDITY:
      u8g2.setFont(u8g2_font_open_iconic_thing_6x_t);
      u8g2.drawGlyph(x, y, 72);
      break;
    case HUMIDEX:
      u8g2.setFont(u8g2_font_open_iconic_thing_6x_t);
      u8g2.drawGlyph(x, y, 78);
      break;
  }
}


void drawWeather(uint8_t symbol, int degree)
{
  drawWeatherSymbol(0, 48, symbol);
  u8g2.setFont(u8g2_font_logisoso32_tf);
  u8g2.setCursor(48+3, 42);
  u8g2.print(degree);
  u8g2.print("°C");   // requires enableUTF8Print()
}

void drawHumidity(uint8_t symbol, int percentage)
{
  drawWeatherSymbol(0, 48, symbol);
  u8g2.setFont(u8g2_font_logisoso32_tf);
  u8g2.setCursor(48+3, 42);
  u8g2.print(percentage);
  u8g2.print("%");   // requires enableUTF8Print()
}


/*
  Draw a string with specified pixel offset. 
  The offset can be negative.
  Limitation: The monochrome font with 8 pixel per glyph
*/
void drawScrollString(int16_t offset, const char *s)
{
  static char buf[36];  // should for screen with up to 256 pixel width 
  size_t len;
  size_t char_offset = 0;
  u8g2_uint_t dx = 0;
  size_t visible = 0;
  len = strlen(s);
  if ( offset < 0 )
  {
    char_offset = (-offset)/8;
    dx = offset + char_offset*8;
    if ( char_offset >= u8g2.getDisplayWidth()/8 )
      return;
    visible = u8g2.getDisplayWidth()/8-char_offset+1;
    strncpy(buf, s, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_9x18B_mf);
    u8g2.drawStr(char_offset*8-dx, 62, buf);
  }
  else
  {
    char_offset = offset / 8;
    if ( char_offset >= len )
      return; // nothing visible
    dx = offset - char_offset*8;
    visible = len - char_offset;
    if ( visible > u8g2.getDisplayWidth()/8+1 )
      visible = u8g2.getDisplayWidth()/8+1;
    strncpy(buf, s+char_offset, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_9x18B_mf);
    u8g2.drawStr(-dx, 62, buf);
  }
  
}


void draw(const char *s, uint8_t symbol, int degree)
{
  int16_t offset = -(int16_t)u8g2.getDisplayWidth();
  int16_t len = strlen(s);
  for(;;)
  {
    u8g2.firstPage();
    do {
      drawWeather(symbol, degree);
      drawScrollString(offset, s);
    } while ( u8g2.nextPage() );
    delay(20);
    offset+=5;
    if ( offset > len*8+1 )
      break;
  }
}


void drawHum(const char *s, uint8_t symbol, int percentage)
{
  int16_t offset = -(int16_t)u8g2.getDisplayWidth();
  int16_t len = strlen(s);
  for(;;)
  {
    u8g2.firstPage();
    do {
      drawHumidity(symbol, percentage);
      drawScrollString(offset, s);
    } while ( u8g2.nextPage() );
    delay(20);
    offset+=5;
    if ( offset > len*8+1 )
      break;
  }
}


byte zambrettiForecast(unsigned int pressure, byte pressureDerivative){
  
//  fall    Z = 130-10P/81  
//  steady  Z = 147-50P/376 
//  rise    Z = 179-20P/129 
  
  byte result;

  switch(pressureDerivative)
  {
    case 0:   // pressure falling
      result = 130 - 10*pressure/81;
      break;
    case 1:   // pressure steady
      result = 147 - 50*pressure/376;
      break;
    case 2:   // pressure rising
      result = 179 - 20*pressure/129 ;
      break;
  }

  result = constrain(result, 1, 32);
  return result;
}


// The program core
void setup() {
  
//  Serial.begin(9600);

  // Initialize the temperature sensor
  dhtSensor.begin();

  // Initialize the bmpSensor sensor
  bmpSensor.begin();
//  Serial.println(F("REBOOT"));
//  if (bmpSensor.begin()){
//    Serial.println(F("BMP180 init success"));
//  }
//  else
//  {
//    // Oops, something went wrong, this is usually a connection problem,
//    // see the comments at the top of this sketch for the proper connections.
//    Serial.println(F("BMP180 init fail\n\n"));
//    while(1); // Pause forever
//  }

  u8g2.begin();  
  u8g2.enableUTF8Print();

}

void loop() {

  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.
  status = bmpSensor.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    status = bmpSensor.getTemperature(temperature);
    if (status != 0)
    {
      // Print out the measurement:
//      Serial.print("temperature: ");
//      Serial.print(temperature,2);
//      Serial.print(" deg C, ");
//      Serial.print((9.0/5.0)*temperature+32.0,2);
//      Serial.println(" deg F");
      
      // Start a bmpSensor measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.

      status = bmpSensor.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed bmpSensor measurement:
        // Note that the measurement is stored in the variable P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of bmpSensor measurements.)
        // Function returns 1 if successful, 0 if failure.

        status = bmpSensor.getPressure(pressure,temperature);
        if (status != 0)
        {
//           Print out the measurement:
//          Serial.print("absolute pressure: ");
//          Serial.print(pressure,2);
//          Serial.print(" mb, ");
//          Serial.print(pressure*0.0295333727,2);
//          Serial.println(" inHg");

          // The bmpSensor sensor returns abolute bmpSensor, which varies with altitude.
          // To remove the effects of altitude, use the sealevel function and your current altitude.
          // This number is commonly used in weather reports.
          // Parameters: P = absolute bmpSensor in mb, ALTITUDE = current altitude in m.
          // Result: P = sea-level compensated bmpSensor in mb

          pressure = bmpSensor.sealevel(pressure,ALTITUDE); // we're at 50 meters (San Paolo Bel Sito, NA)
//          Serial.print("relative (sea-level) pressure: ");
//          Serial.print(pressure,2);
//          Serial.print(" mb, ");
//          Serial.print(pressure*0.0295333727,2);
//          Serial.println(" inHg");

          // On the other hand, if you want to determine your altitude from the bmpSensor reading,
          // use the altitude function along with a baseline bmpSensor (sea-level or other).
          // Parameters: P = absolute bmpSensor in mb, P = baseline bmpSensor in mb.
          // Result: a = altitude in m.

//          a = bmpSensor.altitude(pressure,pressure);
//          Serial.print("computed altitude: ");
//          Serial.print(a,0);
//          Serial.print(" meters, ");
//          Serial.print(a*3.28084,0);
//          Serial.println(" feet");
        }
//        else Serial.println(F("error retrieving bmpSensor measurement\n"));
      }
//      else Serial.println(F("error starting bmpSensor measurement\n"));
    }
//    else Serial.println(F("error retrieving temperature measurement\n"));
  }
//  else Serial.println(F("error starting temperature measurement\n"));

  // Read humidity and temperature
  humidity = dhtSensor.readHumidity();
//  Serial.print(F("Humidity: "));
//  Serial.print(humidity,1);
//  Serial.print(F("%\t"));

  temperature = dhtSensor.readTemperature();
//  Serial.print(F("Temperature: "));
//  Serial.print(temperature,1);
//  Serial.print(F("°C\t"));

  // Calculates humidex index and display its status
  humidex = temperature + (0.5555 * (0.06 * humidity * pow(10,0.03*temperature) -10));
//  Serial.print(F("Humidex: "));
//  Serial.print(humidex);
//  Serial.print(F("\t"));

  // Humidex category
//  if (humidex < 20){
//    Serial.println(F("No index"));
//  }
//  else if (humidex >= 20 && humidex < 27){
//    Serial.println(F("Comfort"));
//  }
//  else if (humidex >= 27 && humidex < 30){
//    Serial.println(F("Caution"));
//  }
//  else if (humidex >= 30 && humidex < 40){
//    Serial.println(F("Extreme caution"));
//  }
//  else if (humidex >= 40 && humidex < 55){
//    Serial.println(F("Danger"));
//  }
//  else {
//    Serial.println(F("Extreme danger"));
//  }

  // Calculates pressure time derivative
  unsigned long currentMillis = millis(); // grab current time
 
// Check if "interval" time has passed (in milliseconds)
if ((unsigned long)(currentMillis - previousMillis) >= interval) {

  // Trick to initialize pressure derivative calculation 
  if (previousPressure == 0){
    previousPressure = pressure;
//    Serial.println("Start time, previous pressure initialization = pressure");
  }

    // We are only interested at the sign of the derivative
    int pressureDerFloat = (pressure - previousPressure)/interval;
    if (pressureDerFloat < 0){        // pressure falling (during "interval")
      pressureDerivative = 0;
    }
    else if (pressureDerFloat == 0){  // pressure steady (during "interval")
      pressureDerivative = 1;
    }
    else{                             // pressure rising (during "interval")
      pressureDerivative = 2;
    }
   
   // update the "current" time and pressure
   previousMillis = millis();
   previousPressure = pressure;

//   Serial.print(F("previous pressure = "));
//   Serial.println(previousPressure);
//   Serial.print(F("pressure = "));
//   Serial.print(pressure);
//   Serial.print(F(" at time "));
//   Serial.println(millis());
//   Serial.print(F("pressure derivative = "));
//   Serial.println(pressureDerFloat);
//   Serial.print(F("pressure derivative flag (0, 1, 2) = "));
//   Serial.println(pressureDerivative);
 }

  // Weather forecast (the very first reading will be wrong, because we have no info on pressure derivative at time zero)
  byte Z = zambrettiForecast(pressure, pressureDerivative);
//  Serial.print(F("Z = "));
//  Serial.println(Z);

  // Uncomment for debug. Remember to comment the next section to free up flash and SRAM memories.
//  delay(10000);

  // Display values on screen
  drawHum("Umidita relativa", HUMIDITY, humidity);
  draw("Temperatura percepita", HUMIDEX, humidex);

  switch(Z)   // Show weather forecast
  {
    case 1:
      draw("Bel tempo stabile", SUN, temperature);
      break;
    case 2:
      draw("Bel tempo", SUN, temperature);
      break;
    case 3:
      draw("Bel tempo, diventa instabile", SUN_CLOUD, temperature);
      break;
    case 4:
      draw("Abbastanza bello, possibili piogge piu tardi", SUN_CLOUD, temperature);
      break;
    case 5:
      draw("Piovoso, diventa instabile", RAIN, temperature);
      break;
    case 6:
      draw("Instabile, piogge piu tardi", CLOUD, temperature);
      break;
    case 7:
      draw("Piogge occasionali, in peggioramento", RAIN, temperature);
      break;
    case 8:
      draw("Piogge di tanto in tanto, molto instabile", RAIN, temperature);
      break;
    case 9:
      draw("Piogge, molto instabile", RAIN, temperature);
      break;
    case 10:
      draw("Bel tempo stabile", SUN, temperature);
      break;
    case 11:
      draw("Bel tempo", SUN, temperature);
      break;
    case 12:
      draw("Bel tempo, possibili piogge", SUN, temperature);
      break;
    case 13:
      draw("Abbastanza bello, probabilmente piovoso", SUN_CLOUD, temperature);
      break;
    case 14:
      draw("Piovoso ad intervalli", RAIN, temperature);
      break;
    case 15:
      draw("Variabile, alcune piogge", CLOUD, temperature);
      break;
    case 16:
      draw("Instabile, alcune piogge", RAIN, temperature);
      break;
    case 17:
      draw("Piogge frequenti", RAIN, temperature);
      break;
    case 18:
      draw("Piogge, molto instabile", RAIN, temperature);
      break;
    case 19:
      draw("Tempesta, acqua a sassate", THUNDER, temperature);
      break;
    case 20:
      draw("Bel tempo stabile", SUN, temperature);
      break;
    case 21:
      draw("Bel tempo", SUN, temperature);
      break;
    case 22:
      draw("Arriva il bel tempo", SUN, temperature);
      break;
    case 23:
      draw("Abbastanza bello, in miglioramento", SUN_CLOUD, temperature);
      break;
    case 24:
      draw("Abbastanza bello, possibili piogge presto", SUN_CLOUD, temperature);
      break;
    case 25:
      draw("Piovoso presto, in miglioramento", RAIN, temperature);
      break;
    case 26:
      draw("Variabile, ma si aggiusta", CLOUD, temperature);
      break;
    case 27:
      draw("Piuttosto instabile, al bello più tardi", CLOUD, temperature);
      break;
    case 29:
      draw("Instabile, brevi intervalli di bel tempo", SUN_CLOUD, temperature);
      break;
    case 30:
      draw("Molto instabile", CLOUD, temperature);
      break;
    case 31:
      draw("Tempesta, possibile miglioramento", THUNDER, temperature);
      break;
    case 32:
      draw("Tempesta, acqua a sassate", THUNDER, temperature);
      break;
  }

}
