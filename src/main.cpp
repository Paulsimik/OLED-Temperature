//##############################//
//      Receiver Temp Sensor    //
//          Version 1.2         //
//            By Paul           //
//##############################//

#include <Arduino.h>
#include "Buzzer.h"
#include <RF24.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <OneButton.h>
#include <EEPROM.h>

#define CE_PIN        PB13
#define CSN_PIN       PB12
#define LED_PIN       PC13
#define BUZZER_PIN    PB1
#define BUTTON_LEFT   PB11
#define BUTTON_RIGHT  PB10
#define R_LED         PA0
#define G_LED         PA1
#define B_LED         PA2

uint8_t minimal_temp = 25;
uint8_t normal_temp = 35;
uint8_t high_temp = 55;
uint8_t critical_temp = 70;

enum pageType
{
  MAIN_SCREEN,
  MIN_TEMP,
  NORMAL_TEMP,
  HIGH_TEMP,
  CRITICAL_TEMP
};

enum statusTemp
{
  TEMP_MINIMUM,
  TEMP_NORMAL,
  TEMP_HIGH,
  TEMP_CRITICAL
};

enum pageType currPage = MAIN_SCREEN;
//enum pageType currPage = MIN_TEMP;
enum statusTemp currTemp = TEMP_MINIMUM;

const byte address[6] = "00001";
RF24 radio(CE_PIN, CSN_PIN);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
OneButton buttonLeft(BUTTON_LEFT, INPUT_PULLUP, true);
OneButton buttonRight(BUTTON_RIGHT, INPUT_PULLUP, true);

void Init();
void GetButtonState();
void IsClickLeft();
void IsClickRight();
void IsLongPressLeft();
void IsLongPressRight();
void MainScreen();
void Menu_MinTemp();
void Menu_NormalTemp();
void Menu_HighTemp();
void Menu_CriticalTemp();
void EEPROMSave();
void EEPROMRead();
void EEPROMUpdate();
void RGBLedTick();
void CheckTempAlarm();
void SetRGB(uint8_t R, uint8_t G, uint8_t B);
void AdjustUint8_t(uint8_t *value, uint8_t min, uint8_t max);
boolean IsMinimalTemp();
boolean IsNormalTemp();
boolean IsHighTemp();
boolean IsCriticalTemp();
void HighAlarm();
void CriticalAlarm();
void Blinking();

bool startCheckMinTemp = false, startCheckNormalTemp = false, startCheckHighTemp = false, startCheckCriticalTemp = false;
bool updateValue = false, eepromSave = false, blinking = false;
bool isClickLeft = false, isClickRight = false, isLongPressLeft = false, isLongPressRight = false;
bool alarm = false, alarmHighEnabled = false, alarmCriticalEnabled = false, alarmCriticalRepeatEnabled = false;

int temperature = 0, radioRepeatError = 0;

uint8_t red, green, blue;
uint8_t tempPositionX, criticalPosX;

unsigned long radioCheckMillis, minimalTempMillis, normalTempMillis, highTempMillis, criticalTempMillis, criticalTempRepeatMillis, blinkingMillis;
unsigned long delayTimeStatus = 60, repeatCriticalAlarmTime = 300;

// =======================================================================//
//                                 MAIN                                   //
// =======================================================================//

void setup()
{
  EEPROM.begin();
  SPI.begin();
  Wire.begin();
  u8g2.begin();
  radio.begin();
  radio.openReadingPipe(0, address); 
  radio.setPALevel(RF24_PA_MAX);
  radio.startListening();
  buttonLeft.attachClick(IsClickLeft);
  buttonRight.attachClick(IsClickRight);
  buttonLeft.attachLongPressStart(IsLongPressLeft);
  buttonRight.attachLongPressStart(IsLongPressRight);

  pinMode(LED_PIN, OUTPUT);
  pinMode(CE_PIN, OUTPUT);
  pinMode(CSN_PIN, OUTPUT);
  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(B_LED, OUTPUT);

  Init();
  EEPROMRead();
}

void loop()
{
  switch (currPage)
  {
    case MAIN_SCREEN:   MainScreen();   break;
    case MIN_TEMP:      Menu_MinTemp();      break;
    case NORMAL_TEMP:   Menu_NormalTemp();   break;
    case HIGH_TEMP:     Menu_HighTemp();     break;
    case CRITICAL_TEMP: Menu_CriticalTemp(); break;
  }
}

