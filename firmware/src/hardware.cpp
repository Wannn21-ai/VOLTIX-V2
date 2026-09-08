#include "hardware.h"

#include "config.h"
#include "state.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <math.h>

namespace {
Adafruit_SSD1306 display(128, 64, &Wire, -1);
PZEM004Tv30 pzem(Serial2, Config::PZEM_RX_PIN, Config::PZEM_TX_PIN);
bool displayReady = false;
uint32_t lastSensorMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t buzzerChangedMs = 0;
bool buzzerOn = false;

volatile bool okInterruptPending = false;
volatile bool navInterruptPending = false;

uint32_t okLastAcceptedMs = 0;
uint32_t navLastAcceptedMs = 0;

int relayLevel(bool on) {
  return Config::RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
}

float safeValue(float value) {
  return isnan(value) || isinf(value) ? 0.0f : value;
}

void IRAM_ATTR onOkButtonPressed() {
  okInterruptPending = true;
}

void IRAM_ATTR onNavButtonPressed() {
  navInterruptPending = true;
}

bool takeButtonPress(
  volatile bool& pending,
  uint32_t& lastAcceptedMs
) {
  noInterrupts();
  const bool wasPending = pending;
  pending = false;
  interrupts();

  if (!wasPending) return false;

  const uint32_t now = millis();

  if (
    lastAcceptedMs > 0 &&
    now - lastAcceptedMs < Config::BUTTON_DEBOUNCE_MS
  ) {
    return false;
  }

  lastAcceptedMs = now;
  return true;
}

void line(uint8_t y, const String& value, uint8_t size = 1) {
  display.setTextSize(size);
  display.setCursor(0, y);
  display.print(value);
}

void renderMenu(const char* title, const char* const* items, uint8_t count) {
  line(0, title, 1);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  for (uint8_t index = 0; index < count; ++index) {
    display.setCursor(0, 17 + index * 14);
    display.print(index == menuIndex ? "> " : "  ");
    display.print(items[index]);
  }
}

String oledDuration(uint32_t totalSeconds) {
  const uint32_t hours =
    (totalSeconds / 3600U) % 100U;

  const uint32_t minutes =
    (totalSeconds % 3600U) / 60U;

  const uint32_t seconds =
    totalSeconds % 60U;

  char buffer[9];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02lu:%02lu:%02lu",
    static_cast<unsigned long>(hours),
    static_cast<unsigned long>(minutes),
    static_cast<unsigned long>(seconds)
  );

  return String(buffer);
}

String oledCost(float value) {
  String result;

  if (value < 10.0f) {
    result = String(value, 1);
  } else if (value < 1000.0f) {
    result = String(value, 0);
  } else {
    result = String(
      static_cast<uint32_t>(value)
    );
  }

  result.replace(".", ",");
  return String("Rp") + result;
}

void metricCell(
  uint8_t x,
  uint8_t y,
  const char* label,
  const String& value
) {
  display.setTextSize(1);
  display.setCursor(x, y);
  display.print(label);
  display.print(" ");
  display.print(value);
}

void renderMonitoringGrid() {
  String header =
    String(operatingModeText(operatingMode)) +
    ": " +
    currentSession.loadName;

  // Maksimal sekitar 21 karakter pada OLED 128 px.
  if (header.length() > 21) {
    header = header.substring(0, 21);
  }

  line(0, header, 1);

  // Garis header dan pemisah dua kolom.
  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );

  display.drawLine(
    62,
    13,
    62,
    63,
    SSD1306_WHITE
  );

  // Baris pertama: tegangan dan arus.
  metricCell(
    0,
    15,
    "V",
    String(sensorReading.voltage, 1) + "V"
  );

  metricCell(
    66,
    15,
    "I",
    String(sensorReading.current, 3) + "A"
  );

  // Baris kedua: daya dan energi sesi.
  metricCell(
    0,
    31,
    "P",
    String(sensorReading.power, 1) + "W"
  );

  metricCell(
    66,
    31,
    "E",
    String(currentSession.energyWh, 2) + "Wh"
  );

  // Baris ketiga: durasi dan biaya.
  metricCell(
    0,
    47,
    "D",
    oledDuration(currentSession.durationSec)
  );

  metricCell(
    66,
    47,
    "C",
    oledCost(currentSession.cost)
  );
}

