#include "alarm_manager.h"

#include "application.h"
#include "settings.h"

#include <cJSON.h>
#include <cstring>
#include <esp_log.h>
#include <time.h>

#define TAG "AlarmManager"

AlarmManager* AlarmManager::instance_ = nullptr;

AlarmManager::AlarmManager() {
    memset(alarm_timers_, 0, sizeof(alarm_timers_));
    for (int i = 0; i < MAX_ALARMS; ++i) {
        alarms_[i].id = i;
        alarms_[i].active = false;
        alarms_[i].time = 0;
        alarms_[i].repeat = 1;
        alarms_[i].interval = 0;
        alarms_[i].name.clear();
    }
}

AlarmManager::~AlarmManager() {
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarm_timers_[i]) {
            esp_timer_stop(alarm_timers_[i]);
            esp_timer_delete(alarm_timers_[i]);
            alarm_timers_[i] = nullptr;
        }
    }
}

AlarmManager* AlarmManager::GetInstance() {
    if (instance_ == nullptr) {
        instance_ = new AlarmManager();
        instance_->Init();
    }
    return instance_;
}

void AlarmManager::Init() {
    LoadAlarms();
}

void AlarmManager::LoadAlarms() {
    ESP_LOGI(TAG, "Loading alarms from NVS");
    Settings settings("alarm_clock");
    int active_count = 0;

    for (int i = 0; i < MAX_ALARMS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "alarm_%d", i);
        if (settings.GetInt(key, 0) != 1) {
            alarms_[i].active = false;
            continue;
        }

        alarms_[i].id = i;
        alarms_[i].active = true;

        snprintf(key, sizeof(key), "alarm_time_%d", i);
        alarms_[i].time = settings.GetInt(key, 0);

        snprintf(key, sizeof(key), "alarm_rpt_%d", i);
        alarms_[i].repeat = settings.GetInt(key, 1);

        snprintf(key, sizeof(key), "alarm_itv_%d", i);
        alarms_[i].interval = settings.GetInt(key, 0);

        snprintf(key, sizeof(key), "alarm_name_%d", i);
        alarms_[i].name = settings.GetString(key, "");

        time_t now = time(nullptr);
        if (alarms_[i].time > now) {
            StartTimerForAlarm(&alarms_[i]);
            active_count++;
            ESP_LOGI(TAG, "Loaded alarm %d '%s' at %lld", i, alarms_[i].name.c_str(),
                     (long long)alarms_[i].time);
        } else {
            // Phase 1: skip overdue one-shots (no instant ring after reboot).
            ESP_LOGW(TAG, "Alarm %d overdue, marking inactive", i);
            alarms_[i].active = false;
        }
    }

    ESP_LOGI(TAG, "Alarm load done, active=%d", active_count);
    SaveAlarms();
}

void AlarmManager::SaveAlarms() {
    Settings settings("alarm_clock", true);
    for (int i = 0; i < MAX_ALARMS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "alarm_%d", i);
        settings.SetInt(key, alarms_[i].active ? 1 : 0);
        if (!alarms_[i].active) {
            continue;
        }
        snprintf(key, sizeof(key), "alarm_time_%d", i);
        settings.SetInt(key, static_cast<int32_t>(alarms_[i].time));
        snprintf(key, sizeof(key), "alarm_rpt_%d", i);
        settings.SetInt(key, alarms_[i].repeat);
        snprintf(key, sizeof(key), "alarm_itv_%d", i);
        settings.SetInt(key, alarms_[i].interval);
        snprintf(key, sizeof(key), "alarm_name_%d", i);
        settings.SetString(key, alarms_[i].name);
    }
}

bool AlarmManager::SetAlarm(int delay, int hour, int minute, int /*repeat*/, int /*interval*/,
                            const std::string& name) {
    ESP_LOGI(TAG, "SetAlarm delay=%d hour=%d minute=%d name='%s'", delay, hour, minute, name.c_str());

    int free_slot = -1;
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!alarms_[i].active) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        ESP_LOGE(TAG, "No free alarm slots");
        return false;
    }

    const bool has_clock = (hour >= 0 && hour < 24) && (minute >= 0 && minute < 60);
    const bool has_delay = delay > 0;
    if (!has_clock && !has_delay) {
        ESP_LOGE(TAG, "Need delay>0 or valid hour+minute");
        return false;
    }

    Alarm* alarm = &alarms_[free_slot];
    alarm->id = free_slot;
    alarm->active = true;
    // Phase 1: one-shot only
    alarm->repeat = 1;
    alarm->interval = 0;
    alarm->name = name;

    time_t now = time(nullptr);
    if (has_clock) {
        struct tm timeinfo = {};
        localtime_r(&now, &timeinfo);
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = 0;
        time_t target = mktime(&timeinfo);
        if (target <= now) {
            target += 86400;
        }
        alarm->time = target;
    } else {
        alarm->time = now + delay;
    }

    StartTimerForAlarm(alarm);
    SaveAlarms();
    ESP_LOGI(TAG, "Alarm %d scheduled at %lld", alarm->id, (long long)alarm->time);
    return true;
}

