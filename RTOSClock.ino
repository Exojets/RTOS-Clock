// Author: Ryan Kelly

#include "RTOSClock.h" // Function prototypes
#include <LiquidCrystal_I2C.h> // Download at https://github.com/johnrickman/LiquidCrystal_I2C, credit to John Rickman

// GPIO Pins
static const int timePin = 12, hourPin = 27, minutePin = 33, alarmPin = 15, snoozePin = 32, switchPin = 14, buzzerPin = 4, lightPin = 5;

// Globals
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD screen at address 0x27 with 16 columns and 2 rows

static const uint16_t timer_divider = 80; // Timer ticks at 1 MHz
static const uint64_t timer_max = 1000000; // Timer reaches max once one second has elapsed 
static hw_timer_t* timer = NULL;

SemaphoreHandle_t xSoundSemaphore1, xSoundSemaphore3, xSoundSemaphore2, xLightSemaphore1, xLightSemaphore3, xLightSemaphore2;

static TaskHandle_t timer_init_task = NULL, interrupt_init_task = NULL;

uint16_t hour = 12, minute = 0, second = 0, meridiem = 0, alarm_hour = 12, alarm_minute = 0, alarm_meridiem = 0;
unsigned long button_time = 0, last_button_time = 0;
volatile bool alarm_active, alarm_select = false;

//****************************************************************************************************************************************
// Functions

// Having timer initialization be in its own task allows the timer to be pinned to a specific CPU core. In this program the
// timer is pinned to the first core.
void timerInit(void* parameter){
  timer = timerBegin(0, timer_divider, true);

  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, timer_max, true);

  timerAlarmEnable(timer);

  while(1){
    
  }
}

// Having the initial hardware interrupts be in their own task allows these interrupts and all future interrupts that result
// from them to be pinned to a specific CPU core. In this program all interrupts are pinned to the second core.
void interruptInit(void* parameter){
  attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING);
  attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING);

  while(1){
    
  }
}

// Produces a repeating beeping noise from a buzzer when activated via the binary semaphore xSoundSemaphore1 (given by the
// task alarmCheck), is paused for 5 minutes via the binary semaphore xSoundSemaphore2 (given by the ISR snooze), is
// deactivated via the binary semaphore xSoundSemaphore3 (given by the ISR alarmSwitchOff).
void alarmSound(void* parameter){
  while(1){
    xSemaphoreTake(xSoundSemaphore1, portMAX_DELAY);
    while(1){
      if(xSemaphoreTake(xSoundSemaphore3, 0) == pdTRUE)
        break;
      tone(buzzerPin, 800, 300);
      vTaskDelay(600 / portTICK_PERIOD_MS);
      if(xSemaphoreTake(xSoundSemaphore2, 0) == pdTRUE)
        if(xSemaphoreTake(xSoundSemaphore3, (300000 / portTICK_PERIOD_MS)) == pdTRUE)
          break;
    }
  }
}

