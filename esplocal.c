#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/* ================= DEVICE INFO ================= */
const char* DEVICE_NAME = "Priya_esp";

/* ================= WIFI ================= */
const char* ssid = "Sorry";
const char* password = "aaaaaaaa";

/* ================= RELAY PINS ================= */
#define LIGHT_PIN  26
#define FAN_PIN    27
#define PUMP_PIN   12

/* ================= STATE ================= */
bool lightState = false;
bool fanState   = false;
bool pumpState  = false;

/* ================= SERVERS ================= */
WebServer server(80);
WebSocketsServer webSocket(81);

/* ================= APPLY COMMAND ================= */
void applyCommand(char c) {
  switch (c) {
    case '0': lightState = false; break;
    case '1': lightState = true;  break;
    case '2': fanState   = false; break;
    case '3': fanState   = true;  break;
    case '4': pumpState  = false; break;
    case '5': pumpState  = true;  break;
  }
}

/* ================= UPDATE RELAYS ================= */
void updateRelays() {
  digitalWrite(LIGHT_PIN, lightState ? HIGH : LOW);
  digitalWrite(FAN_PIN,   fanState   ? HIGH : LOW);
  digitalWrite(PUMP_PIN,  pumpState  ? HIGH : LOW);
}

/* ================= STATE STRING ================= */
String getStateString() {
  String s = "";
  s += (lightState ? "1" : "0");
  s += (fanState   ? "3" : "2");
  s += (pumpState  ? "5" : "4");
  return s;
}

/* ================= CLOUD UPDATE ================= */
void updateCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();   // Ignore SSL certificate

  HTTPClient https;

  Serial.println("🌐 Sending cloud update...");

  if (!https.begin(client, "https://internetprotocal.onrender.com/config")) {
    Serial.println("❌ HTTPS begin failed");
    return;
  }

  https.addHeader("Content-Type", "application/json");

  // ===== EXACT JSON FORMAT (as requested) =====
  String jsonBody;
  jsonBody.reserve(256);

  jsonBody =
    String("{\"deviceName\":\"") + DEVICE_NAME + "\"," +
    "\"wifiName\":\"" + String(ssid) + "\"," +
    "\"wifiPassword\":\"" + String(password) + "\"," +
    "\"deviceIp\":\"" + WiFi.localIP().toString() + "\"}";

  Serial.println("POST Body:");
  Serial.println(jsonBody);

  int httpCode = https.POST(jsonBody);

  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println("Server Response:");
    Serial.println(https.getString());
  }

  https.end();
}

/* ================= WEBSOCKET EVENT ================= */
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    for (size_t i = 0; i < length; i++) {
      applyCommand((char)payload[i]);
    }
    updateRelays();
    
    String state = getStateString();
webSocket.broadcastTXT(state);

  }
}

/* ================= HTTP HANDLER ================= */
void handleHttp() {
  String uri = server.uri();

  if (uri.startsWith("/setcmd/")) {
    String cmd = uri.substring(8);

    for (char c : cmd) {
      applyCommand(c);
    }

    updateRelays();
    String state = getStateString();
    webSocket.broadcastTXT(state);
    server.send(200, "text/plain", state);
    return;
  }

  server.send(404, "text/plain", "Not Found");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  updateRelays();

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.onNotFound(handleHttp);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // First cloud update
  updateCloud();
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();
  webSocket.loop();

  static unsigned long lastCloud = 0;
  if (millis() - lastCloud > 60000) {   // every 60 seconds
    updateCloud();
    lastCloud = millis();
  }
}
