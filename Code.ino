#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WebSocketsServer.h>  // Added for WebSocket support

// Fix for MD_MAX72xx warnings
#define USE_SPI_HW_TRANSACTIONS
#define MD_MAX72XX_USE_SPI_HW_TRANSACTIONS
#define MD_MAX72XX_SPI_USE_MUTEX

// RFID Setup
#define RST_PIN     21
#define SS_PIN      5
MFRC522 rfid(SS_PIN, RST_PIN);

// EEPROM Setup
#define EEPROM_SIZE     250
#define UID_COUNT       5
#define UID_LENGTH      4
#define UID_DATA_ADDR   150
#define API1_ADDR       0
#define API2_ADDR       50
#define API3_ADDR       100
#define AUTH_USER_ADDR  170
#define AUTH_PASS_ADDR  190
#define MAX_API_LEN     50
#define MAX_AUTH_LEN    20

// LED Matrix Setup
#define HARDWARE_TYPE   MD_MAX72XX::FC16_HW
#define MAX_DEVICES     4
#define DATA_PIN        22
#define CLK_PIN         4
#define CS_PIN          15
MD_Parola matrix = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Web server and WebSocket
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);  // WebSocket on port 81
WiFiManager wm;
unsigned long lastScanTime = 0;
const unsigned long scanCooldown = 100;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 50;

// Global variables
String api1, api2, api3;
String authUser, authPass;
String lastScannedUID = "";
bool isScanning = false;

// UI Styles
const char* uiStyles = R"(
:root {
  --primary: #4361ee;
  --secondary: #3f37c9;
  --success: #4cc9f0;
  --danger: #f72585;
  --dark: #212529;
  --light: #f8f9fa;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #1a2a6c, #2d388a, #4361ee);
  color: #fff;
  padding: 20px;
  min-height: 100vh;
}

.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 20px;
}

.card {
  background: rgba(255, 255, 255, 0.1);
  backdrop-filter: blur(10px);
  border-radius: 15px;
  padding: 25px;
  margin-bottom: 25px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
  border: 1px solid rgba(255, 255, 255, 0.18);
}

h1, h2, h3 {
  color: #fff;
  margin-bottom: 20px;
  text-shadow: 0 2px 4px rgba(0,0,0,0.2);
}

.btn {
  background: var(--primary);
  color: white;
  border: none;
  padding: 12px 25px;
  border-radius: 50px;
  cursor: pointer;
  font-size: 16px;
  font-weight: 600;
  transition: all 0.3s ease;
  box-shadow: 0 4px 15px rgba(67, 97, 238, 0.3);
}

.btn:hover {
  background: var(--secondary);
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(67, 97, 238, 0.4);
}

.btn-danger {
  background: var(--danger);
  box-shadow: 0 4px 15px rgba(247, 37, 133, 0.3);
}

.btn-danger:hover {
  background: #e11d74;
}

table {
  width: 100%;
  border-collapse: collapse;
  margin: 20px 0;
  background: rgba(255, 255, 255, 0.05);
  border-radius: 10px;
  overflow: hidden;
}

th, td {
  padding: 15px;
  text-align: left;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

th {
  background: rgba(255, 255, 255, 0.1);
  font-weight: 600;
}

tr:hover {
  background: rgba(255, 255, 255, 0.07);
}

input, select {
  width: 100%;
  padding: 14px;
  margin: 10px 0;
  border-radius: 10px;
  border: none;
  background: rgba(255, 255, 255, 0.1);
  color: white;
  font-size: 16px;
}

input:focus, select:focus {
  outline: none;
  background: rgba(255, 255, 255, 0.15);
  box-shadow: 0 0 0 3px rgba(67, 97, 238, 0.3);
}

.form-group {
  margin-bottom: 20px;
}

.status-card {
  text-align: center;
  padding: 30px;
}

.status-icon {
  font-size: 48px;
  margin-bottom: 15px;
  color: var(--success);
}

.scan-area {
  background: rgba(0, 0, 0, 0.2);
  padding: 30px;
  border-radius: 15px;
  text-align: center;
  margin-top: 20px;
}

#uidDisplay {
  font-size: 24px;
  font-weight: bold;
  letter-spacing: 3px;
  margin: 20px 0;
  background: rgba(0, 0, 0, 0.3);
  padding: 15px;
  border-radius: 10px;
  font-family: monospace;
}

.actions {
  display: flex;
  gap: 15px;
  flex-wrap: wrap;
}

.alert {
  padding: 15px;
  border-radius: 10px;
  margin: 15px 0;
}

.alert-warning {
  background: rgba(255, 193, 7, 0.2);
  border: 1px solid rgba(255, 193, 7, 0.3);
}

.alert-success {
  background: rgba(40, 167, 69, 0.2);
  border: 1px solid rgba(40, 167, 69, 0.3);
}

