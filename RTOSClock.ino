// Author: Ryan Kelly

#include <LiquidCrystal_I2C.h> // Download at https://github.com/johnrickman/LiquidCrystal_I2C, credit to John Rickman

// GPIO Pins
static const int time_pin = 12, hour_pin = 27, minute_pin = 33, alarm_pin = 15, snooze_pin = 32, switch_pin = 14, buzzer_pin = 4, light_pin = 5;

// Globals
static LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD screen at address 0x27 with 16 columns and 2 rows

static const uint16_t timer_divider = 80; // Timer ticks at 1 MHz
static const uint64_t timer_max = 1000000; // Timer reaches max once one second has elapsed 
static hw_timer_t* timer = NULL;

static SemaphoreHandle_t xSoundSemaphore1, xSoundSemaphore3, xSoundSemaphore2, xLightSemaphore1, xLightSemaphore3, xLightSemaphore2;

static TaskHandle_t timer_init_task = NULL, interrupt_init_task = NULL;

static volatile uint16_t hour = 12, minute = 0, second = 0, meridiem = 0, alarm_hour = 12, alarm_minute = 0, alarm_meridiem = 0;
static volatile bool alarm_active = false, alarm_switch_on;

enum Button_State {neutral, time_button, alarm_button};
static volatile Button_State button_state = neutral;

//****************************************************************************************************************************************
// Interrupt Service Routines (ISRs)

// Increments the time displayed on the clock by one second, executes when timer reaches max (and resets).
static void IRAM_ATTR onTimer(){
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

// Changes state depending on whether the time button is held down or released.
static void IRAM_ATTR timeButton(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250) // If statement "debounces" the button to ensure that the code in the ISR only executes once for one button press
  {
    if(button_state == neutral)
      button_state = time_button;
    else if(button_state == time_button){
      button_state = neutral;
      timerAlarmEnable(timer);
    }
    last_button_time = button_time;
  }
}

// Depending on whether the time or alarm button is held down, this ISR increments the clock's hour or increments the hour at 
// which the alarm will go off.
static void IRAM_ATTR hourButtonPressed(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250)
  {
    if(button_state == time_button){
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
    }
    else if(button_state == alarm_button){
      if(alarm_hour == 11){
        alarm_meridiem ^= 1;
        alarm_hour++;
      }
      else if(alarm_hour == 12)
        alarm_hour = 1;
      else
        alarm_hour++;
      }
    last_button_time = button_time;
  }
}

// Depending on whether the time or alarm button is held down, this ISR increments the clock's minute or increments the minute at 
// which the alarm will go off.
static void IRAM_ATTR minuteButtonPressed(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250)
  {
    if(button_state == time_button){
      if(timerAlarmEnabled(timer)){ // Set the second to 0 and stop the clock from ticking while time is being set
        second = 0;
        timerAlarmDisable(timer);
      }
      if(minute < 59)
        minute++;
      else
        minute = 0;
    }
    else if(button_state == alarm_button){
      if(alarm_minute < 59)
        alarm_minute++;
      else
        alarm_minute = 0;
      last_button_time = button_time;
    }
    last_button_time = button_time;
  }
}

// Changes state depending on whether the alarm button is held down or released.
static void IRAM_ATTR alarmButton(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250)
  {
    if(button_state == neutral)
      button_state = alarm_button;
    else if(button_state == alarm_button)
      button_state = neutral;
    last_button_time = button_time;
  }
}

// Changes state depending on whether the alarm switch is moved into the on or off position. If the switch is moved to the off position,
// the ISR gives the binary semaphores needed to break the nested loops found in the alarmSound and alarmLight tasks, thereby stopping 
// the buzzer from buzzing and the LED from blinking.
static void IRAM_ATTR alarmSwitch(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250)
  {
    if(alarm_switch_on == false)
      alarm_switch_on = true;
    else if(alarm_switch_on == true)
    {
      alarm_switch_on = false;
      alarm_active = false;
      xSemaphoreGive(xSoundSemaphore3);
      xSemaphoreGive(xLightSemaphore3);
    }
    last_button_time = button_time;
  }
}

// This ISR gives the binary semaphores needed to make the alarmSound and alarmLight tasks wait for 5 minutes, thereby temporarily 
// stopping the buzzer from buzzing and the LED from blinking.
static void IRAM_ATTR snooze(){
  unsigned long button_time = millis();
  static unsigned long last_button_time;
  if (button_time - last_button_time > 250 && alarm_active == true)
  {
    xSemaphoreGive(xSoundSemaphore2);
    xSemaphoreGive(xLightSemaphore2);
    last_button_time = button_time;
  }
}

//****************************************************************************************************************************************
// Functions

// Having timer initialization be in its own task allows the timer to be pinned to a specific CPU core. In this program the
// timer is pinned to the first core.
static void timerInit(void* parameter){
  timer = timerBegin(0, timer_divider, true);

  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, timer_max, true);

  timerAlarmEnable(timer);

  while(1){
    
  }
}

// Having the hardware interrupts be in their own task allows these interrupts to be pinned to a specific CPU core. In this 
// program all interrupts are pinned to the second core.
static void interruptInit(void* parameter){
  attachInterrupt(digitalPinToInterrupt(time_pin), timeButton, CHANGE);
  attachInterrupt(digitalPinToInterrupt(alarm_pin), alarmButton, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switch_pin), alarmSwitch, CHANGE);
  attachInterrupt(digitalPinToInterrupt(snooze_pin), snooze, HIGH);
  attachInterrupt(digitalPinToInterrupt(hour_pin), hourButtonPressed, HIGH);
  attachInterrupt(digitalPinToInterrupt(minute_pin), minuteButtonPressed, HIGH);
  
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
      tone(buzzer_pin, 800, 300);
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
static void alarmLight(void* parameter){
  while(1){
    xSemaphoreTake(xLightSemaphore1, portMAX_DELAY);
    while(1){
      if(xSemaphoreTake(xLightSemaphore3, 0) == pdTRUE)
        break;
      digitalWrite(light_pin, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(light_pin, LOW);
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
static void alarmCheck(void* parameter){
  while(1){
    if(alarm_switch_on == true && hour == alarm_hour && minute == alarm_minute && second == 0 && !alarm_active){
      alarm_active = true;
      xSemaphoreGive(xSoundSemaphore1);
      xSemaphoreGive(xLightSemaphore1);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Updates the time displayed on the LCD screen.
static void draw(void* parameter){
  String time, hour_string, minute_string, second_string, meridiem_string;
  while(1){
    if(button_state != alarm_button){
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
    else{ // If the alarm button is held down, the LCD screen will display the time being set for the alarm
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
// Main

void setup() {
  pinMode(time_pin, INPUT);
  pinMode(hour_pin, INPUT);
  pinMode(minute_pin, INPUT);
  pinMode(alarm_pin, INPUT);
  pinMode(switch_pin, INPUT);
  pinMode(snooze_pin, INPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(light_pin, OUTPUT);

  if(digitalRead(switch_pin) == HIGH)
    alarm_switch_on = true;
  else
    alarm_switch_on = false;

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
                          900,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmLight,
                          "Alarm Light",
                          900,
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
