#pragma once

#include <esp_timer.h>
#include <ctime>
#include <string>

#define MAX_ALARMS 10

struct Alarm {
    int id = -1;
    bool active = false;
    time_t time = 0;
    int repeat = 1;
    int interval = 0;
    std::string name;
};

class AlarmManager {
public:
    static AlarmManager* GetInstance();

    void Init();
    bool SetAlarm(int delay, int hour, int minute, int repeat, int interval, const std::string& name);
    bool DeleteAlarmById(int id);
    bool DeleteAlarmByKeyword(const std::string& keyword);
    std::string QueryAllAlarmsJson();
    int GetActiveAlarmCount();

    bool IsRing() const;
    const char* GetCurrentAlarmName() const;
    void StopRing();

private:
    AlarmManager();
    ~AlarmManager();

    void LoadAlarms();
    void SaveAlarms();
    void StartTimerForAlarm(Alarm* alarm);
    void StopTimerForAlarm(Alarm* alarm);
    static void AlarmTimerCallback(void* arg);
    void HandleTriggeredAlarm(Alarm* alarm);

    Alarm alarms_[MAX_ALARMS];
    esp_timer_handle_t alarm_timers_[MAX_ALARMS];

    bool is_ringing_ = false;
    int current_alarm_id_ = -1;

    static AlarmManager* instance_;
};