.glow {
  animation: glow 1.5s infinite alternate;
}

@keyframes glow {
  from { box-shadow: 0 0 5px rgba(67, 97, 238, 0.5); }
  to { box-shadow: 0 0 20px rgba(67, 97, 238, 0.8); }
}

.pulse {
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0% { transform: scale(1); }
  50% { transform: scale(1.05); }
  100% { transform: scale(1); }
}

.status-indicator {
  display: inline-block;
  width: 15px;
  height: 15px;
  border-radius: 50%;
  margin-right: 8px;
}

.status-online {
  background-color: #4CAF50;
  box-shadow: 0 0 10px #4CAF50;
}

.status-offline {
  background-color: #f44336;
}
)";

void showMessage(const String &msg) {
  matrix.displayClear();
  matrix.displayScroll(msg.c_str(), PA_LEFT, PA_SCROLL_LEFT, 50);
  while (!matrix.displayAnimate()) {
    delay(100);
  }
}

String readEEPROMString(int addr, int maxLen) {
  String s;
  for (int i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == '\0' || c == 0xFF) break;
    s += c;
  }
  return s;
}

void writeEEPROMString(int addr, const String &s, int maxLen) {
  int len = s.length();
  if (len > maxLen - 1) len = maxLen - 1;
  for (int i = 0; i < len; i++) EEPROM.write(addr + i, s[i]);
  EEPROM.write(addr + len, '\0');
  EEPROM.commit();
}