void MainScreen()
{
  EEPROMSave();

  while (1)
  {
    BUZZER.Tick();
    GetButtonState();

    if(isLongPressLeft)
    {
      isLongPressLeft = false;
      currPage = CRITICAL_TEMP;
      return;
    }
    if(isLongPressRight)
    {
      isLongPressRight = false;
      currPage = MIN_TEMP;
      return;
    }

    if(millis() - radioCheckMillis >= 1000)
    {
      if(radio.available()) 
      {   
        radioRepeatError = 0;     
        radio.read(&temperature, sizeof(temperature));    
        if(temperature <= 0)
        {
          tempPositionX = 0;
          temperature = 999;
        }
        if(temperature > 0 && temperature < 10)
        {
        tempPositionX = 55;
        }
        else if (temperature >= 10 && temperature < 100)
        {
          tempPositionX = 30;
        }
        else if (temperature >= 100)
        {
          tempPositionX = 3;
        }

        u8g2.clearBuffer();					
        u8g2.setFont(u8g2_font_logisoso42_tn);
        u8g2.drawStr(tempPositionX, 50, String(temperature).c_str());	
        u8g2.drawCircle(90, 11, 4);
        u8g2.setFont(u8g2_font_logisoso28_tr);
        u8g2.drawStr(97, 38, "C");
        u8g2.sendBuffer();  
      }
      else
      {
        radioRepeatError++;
      }

      if(radioRepeatError >= 30)
      {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso28_tr);
        u8g2.drawStr(45, 30, "NO");
        u8g2.drawStr(15, 60, "SIGNAL");
        u8g2.sendBuffer();
      }

      radioCheckMillis = millis();
    }

    if(temperature > 200)
      return;

    RGBLedTick();
    CheckTempAlarm();
  }
}

// =======================================================================//
//                               TEMP CHECK                               //
// =======================================================================//

void CheckTempAlarm()
{
  if(alarm && (isClickLeft || isClickRight))
  {
    isClickLeft = false;
    isClickRight = false;
    alarm = false;
    BUZZER.AlarmStop();

    if(IsCriticalTemp())
    {
      alarmCriticalRepeatEnabled = true;
      criticalTempRepeatMillis = millis();
    }

    return;
  }

  if(IsMinimalTemp() || IsNormalTemp())
  {
    if(alarm)
    {
      alarm = false;
      BUZZER.AlarmStop();
    }

    alarmHighEnabled = true;
    alarmCriticalEnabled = true;
    alarmCriticalRepeatEnabled = false;
    return;
  }

  if(IsHighTemp())
  {
    HighAlarm();
    alarmCriticalEnabled = true;
    alarmCriticalRepeatEnabled = false;
    u8g2.setPowerSave(0);
    return;
  }

  if(IsCriticalTemp())
  {
    CriticalAlarm();
    Blinking();
  }
}

void HighAlarm()
{
  if(!alarmHighEnabled || alarm)
    return;

  alarm = true;
  alarmHighEnabled = false;
  BUZZER.alarmBeepTime = 800;
  BUZZER.AlarmStart();
}

void CriticalAlarm()
{
  if(alarmCriticalRepeatEnabled && (millis() - criticalTempRepeatMillis >= (repeatCriticalAlarmTime * (long)1000)))
  {
    alarmCriticalRepeatEnabled = false;
    alarm = true;
    BUZZER.alarmBeepTime = 150;
    BUZZER.AlarmStart();
    return;
  }

  if(!alarmCriticalEnabled || alarm)
    return;

  alarm = true;
  alarmCriticalEnabled = false;
  BUZZER.alarmBeepTime = 300;
  BUZZER.AlarmStart();
}

boolean IsMinimalTemp()
{
  if(temperature > minimal_temp)
  {
    startCheckMinTemp = false;
    return false;
  }

  if(!startCheckMinTemp && temperature <= minimal_temp)
  {
    startCheckMinTemp = true;
    minimalTempMillis = millis();
    return false;
  }

  if(startCheckMinTemp && (millis() - minimalTempMillis >= (delayTimeStatus * (long)1000)))
  {
    currTemp = TEMP_MINIMUM;
    return true;
  }

  return false;
}

boolean IsNormalTemp()
{
  if(temperature <= minimal_temp || temperature >= high_temp)
  {
    startCheckNormalTemp = false;
    return false;
  }

  if(!startCheckNormalTemp && (temperature > minimal_temp && temperature < high_temp))
  {
    startCheckNormalTemp = true;
    normalTempMillis = millis();
    return false;
  }

  if(startCheckNormalTemp && (millis() - normalTempMillis >= (delayTimeStatus * (long)1000)))
  {
    currTemp = TEMP_NORMAL;
    return true;
  }

  return false;
}

