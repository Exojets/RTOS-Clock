#include "RTOSClock.h"
#include <LiquidCrystal_I2C.h>

#define timePin 5
#define hourPin 18
#define minutePin 19
#define alarmPin 16
#define snoozePin 17
#define switchPin 21
#define buzzerPin 27
#define lightPin 33

LiquidCrystal_I2C lcd(0x27, 16, 2);

static const uint16_t timer_divider = 80;
static const uint64_t timer_max = 1000000;
static hw_timer_t* timer = NULL;
SemaphoreHandle_t xSoundSemaphore1, xSoundSemaphore2, xSoundSemaphore3, xLightSemaphore1, xLightSemaphore2, xLightSemaphore3;
static TaskHandle_t timer_init_task = NULL, interrupt_init_task = NULL;
uint16_t hour = 12, minute = 0, second = 0, meridiem = 0, alarm_hour = 12, alarm_minute = 0, alarm_meridiem = 0;
unsigned long button_time = 0, last_button_time = 0;
bool alarm_active, alarm_select = false;

void timerInit(void* parameter){
  timer = timerBegin(0, timer_divider, true);

  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, timer_max, true);

  timerAlarmEnable(timer);

  while(1){
    
  }
}

void interruptInit(void* parameter){
  attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING);
  attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING);

  while(1);
}

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

void IRAM_ATTR timeButtonHeld(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    detachInterrupt(digitalPinToInterrupt(timePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(hourPin), hourButtonPressedTime, RISING);
    attachInterrupt(digitalPinToInterrupt(minutePin), minuteButtonPressedTime, RISING);
    detachInterrupt(digitalPinToInterrupt(alarmPin));
    last_button_time = button_time;
  }
}

void IRAM_ATTR timeButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    detachInterrupt(digitalPinToInterrupt(timePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(hourPin));
    detachInterrupt(digitalPinToInterrupt(minutePin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING);
    if(!timerAlarmEnabled(timer))
      timerAlarmEnable(timer);
    last_button_time = button_time;
  }
}

void IRAM_ATTR hourButtonPressedTime(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(timerAlarmEnabled(timer)){
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

void IRAM_ATTR minuteButtonPressedTime(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    if(timerAlarmEnabled(timer)){
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

void IRAM_ATTR alarmButtonHeld(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_select = true;
    detachInterrupt(digitalPinToInterrupt(alarmPin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(hourPin), hourButtonPressedAlarm, RISING);
    attachInterrupt(digitalPinToInterrupt(minutePin), minuteButtonPressedAlarm, RISING);
    detachInterrupt(digitalPinToInterrupt(timePin));
    last_button_time = button_time;
  }
}

void IRAM_ATTR alarmButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_select = false;
    detachInterrupt(digitalPinToInterrupt(alarmPin));
    attachInterrupt(digitalPinToInterrupt(alarmPin), alarmButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(hourPin));
    detachInterrupt(digitalPinToInterrupt(minutePin));
    attachInterrupt(digitalPinToInterrupt(timePin), timeButtonHeld, RISING);
    last_button_time = button_time;
  }
}

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

void alarmSound(void* parameter){
  while(1){
    xSemaphoreTake(xSoundSemaphore1, portMAX_DELAY);
    while(1){
      if(xSemaphoreTake(xSoundSemaphore2, 0) == pdTRUE)
        break;
      tone(buzzerPin, 800, 300);
      vTaskDelay(600 / portTICK_PERIOD_MS);
      if(xSemaphoreTake(xSoundSemaphore3, 0) == pdTRUE)
        if(xSemaphoreTake(xSoundSemaphore2, (300000 / portTICK_PERIOD_MS)) == pdTRUE)
          break;
    }
  }
}

void alarmLight(void* parameter){
  while(1){
    xSemaphoreTake(xLightSemaphore1, portMAX_DELAY);
    while(1){
      if(xSemaphoreTake(xLightSemaphore2, 0) == pdTRUE)
        break;
      digitalWrite(lightPin, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(lightPin, LOW);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if(xSemaphoreTake(xLightSemaphore3, 0) == pdTRUE)
        if(xSemaphoreTake(xLightSemaphore2, (300000 / portTICK_PERIOD_MS)) == pdTRUE)
          break;
    }
  }
}

void IRAM_ATTR alarmSwitchOff(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_active = false;
    xSemaphoreGive(xSoundSemaphore2);
    xSemaphoreGive(xLightSemaphore2);
    detachInterrupt(digitalPinToInterrupt(switchPin));
    detachInterrupt(digitalPinToInterrupt(snoozePin));
    last_button_time = button_time;
  }
}

void IRAM_ATTR snooze(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    xSemaphoreGive(xSoundSemaphore3);
    xSemaphoreGive(xLightSemaphore3);
    last_button_time = button_time;
  }
}

void draw(void* parameter){
  String time, hour_string, minute_string, second_string, meridiem_string;
  while(1){
    if(alarm_select == false){
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

void setup() {
  pinMode(5, INPUT); //Time
  pinMode(18, INPUT); //Hour
  pinMode(19, INPUT); //Minute
  pinMode(16, INPUT); //Alarm
  pinMode(21, INPUT); //Switch
  pinMode(17, INPUT); //Snooze
  pinMode(27, OUTPUT); //Buzzer
  pinMode(33, OUTPUT); //Light

  Serial.begin(115200);

  lcd.init();                      
  lcd.backlight();

  xSoundSemaphore1 = xSemaphoreCreateBinary();
  xSoundSemaphore2 = xSemaphoreCreateBinary();
  xSoundSemaphore3 = xSemaphoreCreateBinary();
  xLightSemaphore1 = xSemaphoreCreateBinary();
  xLightSemaphore2 = xSemaphoreCreateBinary();
  xLightSemaphore3 = xSemaphoreCreateBinary();
  
  xTaskCreatePinnedToCore(
                          timerInit,
                          "Timer Initialization",
                          2048,
                          NULL,
                          1,
                          &timer_init_task,
                          0);

  xTaskCreatePinnedToCore(
                          interruptInit,
                          "ISR Initialization",
                          2048,
                          NULL,
                          1,
                          &interrupt_init_task,
                          1);

  xTaskCreatePinnedToCore(
                          alarmSound,
                          "Alarm Sound",
                          2048,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmLight,
                          "Alarm Light",
                          2048,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmCheck,
                          "Alarm Check",
                          2048,
                          NULL,
                          2,
                          NULL,
                          1);                          

  xTaskCreatePinnedToCore(
                          draw,
                          "LCD Draw",
                          2048,
                          NULL,
                          1,
                          NULL,
                          1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  vTaskDelete(timer_init_task);
  vTaskDelete(interrupt_init_task);
  vTaskDelete(NULL);
}

void loop() {
  
}
