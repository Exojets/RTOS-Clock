#include "clock.h"
#include <LiquidCrystal.h>

static const uint16_t timer_divider = 80;
static const uint64_t timer_max = 1000000;

static hw_timer_t* timer = NULL;
static TaskHandle_t alarm_sound_task = NULL;
static TaskHandle_t alarm_light_task = NULL;
uint16_t hour = 12, minute = 0, second = 0, meridiem = 0, alarm_hour = 12, alarm_minute = 0, alarm_meridiem = 0;
unsigned long button_time = 0;  
unsigned long last_button_time = 0;
bool alarm_active, alarm_select = false;
LiquidCrystal lcd(27, 33, 15, 32, 17, 21);

void timerInit(void* parameter){
  timer = timerBegin(0, timer_divider, true);

  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, timer_max, true);

  timerAlarmEnable(timer);
}

void interruptInit(void* parameter){
  attachInterrupt(digitalPinToInterrupt(5), timeButtonHeld, RISING);
  attachInterrupt(digitalPinToInterrupt(16), alarmButtonHeld, RISING);
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
    detachInterrupt(digitalPinToInterrupt(5));
    attachInterrupt(digitalPinToInterrupt(5), timeButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(18), hourButtonPressedTime, RISING);
    attachInterrupt(digitalPinToInterrupt(19), minuteButtonPressedTime, RISING);
    last_button_time = button_time;
  }
}

void IRAM_ATTR timeButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    detachInterrupt(digitalPinToInterrupt(5));
    attachInterrupt(digitalPinToInterrupt(5), timeButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(18));
    detachInterrupt(digitalPinToInterrupt(19));
    if(!timerAlarmEnabled)
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
    detachInterrupt(digitalPinToInterrupt(16));
    attachInterrupt(digitalPinToInterrupt(16), alarmButtonReleased, FALLING);
    attachInterrupt(digitalPinToInterrupt(18), hourButtonPressedAlarm, RISING);
    attachInterrupt(digitalPinToInterrupt(19), minuteButtonPressedAlarm, RISING);
    last_button_time = button_time;
  }
}

void IRAM_ATTR alarmButtonReleased(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_select = false;
    detachInterrupt(digitalPinToInterrupt(16));
    attachInterrupt(digitalPinToInterrupt(16), alarmButtonHeld, RISING);
    detachInterrupt(digitalPinToInterrupt(18));
    detachInterrupt(digitalPinToInterrupt(19));
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
    if(digitalRead(14) == HIGH && hour == alarm_hour && minute == alarm_minute && !alarm_active){
      alarm_active = true;
      xTaskNotifyGive(alarm_sound_task);
      xTaskNotifyGive(alarm_light_task);
      attachInterrupt(digitalPinToInterrupt(14), alarmSwitchOff, FALLING);
      attachInterrupt(digitalPinToInterrupt(4), snooze, RISING);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void alarmSound(void* parameter){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while(1){
      if(ulTaskNotifyTakeIndexed(1, pdTRUE, 0) != 0)
        break;
      tone(22, 800, 500);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      if(ulTaskNotifyTakeIndexed(2, pdTRUE, 0) != 0)
        vTaskDelay(300000 / portTICK_PERIOD_MS);
    }
  }
}

void alarmLight(void* parameter){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while(1){
      if(ulTaskNotifyTakeIndexed(1, pdTRUE, 0) != 0)
        break;
      digitalWrite(23, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(23, LOW);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if(ulTaskNotifyTakeIndexed(2, pdTRUE, 0) != 0)
        vTaskDelay(300000 / portTICK_PERIOD_MS);
    }
  }
}

void IRAM_ATTR alarmSwitchOff(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    alarm_active = false;
    vTaskNotifyGiveIndexedFromISR(alarm_sound_task, 1, NULL);
    vTaskNotifyGiveIndexedFromISR(alarm_light_task, 1, NULL);
    detachInterrupt(digitalPinToInterrupt(14));
    detachInterrupt(digitalPinToInterrupt(4));
    last_button_time = button_time;
  }
}

void IRAM_ATTR snooze(){
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
    vTaskNotifyGiveIndexedFromISR(alarm_sound_task, 2, NULL);
    vTaskNotifyGiveIndexedFromISR(alarm_light_task, 2, NULL);
    last_button_time = button_time;
  }
}

void draw(void* parameter){
  String time, meridiemString;
  while(1){
    if(alarm_select == false){
      if(meridiem == 0)
        meridiemString = "AM";
      else
        meridiemString = "PM";
      time = String(String(hour) + ":" + String(minute) + ":" + String(second) + " " + meridiemString);
      lcd.print(time);
    }
    else{
      if(alarm_meridiem == 0)
        meridiemString = "AM";
      else
        meridiemString = "PM";
      time = String(String(alarm_hour) + ":" + String(alarm_minute) + " " + meridiemString);
      lcd.print(time);
    }

  }
}

void setup() {
  pinMode(5, INPUT); //Time
  pinMode(18, INPUT); //Hour
  pinMode(19, INPUT); //Minute
  pinMode(16, INPUT); //Alarm
  pinMode(14, INPUT); //Switch
  pinMode(4, INPUT); //Snooze
  pinMode(22, OUTPUT); //Buzzer
  pinMode(23, OUTPUT); //Light

  lcd.begin(10, 1);
  
  xTaskCreatePinnedToCore(
                          timerInit,
                          "Timer Initialization",
                          1024,
                          NULL,
                          1,
                          NULL,
                          0);

  xTaskCreatePinnedToCore(
                          interruptInit,
                          "ISR Initialization",
                          1024,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmCheck,
                          "Alarm Check",
                          1024,
                          NULL,
                          1,
                          NULL,
                          1);

  xTaskCreatePinnedToCore(
                          alarmSound,
                          "Alarm Sound",
                          1024,
                          NULL,
                          1,
                          &alarm_sound_task,
                          1);

  xTaskCreatePinnedToCore(
                          alarmLight,
                          "Alarm Light",
                          1024,
                          NULL,
                          1,
                          &alarm_light_task,
                          1);

  xTaskCreatePinnedToCore(
                          draw,
                          "LCD Draw",
                          1024,
                          NULL,
                          1,
                          NULL,
                          1);

  vTaskDelete(NULL);
}

void loop() {
  
}