boolean IsHighTemp()
{
  if(temperature < high_temp || temperature >= critical_temp)
  {
    startCheckHighTemp = false;
    return false;
  }

  if(!startCheckHighTemp && (temperature >= high_temp && temperature < critical_temp))
  {
    startCheckHighTemp = true;
    highTempMillis = millis();
    return false;
  }

  if(startCheckHighTemp && (millis() - highTempMillis >= (delayTimeStatus * (long)1000)))
  {
    currTemp = TEMP_HIGH;
    return true;
  }

  return false;
}

boolean IsCriticalTemp()
{
  if(temperature < critical_temp)
  {
    startCheckCriticalTemp = false;
    return false;
  }

  if(!startCheckCriticalTemp && temperature >= critical_temp)
  {
    startCheckCriticalTemp = true;
    criticalTempMillis = millis();
    return false;
  }

  if(startCheckCriticalTemp && (millis() - criticalTempMillis >= (delayTimeStatus * (long)1000)))
  {
    currTemp = TEMP_CRITICAL;
    return true;
  }

  return false;
}

// =======================================================================//
//                               SETUP MENUS                              //
// =======================================================================//

void Menu_MinTemp()
{
  updateValue = true;

  while (1)
  {
    BUZZER.Tick();
    GetButtonState();
    
    if(isLongPressLeft)
    {
      isLongPressLeft = false;
      currPage = MAIN_SCREEN;
      return;
    }

    if(isLongPressRight)
    {
      isLongPressRight = false;
      currPage = NORMAL_TEMP;
      return;
    }

    AdjustUint8_t(&minimal_temp, 10, normal_temp - 1);

    if(updateValue)
    {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_9x18_tr);
      u8g2.drawStr(0, 15, "MIN TEMP");
      u8g2.drawLine(0, 20, 128, 20);
      u8g2.setFont(u8g2_font_fub20_tn);
      u8g2.drawStr(50, 52, String(minimal_temp).c_str());
      u8g2.drawStr(10, 50, "-");
      u8g2.drawStr(100, 50, "+");
      u8g2.sendBuffer();
      updateValue = false;
    }
  }
}

void Menu_NormalTemp()
{
  updateValue = true;

  while (1)
  {
    BUZZER.Tick();
    GetButtonState();
    
    if(isLongPressLeft)
    {
      isLongPressLeft = false;
      currPage = MIN_TEMP;
      return;
    }

    if(isLongPressRight)
    {
      isLongPressRight = false;
      currPage = HIGH_TEMP;
      return;
    }

    AdjustUint8_t(&normal_temp, minimal_temp + 1, high_temp - 1);

    if(updateValue)
    {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_9x18_tr);
      u8g2.drawStr(0, 15, "NORMAL TEMP");
      u8g2.drawLine(0, 20, 128, 20);
      u8g2.setFont(u8g2_font_fub20_tn);
      u8g2.drawStr(50, 52, String(normal_temp).c_str());
      u8g2.drawStr(10, 50, "-");
      u8g2.drawStr(100, 50, "+");
      u8g2.sendBuffer();
      updateValue = false;
    }
  }
}

void Menu_HighTemp()
{
  updateValue = true;

  while (1)
  {
    BUZZER.Tick();
    GetButtonState();
    
    if(isLongPressLeft)
    {
      isLongPressLeft = false;
      currPage = NORMAL_TEMP;
      return;
    }

    if(isLongPressRight)
    {
      isLongPressRight = false;
      currPage = CRITICAL_TEMP;
      return;
    }

    AdjustUint8_t(&high_temp, normal_temp + 1, critical_temp - 1);

    if(updateValue)
    {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_9x18_tr);
      u8g2.drawStr(0, 15, "HIGH TEMP");
      u8g2.drawLine(0, 20, 128, 20);
      u8g2.setFont(u8g2_font_fub20_tn);
      u8g2.drawStr(50, 52, String(high_temp).c_str());
      u8g2.drawStr(10, 50, "-");
      u8g2.drawStr(100, 50, "+");
      u8g2.sendBuffer();
      updateValue = false;
    }
  }
}

void Menu_CriticalTemp()
{
  updateValue = true;

  while (1)
  {
    BUZZER.Tick();
    GetButtonState();
    
    if(isLongPressLeft)
    {
      isLongPressLeft = false;
      currPage = HIGH_TEMP;
      return;
    }

    if(isLongPressRight)
    {
      isLongPressRight = false;
      currPage = MAIN_SCREEN;
      return;
    }

    AdjustUint8_t(&critical_temp, high_temp + 1, 150);

    if(updateValue)
    {
      if(critical_temp > 99)
      {
        criticalPosX = 40;
      }
      else
        criticalPosX = 50;

      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_9x18_tr);
      u8g2.drawStr(0, 15, "CRITICAL TEMP");
      u8g2.drawLine(0, 20, 128, 20);
      u8g2.setFont(u8g2_font_fub20_tn);
      u8g2.drawStr(criticalPosX, 52, String(critical_temp).c_str());
      u8g2.drawStr(10, 50, "-");
      u8g2.drawStr(100, 50, "+");
      u8g2.sendBuffer();
      updateValue = false;
    }
  }
}

