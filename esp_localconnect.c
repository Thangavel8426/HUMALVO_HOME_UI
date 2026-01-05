#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>

/* ================= DEVICE INFO ================= */
const char* DEVICE_NAME = "GESTURE_ESP";//CHANGE THE DEVICE NAME

/* ================= WIFI ================= */
const char* ssid = "Sorry";
const char* password = "aaaaaaaa";

/* ================= HOST SERVER ================= */
//change the ip according to the localserver
const char* HOST_SERVER = "http://192.168.2.52:7000";

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

/* ================= RELAY LOGIC ================= */
void applyCommand(char c) {
  if (c == '0') lightState = false;
  if (c == '1') lightState = true;
  if (c == '2') fanState   = false;
  if (c == '3') fanState   = true;
  if (c == '4') pumpState  = false;
  if (c == '5') pumpState  = true;
}

void updateRelays() {
  digitalWrite(LIGHT_PIN, lightState ? HIGH : LOW);
  digitalWrite(FAN_PIN,   fanState   ? HIGH : LOW);
  digitalWrite(PUMP_PIN,  pumpState  ? HIGH : LOW);
}

String getStateString() {
  return String(lightState ? "1" : "0") +
         String(fanState   ? "3" : "2") +
         String(pumpState  ? "5" : "4");
}

/* ================= REGISTER ================= */
bool registerToHost() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  HTTPClient http;

  String url = String(HOST_SERVER) + "/register";
  String body =
    "{\"name\":\"" + String(DEVICE_NAME) +
    "\",\"ip\":\"" + WiFi.localIP().toString() +
    "\",\"port\":80}";

  Serial.println("\n📡 REGISTER");
  Serial.println("URL : " + url);
  Serial.println("BODY: " + body);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  Serial.println("HTTP CODE: " + String(code));

  http.end();
  return (code == 200 || code == 201);
}

/* ================= HEARTBEAT ================= */
void sendAlivePing() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  String url = String(HOST_SERVER) + "/ping";
  String body = "{\"name\":\"" + String(DEVICE_NAME) + "\"}";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  Serial.println("💓 ALIVE → HTTP " + String(code));

  http.end();
}

/* ================= WEBSOCKET ================= */
void wsEvent(uint8_t, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    for (size_t i = 0; i < length; i++) applyCommand((char)payload[i]);
    updateRelays();

    String state = getStateString();
    webSocket.broadcastTXT(state);
  }
}

/* ================= HTTP ================= */
void handleHttp() {
  if (server.uri().startsWith("/setcmd/")) {
    String cmd = server.uri().substring(8);
    for (char c : cmd) applyCommand(c);
    updateRelays();

    String s = getStateString();
    webSocket.broadcastTXT(s);
    server.send(200, "text/plain", s);
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

  WiFi.begin(ssid, password);
  Serial.print("📶 Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected");
  Serial.println("ESP IP: " + WiFi.localIP().toString());

  server.onNotFound(handleHttp);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(wsEvent);
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();
  webSocket.loop();

  static unsigned long lastRegister = 0;
  static unsigned long lastAlive = 0;

  // 🔁 Keep registering until host accepts
  if (millis() - lastRegister > 10000) {
    registerToHost();
    lastRegister = millis();
  }

  // 💓 I'm alive
  if (millis() - lastAlive > 5000) {
    sendAlivePing();
    lastAlive = millis();
  }
}
