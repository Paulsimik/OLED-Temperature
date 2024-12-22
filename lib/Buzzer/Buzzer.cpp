#include "Buzzer.h"

Buzzer_Class BUZZER;

void Buzzer_Class::InitBuzzer(uint32_t pin)
{
    _pin = pin;
    pinMode(pin, OUTPUT);
    InternalTone();
    delay(50);
    InternalNoTone();
    delay(50);
    InternalTone();
    delay(50);
    InternalNoTone();
}

void Buzzer_Class::Tick()
{
    switch (buzzerStatus)
    {
        case BEEP_IDLE: break;
        case BEEP_SHORT:
            if(millis() - beepMillis <= 50)
            {
                InternalTone();
            }
            else
            {
                InternalNoTone();
                buzzerStatus = BEEP_IDLE;
            }
            break;
        case BEEP_LONG:
            if(millis() - beepMillis <= 150)
            {
                InternalTone();
            }
            else
            {
                InternalNoTone();
                buzzerStatus = BEEP_IDLE;
            }
            break;
        case BEEP_ALARM:
            if(millis() - beepMillis >= alarmBeepTime)
            {
                buzzerState = !buzzerState;
                beepMillis = millis();
            }

            buzzerState ? InternalTone() : InternalNoTone();
            break;
    }
}

void Buzzer_Class::Single()
{
    if(buzzerStatus != BEEP_IDLE) 
        return;

    buzzerStatus = BEEP_SHORT; 
    beepMillis = millis();
}

void Buzzer_Class::Long()
{
    if(buzzerStatus != BEEP_IDLE) 
        return;

    buzzerStatus = BEEP_LONG; 
    beepMillis = millis();
}

void Buzzer_Class::AlarmStart()
{
    if(buzzerStatus != BEEP_IDLE) 
        return;

    buzzerStatus = BEEP_ALARM; 
    beepMillis = millis();
}

void Buzzer_Class::AlarmStop()
{
    if(buzzerStatus != BEEP_ALARM) 
        return;

    buzzerStatus = BEEP_IDLE; 
    noTone(_pin);
}

void Buzzer_Class::InternalTone()
{
    tone(_pin, 4000);
}

void Buzzer_Class::InternalNoTone()
{
    noTone(_pin);
}