// =======================================================================//
//                                INTERNAL                                //
// =======================================================================//

void Init()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_13b_mf);
  u8g2.drawStr(7, 15, "OLED Temperature");
  u8g2.drawStr(36, 35, "Receiver");
  u8g2.drawStr(8, 55, "Version 1.2 2024");
  u8g2.sendBuffer();

  digitalWrite(B_LED, HIGH);
  delay(500);
  digitalWrite(B_LED, LOW);
  digitalWrite(G_LED, HIGH);
  delay(500);
  digitalWrite(G_LED, LOW);
  digitalWrite(R_LED, HIGH);
  delay(500);
  digitalWrite(R_LED, LOW);
  BUZZER.InitBuzzer(BUZZER_PIN);
}

void IsClickLeft() {isClickLeft = true; BUZZER.Single();}

void IsClickRight(){isClickRight = true; BUZZER.Single();}

void IsLongPressLeft(){isLongPressLeft = true; BUZZER.Long();}

void IsLongPressRight(){isLongPressRight = true; BUZZER.Long();}

void GetButtonState()
{
  buttonLeft.tick();
  buttonRight.tick();
}

void AdjustUint8_t(uint8_t *value, uint8_t min, uint8_t max)
{
  if(isClickLeft)
  {
    isClickLeft = false;

    if(*value > min)
    {
      *value = *value - 1;
      updateValue = true;
      eepromSave = true;
    }
  }

  if(isClickRight)
  {
    isClickRight = false;

    if(*value < max)
    {
      *value = *value + 1;
      updateValue = true;
      eepromSave = true;
    }
  }
}

// =======================================================================//
//                                  EEPROM                                //
// =======================================================================//

void EEPROMSave()
{
  if(!eepromSave)
    return;

  eepromSave = false;
  EEPROMUpdate();
}

void EEPROMRead()
{
  minimal_temp = EEPROM.read(0);
  if(minimal_temp >= 100) minimal_temp = 25;
  normal_temp = EEPROM.read(1);
  if(normal_temp >= 100) normal_temp = 40;
  high_temp = EEPROM.read(2);
  if(high_temp >= 150) high_temp = 55;
  critical_temp = EEPROM.read(3);
  if(critical_temp >= 200) critical_temp = 70;
}

void EEPROMUpdate()
{
  EEPROM.update(0, minimal_temp);
  EEPROM.update(1, normal_temp);
  EEPROM.update(2, high_temp);
  EEPROM.update(3, critical_temp);
}

// =======================================================================//
//                                 RGB LED                                //
// =======================================================================//

void Blinking()
{
  if(!blinking && millis() - blinkingMillis >= 300)
  {
    blinking = true;
    blinkingMillis = millis();
    u8g2.setPowerSave(0);
  }
  else if (blinking && millis() - blinkingMillis >= 1000)
  {
    blinking = false;
    blinkingMillis = millis();
    u8g2.setPowerSave(1);
  }
  
  blinking ? analogWrite(R_LED, 255) : analogWrite(R_LED, 0);
}

void RGBLedTick()
{
  if (temperature >= critical_temp)
    return;

  if(temperature < minimal_temp)
  {
    red = 0;
    green = 0;
    blue = 0;
  }
  else if (temperature >= minimal_temp && temperature < normal_temp)
  {
    red = 0;
    green = map(temperature, minimal_temp, normal_temp, 0, 20);
    blue = map(temperature, minimal_temp, normal_temp, 10, 0);
  }
  else if (temperature == normal_temp)
  {
    red = 0;
    green = 20;
    blue = 0;
  }
  else if (temperature > normal_temp && temperature < high_temp)
  {
    red = map(temperature, normal_temp, high_temp, 0, 10);
    green = map(temperature, normal_temp, high_temp, 20, 0);
    blue = 0;
  }
  else if (temperature == high_temp)
  {
    red = 10;
    green = 0;
    blue = 0;
  }
  else if (temperature > high_temp)
  {
    red = map(temperature, high_temp, critical_temp, 10, 100);
    green = 0;
    blue = 0;
  }
  
  SetRGB(red, green, blue);
}

void SetRGB(uint8_t R, uint8_t G, uint8_t B)
{
  analogWrite(R_LED, R);
  analogWrite(G_LED, G);
  analogWrite(B_LED, B);
}