#include "network.h"

#include "config.h"
#include "state.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
constexpr const char* SETUP_AP_NAME = "VOLTIX-SETUP";
DNSServer dnsServer;
WebServer portalServer(80);
Preferences wifiPreferences;
String savedSsid;
String savedPassword;
uint32_t connectionStartedMs = 0;
uint32_t lastRetryMs = 0;
bool portalConfigured = false;
bool credentialsSubmitted = false;
bool wasConnected = false;
bool onlineNetworkingEnabled = false;

String portalPage(const String& message = "") {
  String html;
  html.reserve(1800);
  html += F("<!doctype html><html lang='id'><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>VOLTIX Wi-Fi</title><style>body{font-family:Arial;background:#070b12;color:#eef;padding:24px}");
  html += F("main{max-width:420px;margin:auto;background:#111827;padding:24px;border:1px solid #1f384d;border-radius:16px}");
  html += F("input,button{box-sizing:border-box;width:100%;padding:12px;margin:7px 0;border-radius:9px;border:1px solid #345;background:#09111d;color:#fff}");
  html += F("button{background:#00bcd4;color:#001018;font-weight:700}</style></head><body><main>");
  html += F("<h2>VOLTIX Wi-Fi Setup</h2><p>Masukkan Wi-Fi 2.4 GHz yang akan dipakai alat.</p>");
  if (message.length()) html += "<p>" + message + "</p>";
  html += F("<form method='post' action='/save'><input name='ssid' maxlength='32' placeholder='Nama Wi-Fi' required>");
  html += F("<input name='password' type='password' maxlength='63' placeholder='Password'><button>Simpan & hubungkan</button></form>");
  html += F("</main></body></html>");
  return html;
}

void saveCredentials(const String& ssid, const String& password) {
  if (!wifiPreferences.begin("voltix-wifi", false)) return;
  wifiPreferences.putString("ssid", ssid);
  wifiPreferences.putString("password", password);
  wifiPreferences.end();
  savedSsid = ssid;
  savedPassword = password;
}

void loadCredentials() {
  if (!wifiPreferences.begin("voltix-wifi", true)) return;
  savedSsid = wifiPreferences.getString("ssid", "");
  savedPassword = wifiPreferences.getString("password", "");
  wifiPreferences.end();
}

void configurePortalRoutes() {
  if (portalConfigured) return;
  portalConfigured = true;
  portalServer.on("/", HTTP_GET, []() {
    portalServer.send(200, "text/html", portalPage());
  });
  portalServer.on("/save", HTTP_POST, []() {
    String ssid = portalServer.arg("ssid");
    String password = portalServer.arg("password");
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63) {
      portalServer.send(400, "text/html", portalPage("Data Wi-Fi tidak valid."));
      return;
    }
    saveCredentials(ssid, password);
    credentialsSubmitted = true;
    portalServer.send(200, "text/html", portalPage("Tersimpan. VOLTIX sedang mencoba terhubung."));
  });
  portalServer.onNotFound([]() {
    portalServer.sendHeader("Location", "http://192.168.4.1/", true);
    portalServer.send(302, "text/plain", "");
  });
}

void stopPortal() {
  dnsServer.stop();
  portalServer.stop();
  WiFi.softAPdisconnect(true);
}

void startConnection() {
  if (savedSsid.length() == 0) return;
  stopPortal();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  networkState = NetworkState::CONNECTING;
  connectionStartedMs = millis();
  lastRetryMs = connectionStartedMs;
  Serial.printf("[wifi] connecting to %s\n", savedSsid.c_str());
}

void startPortal() {
  stopPortal();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP_NAME);
  const IPAddress address = WiFi.softAPIP();
  dnsServer.start(53, "*", address);
  configurePortalRoutes();
  portalServer.begin();
  credentialsSubmitted = false;
  networkState = NetworkState::PORTAL;
  Serial.printf("[wifi] setup portal %s at %s\n", SETUP_AP_NAME, address.toString().c_str());
}
}

void networkBegin() {
  loadCredentials();

  onlineNetworkingEnabled = false;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  
  networkState = NetworkState::DISCONNECTED;
  Serial.println("[wifi] radio OFF; waiting for Online selection");
}

void networkStartOnline() {
  onlineNetworkingEnabled = true;
  manualOffline = false;
  operatingMode = OperatingMode::ONLINE;
  loadCredentials();
  if (savedSsid.length() == 0) startPortal();
  else startConnection();
}

void networkEnterManualOffline() {
  onlineNetworkingEnabled = false;
  manualOffline = true;
  operatingMode = OperatingMode::OFFLINE;
  stopPortal();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  networkState = NetworkState::DISCONNECTED;
  Serial.println("[wifi] manual offline mode");
}

void networkUpdate() {
  if (manualOffline || !onlineNetworkingEnabled) {
     return;
  }

  if (networkState == NetworkState::PORTAL) {
    dnsServer.processNextRequest();
    portalServer.handleClient();
    if (credentialsSubmitted) {
      credentialsSubmitted = false;
      startConnection();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (networkState != NetworkState::CONNECTED) {
      networkState = NetworkState::CONNECTED;
      wasConnected = true;
      configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
      Serial.printf("[wifi] connected ip=%s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  const uint32_t now = millis();
  if (networkState == NetworkState::CONNECTING &&
      now - connectionStartedMs >= Config::WIFI_CONNECT_TIMEOUT_MS) {
    if (!wasConnected) startPortal();
    else networkState = NetworkState::DISCONNECTED;
    return;
  }

  if (networkState == NetworkState::CONNECTED) {
    networkState = NetworkState::DISCONNECTED;
    lastRetryMs = now;
    Serial.println("[wifi] connection lost; local session remains active");
  }

  if (networkState == NetworkState::DISCONNECTED && savedSsid.length() > 0 &&
      now - lastRetryMs >= Config::WIFI_RETRY_INTERVAL_MS) {
    startConnection();
  }
}

bool networkConnected() {
  return networkState == NetworkState::CONNECTED && WiFi.status() == WL_CONNECTED;
}

bool networkHasSavedCredentials() {
  return savedSsid.length() > 0;
}

void networkClearCredentials() {
  if (wifiPreferences.begin("voltix-wifi", false)) {
    wifiPreferences.clear();
    wifiPreferences.end();
  }
  savedSsid = "";
  savedPassword = "";
}
