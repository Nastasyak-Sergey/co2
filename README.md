
# co2
STM32 CO2 tester

Pet projekt based on stm32f103 board and SenseAir S8 CO2 sensor. 
- CO2 sensor connected to UART1;
- 0.9 inch OLED display (SSD1306) conneted to I2C1;

What already realised:
- UART FIFOs driven by interrupt;
- I2C OLED dispaly with multiple fonts;
- pyQt5 GUI (main function of communication and display realized); 
- CO2 sensor status indication;

What planed to do;
- draw trends of CO2  on display;
- add menu of settings and display information options;
- comunication throug USB CDC (libopencm3) insteade  of debug msg throug openocd;
- add logging of co2 msg in microcontroller flash (+wear leveling); 
- add new functions to pyQt5 GUI;

GUI

![alt text](https://github.com/Nastasyak-Sergey/co2/blob/master/pyQt5_Gui.png?raw=true)

Assembled PCB

![alt text](https://github.com/Nastasyak-Sergey/co2/blob/master/IMG_0822%2B.jpg?raw=true)
