#pragma once

#include <Arduino.h>

namespace Config {
constexpr const char* PROJECT_NAME = "VOLTIX";
constexpr const char* DEVICE_ID = "voltix-001";
constexpr const char* FIRMWARE_VERSION = "2.0.1-presentation";
constexpr const char* FIREBASE_DATABASE_URL =
  "https://voltv1-default-rtdb.asia-southeast1.firebasedatabase.app";

constexpr uint8_t RELAY_PIN = 27;
constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint8_t PZEM_RX_PIN = 16;
constexpr uint8_t PZEM_TX_PIN = 17;
constexpr uint8_t BUTTON_OK_PIN = 32;
constexpr uint8_t BUTTON_NAV_PIN = 33;
constexpr uint8_t WIFI_LED_PIN = 2;
constexpr uint8_t GREEN_LED_PIN = 25;
constexpr uint8_t RED_LED_PIN = 26;
constexpr uint8_t BUZZER_PIN = 5;
constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr float DEFAULT_TARIFF_PER_KWH = 1444.70f;
constexpr float DEFAULT_OVERLOAD_W = 2000.0f;
constexpr float DEFAULT_LOAD_POWER_W = 1.0f;
constexpr float DEFAULT_LOAD_CURRENT_A = 0.02f;
constexpr uint16_t DEFAULT_LOAD_REMOVED_DELAY_SEC = 3;
constexpr float AC_PRESENT_MIN_VOLTAGE = 180.0f;

constexpr uint32_t SENSOR_INTERVAL_MS = 1000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 250;
constexpr uint32_t LOAD_SETTLE_MS = 1000;
constexpr uint32_t LOAD_VALIDATION_TIMEOUT_MS = 5000;
constexpr uint8_t LOAD_STABLE_SAMPLES = 3;
constexpr uint32_t FINISHED_SUMMARY_MS = 5000;
constexpr uint32_t CHECKPOINT_INTERVAL_MS = 5000;
constexpr uint32_t CLOUD_LIVE_INTERVAL_MS = 1500;
constexpr uint32_t CLOUD_COMMAND_INTERVAL_MS = 1200;
constexpr uint32_t CLOUD_CONFIG_INTERVAL_MS = 15000;
constexpr uint32_t CLOUD_SYNC_INTERVAL_MS = 4000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 120;
constexpr uint32_t BUZZER_ON_MS = 220;
constexpr uint32_t BUZZER_OFF_MS = 380;
}