void renderStatus() {
  if (alarmLatched || currentSession.state == SessionState::OVERLOAD) {
    line(0, "OVERLOAD", 2);
    line(19, "Relay OFF", 1);
    line(31, String("Puncak: ") +
            String(currentSession.peakPowerW, 1) + " W", 1);
    line(44, "Matikan peringatan", 1);
    line(55, "Tekan OK", 1);
    return;
  }

  if (networkState == NetworkState::PORTAL) {
    line(0, "SETUP WIFI", 1);
    line(16, "Hubungkan ke:", 1);
    line(29, "VOLTIX-SETUP", 1);
    line(44, "Buka 192.168.4.1", 1);
    return;
  }

  switch (currentSession.state) {
    case SessionState::VALIDATING_LOAD: {
      line(0, "VALIDASI BEBAN", 1);
      line(16, currentSession.loadName.substring(0, 20), 1);
      line(31, "Relay: ON", 1);
      line(46, String(sensorReading.power, 1) + " W", 1);
      break;
    }
    case SessionState::MONITORING:
  renderMonitoringGrid();
  break;
    case SessionState::FINISHED:
      line(0, "SESSION SELESAI", 1);
      line(16, currentSession.loadName.substring(0, 20), 1);
      line(31, String(currentSession.energyWh, 2) + " Wh", 1);
      line(46, endReasonText(currentSession.endReason), 1);
      break;
    case SessionState::ERROR:
      line(0, "SYSTEM ERROR", 1);
      line(20, "Periksa Serial", 1);
      break;
    default:
      line(0, "VOLTIX V2", 2);
      line(24, String("Mode: ") + operatingModeText(operatingMode), 1);
      line(38, String("WiFi: ") + networkStateText(networkState), 1);
      line(52, "Siap digunakan", 1);
      break;
  }
}
}

void hardwareForceSafeOutputs() {
  pinMode(Config::RELAY_PIN, OUTPUT);
  digitalWrite(Config::RELAY_PIN, relayLevel(false));
  pinMode(Config::GREEN_LED_PIN, OUTPUT);
  pinMode(Config::RED_LED_PIN, OUTPUT);
  pinMode(Config::WIFI_LED_PIN, OUTPUT);
  pinMode(Config::BUZZER_PIN, OUTPUT);
  digitalWrite(Config::GREEN_LED_PIN, LOW);
  digitalWrite(Config::RED_LED_PIN, LOW);
  digitalWrite(Config::WIFI_LED_PIN, LOW);
  digitalWrite(Config::BUZZER_PIN, LOW);
  relayOn = false;
}

void hardwareBegin() {
  pinMode(Config::BUTTON_OK_PIN, INPUT_PULLUP);
  pinMode(Config::BUTTON_NAV_PIN, INPUT_PULLUP);

  attachInterrupt(
  digitalPinToInterrupt(Config::BUTTON_OK_PIN),
  onOkButtonPressed,
  FALLING
);

attachInterrupt(
  digitalPinToInterrupt(Config::BUTTON_NAV_PIN),
  onNavButtonPressed,
  FALLING
);

  Serial2.begin(9600, SERIAL_8N1, Config::PZEM_RX_PIN, Config::PZEM_TX_PIN);
  Wire.begin(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN);
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
  if (displayReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
  }
  Serial.println("[hardware] PZEM Serial2, OLED, buttons and outputs initialized");
}

