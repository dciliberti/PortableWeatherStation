# PortableWeatherStation
A portable weather station made with Arduino Nano. Collects temperature, humidity and pressure data. A DHT22 sensor acquires data about room relative humidity and temperature. A BMP180 sensor acquires pressure data. It uses the Zambretti algorithm to forecast the weather for the next hours. Display the values on a 0.96" OLED screen.

The Zambretti algorithm is described in the [docs folder](docs/ZambrettiAlgorithm.pdf). Actually, weather forecast are displayed in italian.

The physical realization is the same of [this repository]( https://github.com/dciliberti/TempHumPressOLED) (and indeed the schematics names are the same), with the addition of a prototype board scheme.

This is the second step for the development of a more advanced weather station.