## RTOS Clock

### Video Demonstration (Enable Audio)

https://user-images.githubusercontent.com/43423042/201429903-88f6c346-d8f4-4890-ba8b-6f89f45c6b78.mp4

### Project Description
RTOS Clock is a digital clock program written in C++ using the Arduino IDE and ESP-IDF FreeRTOS, which is a variant of FreeRTOS that supports multicore programming. It is meant to be uploaded to an ESP32 Feather microcontroller, which then forms a circuit that includes physical controls for the clock, an LCD screen, a buzzer, and an LED light. 

I completed RTOS Clock as a personal programming project in order to gain an understanding of software development for real-time operating systems. In the process, I also learned more about building electric circuits.

### Physical Components List
* 1x ESP32 Feather microcontoller
* 1x 16×2 I2C LCD
* 2x Full-sized breadboards
* 1x Active buzzer
* 1x Red LED
* 5x Pushbutton switches
* 1x SPDT slide switch
* 6x 10 kΩ resistors
* 1x 220 Ω resistor
* 22 AWG wires

### Installation Guide
1. Set up the circuit as shown below:

![RTOS-Clock](https://user-images.githubusercontent.com/43423042/201443218-0331ecb6-0428-4ce4-91ca-a86bc924f335.jpg)

2. Download and install the [Arduin IDE](https://www.arduino.cc/en/software) version 1.8.19.
3. Download the [LiquidCrystal_I2C library](https://github.com/marcoschwartz/LiquidCrystal_I2C/archive/master.zip) zip folder.
4. Unzip the LiquidCrystal_I2C-master .zip folder.
5. Rename the LiquidCrystal_I2C-master folder to LiquidCrystal_I2C.
6. Move the LiquidCrystal_I2C folder to your Arduino IDE installation "libraries" folder.
7. Clone this GitHub repository into your Arduino sketchbook folder.
8. In the cloned RTOSClock folder, open RTOSClock.ino using the Arduino IDE.
9. Under File->Preferences->Additional Boards Manager URLs, enter the following URL: https://dl.espressif.com/dl/package_esp32_index.json
10. Under Tools->Board->Board Manager, type "esp32" into the search bar and install the latest version of the esp32 package.
11. Under Tools->Board->ESP32 Arduino, select "Adafruit ESP32 Feather".
12. Connect your computer to the Micro-USB port on the ESP32 Feather.
13. Under Tools->Port, select the port that is connected to the ESP32 Feather (the name shown for the port will vary by system).
14. Click the upload button.

### Instructions
The pushbuttons are designated, from left to right: time, hour, min, alarm, snooze. To adjust the time, hold down the time button and press the hour and/or min buttons. To adjust the alarm time, hold down the alarm button and press the hour and/or min buttons. The alarm is enabled when the slide switch is in the left position, and disabled when the slide switch is in the right position. Once the alarm is triggered, pressing the snooze button will temporarily disable the alarm for 5 minutes. Switching the slide switch to the right position will disable the alarm indefinitely.

### License
All code for the content repository is licensed under [MIT](https://github.com/Exojets/RTOSClock/blob/main/LICENSE.txt).