// Repeatedly turns an LED on and off when activated via the binary semaphore xLightSemaphore1 (given by the task alarmCheck),
// is paused for 5 minutes via the binary semaphore xLightSemaphore2 (given by the ISR snooze), is deactivated via the binary
// semaphore xLightSemaphore3 (given by the ISR alarmSwitchOff).
void alarmLight(void* parameter){
  while(1){
    xSemaphoreTake(xLightSemaphore1, portMAX_DELAY);
    while(1){
      if(xSemaphoreTake(xLightSemaphore3, 0) == pdTRUE)
        break;
      digitalWrite(lightPin, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(lightPin, LOW);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if(xSemaphoreTake(xLightSemaphore2, 0) == pdTRUE)
        if(xSemaphoreTake(xLightSemaphore3, (300000 / portTICK_PERIOD_MS)) == pdTRUE)
          break;
    }
  }
}

// Checks to see if the alarm switch is in the "On" position and if the clock's time matches the time set for the alarm.
// If both are true, gives binary semaphores needed to activate the nested loops found in the alarmSound and alarmLight
// tasks, thereby causing the buzzer to buzz and the LED to blink.
void alarmCheck(void* parameter){
  while(1){
    if(digitalRead(switchPin) == HIGH && hour == alarm_hour && minute == alarm_minute && second == 0 && !alarm_active){
      alarm_active = true;
      xSemaphoreGive(xSoundSemaphore1);
      xSemaphoreGive(xLightSemaphore1);
      attachInterrupt(digitalPinToInterrupt(switchPin), alarmSwitchOff, FALLING);
      attachInterrupt(digitalPinToInterrupt(snoozePin), snooze, RISING);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Updates the time displayed on the LCD screen.
void draw(void* parameter){
  String time, hour_string, minute_string, second_string, meridiem_string;
  while(1){
    if(alarm_select == false){ // If the alarm button is held down, the LCD screen will display the time being set for the alarm
      if(hour < 10)
        hour_string = String(" " + String(hour));
      else
        hour_string = String(hour);
      if(minute < 10)
        minute_string = String("0" + String(minute));
      else
        minute_string = String(minute);
      if(second < 10)
        second_string = String("0" + String(second));
      else
        second_string = String(second);
      if(meridiem == 0)
        meridiem_string = "AM";
      else
        meridiem_string = "PM";
    }
    else{
      if(alarm_meridiem == 0)
        meridiem_string = "AM";
      else
        meridiem_string = "PM";
      if(alarm_hour < 10)
        hour_string = String(" " + String(alarm_hour));
      else
        hour_string = String(alarm_hour);
      if(alarm_minute < 10)
        minute_string = String("0" + String(alarm_minute));
      else
        minute_string = String(alarm_minute);
      second_string = "00";
    }
    time = String(hour_string + ":" + minute_string + ":" + second_string + " " + meridiem_string);
    lcd.setCursor(0,0);
    lcd.print(time);
  }
}

//****************************************************************************************************************************************
// Interrupt Service Routines (ISRs)

// Increments the time displayed on the clock by one second, executes when timer reaches max (and resets).
void IRAM_ATTR onTimer(){
  if(second < 59)
    second++;
  else if(minute < 59){
    minute++;
    second = 0;
  }
  else{
    if(hour == 11){
      meridiem ^= 1;
      hour++;
    }
    else if(hour == 12)
      hour = 1;
    else
      hour++;
    minute = 0;
    second = 0;
  }
}

// This ISR is initially attached by the task interruptInit, and can later be reattached by the ISR timeButtonReleased.
void IRAM_ATTR timeButtonHeld(){
  button_time = millis();
  if (button_time - last_button_time > 250) // If statement "debounces" the button to ensure that the code in the ISR only executes once for one button press
  {
    detachInterrupt(digitalPinToInterrupt(timePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(hourPin), hourButtonPressedTime, RISING);
    attachInterrupt(digitalPinToInterrupt(minutePin), minuteButtonPressedTime, RISING);
    detachInterrupt(digitalPinToInterrupt(alarmPin)); // Prevents the alarm button from being used while the time button is held down
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR timeButtonHeld.
void IRAM_ATTR timeButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    detachInterrupt(digitalPinToInterrupt(timePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(hourPin));
    detachInterrupt(digitalPinToInterrupt(minutePin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING); // The alarm button can once again be used now that the time button is no longer held down
    if(!timerAlarmEnabled(timer))
      timerAlarmEnable(timer);
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR timeButtonHeld, and increments the hour while the time is being set.
void IRAM_ATTR hourButtonPressedTime(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(timerAlarmEnabled(timer)){ // Set the second to 0 and stop the clock from ticking while time is being set
      second = 0;
      timerAlarmDisable(timer);
    }
    if(hour == 11){
      meridiem ^= 1;
      hour++;
    }
    else if(hour == 12)
      hour = 1;
    else
      hour++;
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR timeButtonHeld, and increments the minute while the time is being set.
void IRAM_ATTR minuteButtonPressedTime(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(timerAlarmEnabled(timer)){ // Set the second to 0 and stop the clock from ticking while time is being set
      second = 0;
      timerAlarmDisable(timer);
    }
    if(minute < 59)
      minute++;
    else
      minute = 0;
    last_button_time = button_time;
  }
}

// This ISR is initially attached by the task interruptInit, and can later be reattached by the ISR alarmButtonReleased.
void IRAM_ATTR alarmButtonHeld(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_select = true;
    detachInterrupt(digitalPinToInterrupt(alarmPin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(hourPin), hourButtonPressedAlarm, RISING);
    attachInterrupt(digitalPinToInterrupt(minutePin), minuteButtonPressedAlarm, RISING);
    detachInterrupt(digitalPinToInterrupt(timePin)); // Prevents the time button from being used while the alarm button is held down
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR alarmButtonHeld.
void IRAM_ATTR alarmButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_select = false;
    detachInterrupt(digitalPinToInterrupt(alarmPin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(hourPin));
    detachInterrupt(digitalPinToInterrupt(minutePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING); // The time button can once again be used now that the alarm button is no longer held down
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR alarmButtonHeld, and increments the hour while the alarm is being set.
void IRAM_ATTR hourButtonPressedAlarm(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(alarm_hour == 11){
      alarm_meridiem ^= 1;
      alarm_hour++;
    }
    else if(alarm_hour == 12)
      alarm_hour = 1;
    else
      alarm_hour++;
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR alarmButtonHeld, and increments the minute while the alarm is being set.
void IRAM_ATTR minuteButtonPressedAlarm(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(alarm_minute < 59)
      alarm_minute++;
    else
      alarm_minute = 0;
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR alarmCheck, and gives the binary semaphores needed to break the nested loops found in the
// alarmSound and alarmLight tasks, thereby stopping the buzzer from buzzing and the LED from blinking.
void IRAM_ATTR alarmSwitchOff(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_active = false;
    xSemaphoreGive(xSoundSemaphore3);
    xSemaphoreGive(xLightSemaphore3);
    detachInterrupt(digitalPinToInterrupt(switchPin));
    detachInterrupt(digitalPinToInterrupt(snoozePin));
    last_button_time = button_time;
  }
}

// This ISR is attached by the ISR alarmCheck, and gives the binary semaphores needed to make the alarmSound and alarmLight
// tasks wait for 5 minutes, thereby temporarily stopping the buzzer from buzzing and the LED from blinking.
void IRAM_ATTR snooze(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    xSemaphoreGive(xSoundSemaphore2);
    xSemaphoreGive(xLightSemaphore2);
    last_button_time = button_time;
  }
}

//****************************************************************************************************************************************
// Main

void setup() {
  pinMode(timePin, INPUT);
  pinMode(hourPin, INPUT);
  pinMode(minutePin, INPUT);
  pinMode(alarmPin, INPUT);
  pinMode(switchPin, INPUT);
  pinMode(snoozePin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(lightPin, OUTPUT);

  Serial.begin(115200); // Serial allows for error detection using the serial monitor in the Arduino IDE

  lcd.init();                      
  lcd.backlight(); // Turns on the LCD screen's backlight

  xSoundSemaphore1 = xSemaphoreCreateBinary();
  xSoundSemaphore3 = xSemaphoreCreateBinary();
  xSoundSemaphore2 = xSemaphoreCreateBinary();
  xLightSemaphore1 = xSemaphoreCreateBinary();
  xLightSemaphore3 = xSemaphoreCreateBinary();
  xLightSemaphore2 = xSemaphoreCreateBinary();
  
  xTaskCreatePinnedToCore(
                          timerInit,
                          "Timer Initialization",
                          700,
                          NULL,
                          1,
                          &timer_init_task,
                          0);

  xTaskCreatePinnedToCore(
                          interruptInit,
                          "ISR Initialization",
                          900,
                          NULL,
                          1,
                          &interrupt_init_task,
                          1);

  xTaskCreatePinnedToCore(
                          alarmSound,
                          "Alarm Sound",
                          700,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmLight,
                          "Alarm Light",
                          600,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmCheck,
                          "Alarm Check",
                          900,
                          NULL,
                          2,
                          NULL,
                          1);                          

  xTaskCreatePinnedToCore(
                          draw,
                          "LCD Draw",
                          1700,
                          NULL,
                          1,
                          NULL,
                          1);
  vTaskDelay(1000 / portTICK_PERIOD_MS); // Give timerInit and interruptInit tasks enough time to execute before deleting them
  vTaskDelete(timer_init_task);
  vTaskDelete(interrupt_init_task);
  vTaskDelete(NULL); // Delete setup and loop tasks
}

// Left empty, will be deleted at the end of setup
void loop() {
  
}
