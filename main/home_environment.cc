#include "home_environment.h"

#include "board.h"
#include "sdkconfig.h"

#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <esp_log.h>

#define TAG "HomeEnv"

bool FetchHomeEnvironment(HomeEnvironmentData& out) {
    const char* url = CONFIG_HOME_ENV_URL;
    if (url == nullptr || url[0] == '\0') {
        ESP_LOGD(TAG, "HOME_ENV_URL empty; skip");
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        ESP_LOGW(TAG, "No network");
        return false;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGE(TAG, "CreateHttp failed");
        return false;
    }

    http->SetHeader("User-Agent", "ESP32-Xiaozhi-HomeEnv/1.0");
    http->SetHeader("Accept", "application/json");

    if (!http->Open("GET", url)) {
        ESP_LOGW(TAG, "Open failed: %s", url);
        return false;
    }

    int status = http->GetStatusCode();
    std::string body = http->ReadAll();
    http->Close();

    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d for %s", status, url);
        return false;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGW(TAG, "JSON parse failed");
        return false;
    }

    cJSON* ok = cJSON_GetObjectItem(root, "ok");
    if (cJSON_IsBool(ok) && !cJSON_IsTrue(ok)) {
        ESP_LOGW(TAG, "API ok=false");
        cJSON_Delete(root);
        return false;
    }

    cJSON* weather = cJSON_GetObjectItem(root, "weather");
    cJSON* temp = cJSON_GetObjectItem(root, "temp");
    cJSON* humidity_text = cJSON_GetObjectItem(root, "humidity_text");
    cJSON* humidity = cJSON_GetObjectItem(root, "humidity");

    if (!cJSON_IsString(weather) || weather->valuestring == nullptr ||
        weather->valuestring[0] == '\0') {
        ESP_LOGW(TAG, "Missing weather");
        cJSON_Delete(root);
        return false;
    }
    if (!cJSON_IsString(temp) || temp->valuestring == nullptr || temp->valuestring[0] == '\0') {
        ESP_LOGW(TAG, "Missing temp");
        cJSON_Delete(root);
        return false;
    }

    out.weather = weather->valuestring;
    out.temp = temp->valuestring;
    if (cJSON_IsString(humidity_text) && humidity_text->valuestring != nullptr &&
        humidity_text->valuestring[0] != '\0') {
        out.humidity_text = humidity_text->valuestring;
    } else if (cJSON_IsNumber(humidity)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "湿度 %d%%", humidity->valueint);
        out.humidity_text = buf;
    } else {
        out.humidity_text = "湿度 --%";
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Fetched %s %s %s", out.weather.c_str(), out.temp.c_str(),
             out.humidity_text.c_str());
    return true;
}