String getUIDString(byte *buffer, byte size) {
  String uid;
  for (byte i = 0; i < size; i++) {
    if (buffer[i] < 0x10) uid += "0";
    uid += String(buffer[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        webSocket.sendTXT(num, "connected");
      }
      break;
    case WStype_TEXT:
      if (strcmp((char*)payload, "startScan") == 0) {
        isScanning = true;
        lastScannedUID = "";
        Serial.println("WebSocket: Scan requested");
      }
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\n", num, length);
      break;
    case WStype_ERROR:
      Serial.printf("[%u] Error!\n", num);
      break;
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      Serial.printf("[%u] Fragment type: %d\n", num, type);
      break;
    case WStype_PING:
      Serial.printf("[%u] Ping received\n", num);
      break;
    case WStype_PONG:
      Serial.printf("[%u] Pong received\n", num);
      break;
    default:
      Serial.printf("[%u] Unhandled event type: %d\n", num, type);
      break;
  }
}

void handleRoot() {
  // Check authentication
  if (authUser.length() > 0 && authPass.length() > 0 && 
      !server.authenticate(authUser.c_str(), authPass.c_str())) {
    return server.requestAuthentication();
  }
  
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>MEDANTRIK Iron Lung Control</title>";
  html += "<style>" + String(uiStyles) + "</style>";
  html += "<script>";
  html += "function saveConfig(formId) {";
  html += "  const form = document.getElementById(formId);";
  html += "  fetch('/update', { method: 'POST', body: new FormData(form) })";
  html += "    .then(response => response.text())";
  html += "    .then(html => document.body.innerHTML = html);";
  html += "  return false;";
  html += "}";
  html += "let socket;";
  html += "function initWebSocket() {";
  html += "  socket = new WebSocket('ws://' + location.hostname + ':81/');";
  html += "  socket.onopen = function() {";
  html += "    console.log('WebSocket connected');";
  html += "    document.getElementById('ws-status').className = 'status-indicator status-online';";
  html += "  };";
  html += "  socket.onmessage = function(event) {";
  html += "    if (event.data === 'connected') {";
  html += "      return;";
  html += "    }";
  html += "    document.getElementById('uidDisplay').innerText = event.data;";
  html += "    document.getElementById('saveUid').value = event.data;";
  html += "    document.getElementById('scanArea').classList.add('glow');";
  html += "  };";
  html += "  socket.onclose = function() {";
  html += "    console.log('WebSocket disconnected');";
  html += "    document.getElementById('ws-status').className = 'status-indicator status-offline';";
  html += "    setTimeout(initWebSocket, 2000);"; // Reconnect after 2 seconds
  html += "  };";
  html += "}";
  html += "function startScan() {";
  html += "  document.getElementById('scanArea').style.display = 'block';";
  html += "  if (socket.readyState === WebSocket.OPEN) {";
  html += "    socket.send('startScan');";
  html += "  } else {";
  html += "    alert('WebSocket not connected. Try again.');";
  html += "  }";
  html += "}";
  html += "window.onload = function() {";
  html += "  initWebSocket();";
  html += "};";
  html += "</script></head><body>";
  html += "<div class='container'>";
  html += "<div class='card status-card'>";
  html += "<h1>MEDANTRIK Iron Lung Control</h1>";
  html += "<div class='status-icon'>🧬</div>";
  html += "<h3>Device ID: " + WiFi.macAddress() + "</h3>";
  html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
  html += "<p>System Status: <strong>Operational</strong> <span id='ws-status' class='status-indicator status-online'></span></p>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>Authorized Access Cards</h2>";
  html += "<table>";
  html += "<tr><th>Index</th><th>UID (hex)</th></tr>";
  for (int i = 0; i < UID_COUNT; i++) {
    String uidStr;
    for (int j = 0; j < UID_LENGTH; j++) {
      byte b = EEPROM.read(UID_DATA_ADDR + i * UID_LENGTH + j);
      if (b < 0x10) uidStr += "0";
      uidStr += String(b, HEX);
    }
    uidStr.toUpperCase();
    html += "<tr><td>" + String(i) + "</td><td>" + uidStr + "</td></tr>";
  }
  html += "</table>";
  html += "<button class='btn pulse' onclick='startScan()'>Scan New Card</button>";
  html += "<div id='scanArea' class='scan-area' style='display:none'>";
  html += "<h3>Scanning Card</h3>";
  html += "<div id='uidDisplay'>Place card on reader...</div>";
  html += "<form id='uidForm' onsubmit='return saveConfig(\"uidForm\")'>";
  html += "<input type='hidden' name='uid' id='saveUid'>";
  html += "Save to index: <select name='index'>";
  for (int i = 0; i < UID_COUNT; i++) {
    html += "<option value='" + String(i) + "'>" + String(i) + "</option>";
  }
  html += "</select><br><br>";
  html += "<button class='btn' type='submit'>Save UID</button>";
  html += "</form></div></div>";
  
  html += "<div class='card'>";
  html += "<h2>API Configuration</h2>";

  html += "<table>";
  html += "<tr><th>API</th><th>URL</th></tr>";
  html += "<tr><td>API 1</td><td>" + api1 + "</td></tr>";
  html += "<tr><td>API 2</td><td>" + api2 + "</td></tr>";
  html += "<tr><td>API 3</td><td>" + api3 + "</td></tr>";
  html += "</table><br>";

  html += "<form id='apiForm' onsubmit='return saveConfig(\"apiForm\")'>";
  html += "<div class='form-group'>";
  html += "<label>API Endpoint 1:</label>";
  html += "<input type='text' name='api1' value='" + api1 + "'>";
  html += "</div><div class='form-group'>";
  html += "<label>API Endpoint 2:</label>";
  html += "<input type='text' name='api2' value='" + api2 + "'>";
  html += "</div><div class='form-group'>";
  html += "<label>API Endpoint 3:</label>";
  html += "<input type='text' name='api3' value='" + api3 + "'>";
  html += "</div><button class='btn' type='submit'>Save API Settings</button>";
  html += "</form></div>";
  
  html += "<div class='card'>";
  html += "<h2>Security Settings</h2>";
  if (authUser.length() == 0) {
    html += "<div class='alert alert-warning'>";
    html += "<strong>Warning!</strong> Authentication not set!";
    html += "</div>";
  }
  html += "<form id='authForm' onsubmit='return saveConfig(\"authForm\")'>";
  html += "<div class='form-group'>";
  html += "<label>Username:</label>";
  html += "<input type='text' name='auth_user' value='" + authUser + "'>";
  html += "</div><div class='form-group'>";
  html += "<label>Password:</label>";
  html += "<input type='password' name='auth_pass' placeholder='Enter new password'>";
  html += "</div><button class='btn' type='submit'>Save Credentials</button>";
  html += "</form></div>";
  
  html += "<div class='card'>";
  html += "<h2>System Operations</h2>";
  html += "<div class='actions'>";
  html += "<button class='btn' onclick=\"fetch('/restart').then(() => alert('Restarting!'))\">Restart Device</button>";
  html += "<button class='btn btn-danger' onclick=\"if(confirm('Are you sure?')) fetch('/reset').then(() => alert('Resetting!'))\">Factory Reset</button>";
  html += "</div></div></div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  // Check authentication
  if (authUser.length() > 0 && authPass.length() > 0 && 
      !server.authenticate(authUser.c_str(), authPass.c_str())) {
    return server.requestAuthentication();
  }
  
  if (server.hasArg("index") && server.hasArg("uid")) {
    int idx = server.arg("index").toInt();
    String h = server.arg("uid");
    if (idx >= 0 && idx < UID_COUNT && h.length() == UID_LENGTH * 2) {
      for (int i = 0; i < UID_LENGTH; i++) {
        byte b = strtoul(h.substring(2 * i, 2 * i + 2).c_str(), NULL, 16);
        EEPROM.write(UID_DATA_ADDR + idx * UID_LENGTH + i, b);
      }
      EEPROM.commit();
      server.send(200, "text/html", "<div class='alert alert-success'>UID saved! Device will restart in 3 seconds...</div><script>setTimeout(() => location.href='/', 3000)</script>");
      delay(3000);
      ESP.restart();
      return;
    }
  }

  if (server.hasArg("api1") && server.hasArg("api2") && server.hasArg("api3")) {
    writeEEPROMString(API1_ADDR, server.arg("api1"), MAX_API_LEN);
    writeEEPROMString(API2_ADDR, server.arg("api2"), MAX_API_LEN);
    writeEEPROMString(API3_ADDR, server.arg("api3"), MAX_API_LEN);
    server.send(200, "text/html", "<div class='alert alert-success'>API endpoints updated! Device will restart in 3 seconds...</div><script>setTimeout(() => location.href='/', 3000)</script>");
    delay(3000);
    ESP.restart();
    return;
  }

  if (server.hasArg("auth_user") && server.hasArg("auth_pass")) {
    String newUser = server.arg("auth_user");
    String newPass = server.arg("auth_pass");
    
    if (newPass.length() > 0) {
      writeEEPROMString(AUTH_USER_ADDR, newUser, MAX_AUTH_LEN);
      writeEEPROMString(AUTH_PASS_ADDR, newPass, MAX_AUTH_LEN);
    } else if (newUser != authUser) {
      writeEEPROMString(AUTH_USER_ADDR, newUser, MAX_AUTH_LEN);
    }
    
    server.send(200, "text/html", "<div class='alert alert-success'>Credentials updated! Device will restart in 3 seconds...</div><script>setTimeout(() => location.href='/', 3000)</script>");
    delay(3000);
    ESP.restart();
    return;
  }

  server.send(400, "text/plain", "Invalid request");
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleReset() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  server.send(200, "text/plain", "Factory reset complete. Restarting...");
  delay(1000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting...");
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize RFID
  rfid.PCD_Init();
  
  // Initialize Matrix
  matrix.begin();
  matrix.setIntensity(5);
  matrix.displayClear();
  showMessage("Starting...");
  
  // Read stored data
  api1 = readEEPROMString(API1_ADDR, MAX_API_LEN);
  api2 = readEEPROMString(API2_ADDR, MAX_API_LEN);
  api3 = readEEPROMString(API3_ADDR, MAX_API_LEN);
  authUser = readEEPROMString(AUTH_USER_ADDR, MAX_AUTH_LEN);
  authPass = readEEPROMString(AUTH_PASS_ADDR, MAX_AUTH_LEN);
  
  // WiFiManager setup
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(30);
  
  // Start WiFiManager
  showMessage("Connect WiFi");
  if (!wm.autoConnect("IronLung-Setup", "medantrik")) {
    Serial.println("Failed to connect and hit timeout");
    showMessage("WiFi Failed");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  showMessage(WiFi.localIP().toString());
  delay(1000);
  
  // Set up server routes
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.on("/restart", handleRestart);
  server.on("/reset", handleReset);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");
}

void loop() {
  server.handleClient();
  webSocket.loop();  // Handle WebSocket events

  // Handle RFID scanning
  if (millis() - lastScanTime > scanCooldown) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastScanTime = millis();

      byte rawUID[UID_LENGTH];
      memcpy(rawUID, rfid.uid.uidByte, UID_LENGTH);
      String uid = getUIDString(rawUID, UID_LENGTH);
      Serial.println("Scanned UID: " + uid);

      // Send UID via WebSocket if scanning was requested
      if (isScanning) {
        lastScannedUID = uid;
        isScanning = false;
        webSocket.broadcastTXT(uid.c_str());
        Serial.println("UID sent via WebSocket: " + uid);
      }

      // Authentication logic via local server
      bool granted = false;
      String serverURL = "https://medantrik.in/ironlung/api/esp32.php" + uid;
      HTTPClient http;
      http.begin(serverURL);
      int httpCode = http.GET();

      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Server response: " + payload);
        // Grant access if response contains "GRANT"
        granted = payload.indexOf("GRANT") >= 0;
      } else {
        Serial.println("HTTP error code: " + String(httpCode));
      }
      http.end();

      // Fallback to master UID EEPROM check if not granted
      if (!granted) {
        for (int i = 0; i < UID_COUNT; i++) {
          bool match = true;
          for (int j = 0; j < UID_LENGTH; j++) {
            if (EEPROM.read(UID_DATA_ADDR + i * UID_LENGTH + j) != rawUID[j]) {
              match = false;
              break;
            }
          }
          if (match) {
            granted = true;
            Serial.println("Master card matched.");
            break;
          }
        }
      }

      Serial.println(granted ? "Access Granted" : "Access Denied");
      showMessage(granted ? "ACCESS OK" : "DENIED");

      rfid.PICC_HaltA();
    }
  }
}
