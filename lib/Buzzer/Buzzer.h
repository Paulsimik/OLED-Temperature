#include <Arduino.h>

enum Buzzer_Status
{
    BEEP_IDLE,
    BEEP_SHORT,
    BEEP_LONG,
    BEEP_ALARM
};

class Buzzer_Class
{
private:
    enum Buzzer_Status buzzerStatus = BEEP_IDLE;
    uint32_t _pin;
    unsigned long beepMillis;
    bool buzzerState = false;
    void InternalTone();
    void InternalNoTone();

public:
    uint16_t alarmBeepTime = 1000;
    void InitBuzzer(uint32_t pin);
    void Tick();
    void Single();
    void Long();
    void AlarmStart();
    void AlarmStop();
};

extern Buzzer_Class BUZZER;