bool AlarmManager::DeleteAlarmById(int id) {
    if (id < 0 || id >= MAX_ALARMS || !alarms_[id].active) {
        return false;
    }
    StopTimerForAlarm(&alarms_[id]);
    alarms_[id].active = false;
    SaveAlarms();
    ESP_LOGI(TAG, "Deleted alarm %d", id);
    return true;
}

bool AlarmManager::DeleteAlarmByKeyword(const std::string& keyword) {
    if (keyword.empty()) {
        return false;
    }
    bool found = false;
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms_[i].active && alarms_[i].name.find(keyword) != std::string::npos) {
            StopTimerForAlarm(&alarms_[i]);
            alarms_[i].active = false;
            found = true;
            ESP_LOGI(TAG, "Deleted alarm %d by keyword '%s'", i, keyword.c_str());
        }
    }
    if (found) {
        SaveAlarms();
    }
    return found;
}

std::string AlarmManager::QueryAllAlarmsJson() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddTrueToObject(root, "success");
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "alarms", arr);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!alarms_[i].active) {
            continue;
        }
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", alarms_[i].id);
        cJSON_AddStringToObject(obj, "name", alarms_[i].name.c_str());

        char time_str[32];
        struct tm timeinfo = {};
        localtime_r(&alarms_[i].time, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        cJSON_AddStringToObject(obj, "time", time_str);
        cJSON_AddNumberToObject(obj, "repeat", alarms_[i].repeat);
        cJSON_AddNumberToObject(obj, "interval", alarms_[i].interval);
        cJSON_AddItemToArray(arr, obj);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result = json_str ? json_str : "{}";
    if (json_str) {
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
    return result;
}

int AlarmManager::GetActiveAlarmCount() {
    int count = 0;
    time_t now = time(nullptr);
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms_[i].active && alarms_[i].time > now) {
            count++;
        }
    }
    return count;
}

void AlarmManager::StartTimerForAlarm(Alarm* alarm) {
    if (alarm->id < 0 || alarm->id >= MAX_ALARMS) {
        return;
    }

    StopTimerForAlarm(alarm);

    time_t now = time(nullptr);
    int64_t timeout_us = (static_cast<int64_t>(alarm->time) - static_cast<int64_t>(now)) * 1000000LL;
    if (timeout_us <= 0) {
        ESP_LOGW(TAG, "Alarm %d time already past", alarm->id);
        return;
    }

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &AlarmManager::AlarmTimerCallback;
    timer_args.arg = alarm;
    timer_args.name = "alarm_timer";

    esp_err_t err = esp_timer_create(&timer_args, &alarm_timers_[alarm->id]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_timer_start_once(alarm_timers_[alarm->id], timeout_us);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_once failed: %s", esp_err_to_name(err));
        esp_timer_delete(alarm_timers_[alarm->id]);
        alarm_timers_[alarm->id] = nullptr;
        return;
    }
    ESP_LOGI(TAG, "Timer started for alarm %d in %lld us", alarm->id, (long long)timeout_us);
}

void AlarmManager::StopTimerForAlarm(Alarm* alarm) {
    if (alarm->id < 0 || alarm->id >= MAX_ALARMS) {
        return;
    }
    if (alarm_timers_[alarm->id]) {
        esp_timer_stop(alarm_timers_[alarm->id]);
        esp_timer_delete(alarm_timers_[alarm->id]);
        alarm_timers_[alarm->id] = nullptr;
    }
}

void AlarmManager::AlarmTimerCallback(void* arg) {
    Alarm* alarm = static_cast<Alarm*>(arg);
    ESP_LOGI(TAG, "Alarm %d fired: %s", alarm->id, alarm->name.c_str());
    GetInstance()->HandleTriggeredAlarm(alarm);
}

bool AlarmManager::IsRing() const {
    return is_ringing_;
}

const char* AlarmManager::GetCurrentAlarmName() const {
    if (current_alarm_id_ >= 0 && current_alarm_id_ < MAX_ALARMS) {
        return alarms_[current_alarm_id_].name.c_str();
    }
    return "";
}

void AlarmManager::StopRing() {
    is_ringing_ = false;
    current_alarm_id_ = -1;
}

void AlarmManager::HandleTriggeredAlarm(Alarm* alarm) {
    is_ringing_ = true;
    current_alarm_id_ = alarm->id;

    // Phase 1 one-shot: deactivate after fire.
    alarm->active = false;
    StopTimerForAlarm(alarm);
    SaveAlarms();

    // Wake the application main loop (no WakeUp() API in this fork).
    Application::GetInstance().Schedule([]() {});
}