bool hardwareUpdateSensor() {
  const uint32_t now = millis();
  if (lastSensorMs > 0 && now - lastSensorMs < Config::SENSOR_INTERVAL_MS) return false;
  lastSensorMs = now;

  const float voltage = pzem.voltage();
  const float current = pzem.current();
  const float power = pzem.power();
  const float energy = pzem.energy();
  const float frequency = pzem.frequency();
  const float powerFactor = pzem.pf();
  sensorReading.valid = !isnan(voltage) && !isnan(current) && !isnan(power);
  sensorReading.voltage = safeValue(voltage);
  sensorReading.current = safeValue(current);
  sensorReading.power = safeValue(power);
  sensorReading.energy = safeValue(energy);
  sensorReading.frequency = safeValue(frequency);
  sensorReading.powerFactor = safeValue(powerFactor);
  sensorReading.loadDetected = sensorReading.valid &&
    (sensorReading.power >= runtimeConfig.loadPowerThresholdW ||
     sensorReading.current >= runtimeConfig.loadCurrentThresholdA);
  sensorReading.readAtMs = now;
  if (!sensorReading.valid) Serial.println("[sensor] invalid PZEM sample");
  return true;
}

void hardwareUpdateButtons() {
}

bool hardwareTakeOkPress() {
  const bool pressed =
    takeButtonPress(okInterruptPending, okLastAcceptedMs);

  if (pressed) {
    Serial.println("[button] OK");
  }

  return pressed;
}

void hardwareClearOkPress() {
  noInterrupts();
  okInterruptPending = false;
  interrupts();
}

bool hardwareTakeNavPress() {
  const bool pressed = 
    takeButtonPress(navInterruptPending, navLastAcceptedMs);

  if (pressed) {
    Serial.println("[button] NAV");
  }
  
  return pressed;
}

void hardwareSetRelay(bool on) {
  digitalWrite(Config::RELAY_PIN, relayLevel(on));
  if (relayOn != on) {
    relayOn = on;
    Serial.printf("[relay] %s pin=%u level=%s\n", on ? "ON" : "OFF", Config::RELAY_PIN,
                  relayLevel(on) == LOW ? "LOW" : "HIGH");
  }
}

void hardwareUpdateIndicators() {
  digitalWrite(Config::WIFI_LED_PIN, networkState == NetworkState::CONNECTED ? HIGH : LOW);
  digitalWrite(Config::GREEN_LED_PIN, currentSession.state == SessionState::MONITORING ? HIGH : LOW);
  digitalWrite(Config::RED_LED_PIN, alarmLatched ? HIGH : LOW);

  if (!alarmLatched) {
    buzzerOn = false;
    digitalWrite(Config::BUZZER_PIN, LOW);
    return;
  }
  const uint32_t now = millis();
  const uint32_t interval = buzzerOn ? Config::BUZZER_ON_MS : Config::BUZZER_OFF_MS;
  if (now - buzzerChangedMs >= interval) {
    buzzerChangedMs = now;
    buzzerOn = !buzzerOn;
    digitalWrite(Config::BUZZER_PIN, buzzerOn ? HIGH : LOW);
  }
}

void hardwareRenderDisplay() {
  if (!displayReady) return;
  const uint32_t now = millis();
  if (lastDisplayMs > 0 && now - lastDisplayMs < Config::DISPLAY_INTERVAL_MS) return;
  lastDisplayMs = now;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  static const char* bootItems[] = {"Online", "Offline", "Reset System"};
  static const char* offlineItems[] = {"Next Device", "Mode Online"};
  static const char* resetItems[] = {"Batal", "Ya, reset"};
  if (menuScreen == MenuScreen::BOOT) renderMenu("Pilih Mode", bootItems, 3);
  else if (menuScreen == MenuScreen::OFFLINE_NEXT) renderMenu("Opsi Offline", offlineItems, 2);
  else if (menuScreen == MenuScreen::RESET_CONFIRM) renderMenu("Yakin Reset?", resetItems, 2);
  else renderStatus();
  display.display();
}
