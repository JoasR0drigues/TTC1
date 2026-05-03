#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ================= WIFI =================
#define WIFI_SSID "ZTE_8623"
#define WIFI_PASSWORD "58519725"

// ================= FIREBASE =================
// URL base do Realtime Database, sem .json no final
#define FIREBASE_HOST "https://tcc-prototipo-de-patrimonio-default-rtdb.firebaseio.com"

// ================= NTP =================
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-3 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

// ================= RFID / SPI (ESP32) =================
// RC522 SDA/SS -> GPIO 5
// RC522 SCK    -> GPIO 18
// RC522 MISO   -> GPIO 19
// RC522 MOSI   -> GPIO 23
// RC522 RST    -> GPIO 2
#define RFID_SS_PIN    5
#define RFID_RST_PIN   2
#define RFID_SCK_PIN   18
#define RFID_MISO_PIN  19
#define RFID_MOSI_PIN  23

// ================= ULTRASSÔNICO =================
#define TRIG_PIN 13
#define ECHO_PIN 12

// ================= LED =================
// Não use 23, pois 23 é MOSI do SPI
#define LED_PIN 25

// ================= CONSTANTES =================
const float DISTANCIA_LIMITE = 10.0;
const int NUM_LEITURAS = 3;
const unsigned long INTERVALO_LEITURA_RFID = 150;
const unsigned long INTERVALO_DISTANCIA = 1000;
const unsigned long INTERVALO_ENVIO_ULTRASSONICO = 3000;
const unsigned long INTERVALO_DEBUG = 2000;
const unsigned long TEMPO_BLOQUEIO_UID_REPETIDO = 3000;

// ================= OBJETOS =================
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
WebServer server(80);
WiFiClientSecure secureClient;

// ================= VARIÁVEIS =================
bool firebaseOK = false;
bool firebaseTestado = false;
bool rfidOK = false;

float distancia = 0.0;
bool objetoDetectado = false;

unsigned long ultimoPollRFID = 0;
unsigned long ultimoPollDistancia = 0;
unsigned long ultimoEnvioUltrassonico = 0;
unsigned long ultimoDebug = 0;
unsigned long tempoUltimoUID = 0;

String ultimoUIDLido = "";
String ultimoUID = "";
String ultimoNome = "";
String ultimoTipoTag = "desconhecida";
String ultimoPatrimonioNome = "";
String ultimoPatrimonioCodigo = "";

// ================= PROTÓTIPOS =================
float medirDistancia();
void piscarLED(int vezes, int tempo);
void inicializarRFID();
void lerRFID();

void handleRoot();
void handleDistancia();
void handleUltimoRFID();
void handleStatus();
void enviarCORS();
void handleOptions();

long obterTimestamp();

bool firebaseGetString(const String &path, String &result);
bool firebaseGetJson(const String &path, String &result);
bool firebasePostJSON(const String &path, const String &jsonPayload, String &response);
bool firebasePutJSON(const String &path, const String &jsonPayload, String &response);
bool firebasePutString(const String &path, const String &value, String &response);

String jsonEscape(const String &value);
String uidParaChave(const String &uid);

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  piscarLED(2, 150);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.print("\nConectando WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado! IP: " + WiFi.localIP().toString());
    piscarLED(1, 400);
  } else {
    Serial.println("\nFalha ao conectar no WiFi");
  }

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.print("Sincronizando hora");
  time_t now = time(nullptr);
  int tentativasHora = 0;

  while (now < 1000000000 && tentativasHora < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    tentativasHora++;
  }

  if (now >= 1000000000) {
    Serial.println(" OK!");
  } else {
    Serial.println(" falhou, usando fallback.");
  }

  secureClient.setInsecure();
  Serial.println("Firebase REST inicializado, aguardando teste...");

  inicializarRFID();

  server.on("/", handleRoot);
  server.on("/distancia", handleDistancia);
  server.on("/ultimo_rfid", handleUltimoRFID);
  server.on("/status", handleStatus);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/distancia", HTTP_OPTIONS, handleOptions);
  server.on("/ultimo_rfid", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.begin();

  Serial.println("Servidor web iniciado");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Acesse: http://");
    Serial.println(WiFi.localIP());
  }
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (!firebaseTestado && millis() > 5000 && WiFi.status() == WL_CONNECTED) {
    firebaseTestado = true;

    Serial.println("Testando Firebase...");
    String response;
    if (firebasePutString("/teste_conexao/status", "ESP32 conectado", response)) {
      firebaseOK = true;
      Serial.println("Firebase OK");
      piscarLED(2, 250);
    } else {
      Serial.println("Erro Firebase no teste inicial");
    }
  }

  if (millis() - ultimoPollDistancia >= INTERVALO_DISTANCIA) {
    ultimoPollDistancia = millis();
    distancia = medirDistancia();
    objetoDetectado = (distancia > 0 && distancia <= DISTANCIA_LIMITE);
  }

  if (firebaseOK && millis() - ultimoEnvioUltrassonico >= INTERVALO_ENVIO_ULTRASSONICO) {
    ultimoEnvioUltrassonico = millis();
    StaticJsonDocument<256> ultraDoc;
    ultraDoc["uid"] = "SENSOR_ULTRASSONICO";
    ultraDoc["uid_key"] = "SENSOR_ULTRASSONICO";
    ultraDoc["tipo_tag"] = "ultrassonico";
    ultraDoc["distancia"] = distancia;
    ultraDoc["objeto_detectado"] = objetoDetectado;
    ultraDoc["timestamp"] = obterTimestamp();

    String ultraPayload;
    serializeJson(ultraDoc, ultraPayload);
    String ultraResponse;

    if (!firebasePutJSON("/leituras_recentes/ultrassonico", ultraPayload, ultraResponse)) {
      Serial.println("Erro ao enviar distancia ultrassonica em tempo real.");
    }
  }

  if (millis() - ultimoDebug >= INTERVALO_DEBUG) {
    ultimoDebug = millis();
    Serial.print("Distancia: ");
    Serial.print(distancia);
    Serial.println(" cm");
  }

  if (rfidOK && millis() - ultimoPollRFID >= INTERVALO_LEITURA_RFID) {
    ultimoPollRFID = millis();
    lerRFID();
  }

  delay(10);
}

// ================= INICIALIZAÇÃO RFID =================
void inicializarRFID() {
  Serial.println("Inicializando RFID...");

  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  delay(50);

  mfrc522.PCD_Init();
  delay(50);

  byte versao = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

  Serial.print("Firmware Version: 0x");
  Serial.println(versao, HEX);

  if (versao == 0x00 || versao == 0xFF) {
    rfidOK = false;
    Serial.println("Falha na comunicacao com o RC522.");
    Serial.println("Confira: SS=5, SCK=18, MISO=19, MOSI=23, RST=2 e VCC em 3.3V.");
  } else {
    rfidOK = true;
    Serial.println("RFID pronto. Aproxime a tag.");
    piscarLED(3, 120);
  }
}

// ================= FIREBASE REST =================
bool firebaseGetString(const String &path, String &result) {
  HTTPClient https;
  String url = String(FIREBASE_HOST) + path + ".json";

  if (!https.begin(secureClient, url)) {
    Serial.println("Falha ao iniciar GET HTTPS");
    return false;
  }

  int httpCode = https.GET();
  if (httpCode <= 0) {
    Serial.print("GET falhou: ");
    Serial.println(https.errorToString(httpCode));
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("GET HTTP code: ");
    Serial.println(httpCode);
    Serial.println(payload);
    return false;
  }

  payload.trim();
  if (payload == "null") {
    return false;
  }

  if (payload.length() >= 2 && payload[0] == '"' && payload[payload.length() - 1] == '"') {
    payload.remove(payload.length() - 1, 1);
    payload.remove(0, 1);
  }

  result = payload;
  return true;
}

bool firebaseGetJson(const String &path, String &result) {
  HTTPClient https;
  String url = String(FIREBASE_HOST) + path + ".json";

  if (!https.begin(secureClient, url)) {
    Serial.println("Falha ao iniciar GET HTTPS (JSON)");
    return false;
  }

  int httpCode = https.GET();
  if (httpCode <= 0) {
    Serial.print("GET JSON falhou: ");
    Serial.println(https.errorToString(httpCode));
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("GET JSON HTTP code: ");
    Serial.println(httpCode);
    Serial.println(payload);
    return false;
  }

  payload.trim();
  if (payload == "null") {
    return false;
  }

  result = payload;
  return true;
}

bool firebasePostJSON(const String &path, const String &jsonPayload, String &response) {
  HTTPClient https;
  String url = String(FIREBASE_HOST) + path + ".json";

  if (!https.begin(secureClient, url)) {
    Serial.println("Falha ao iniciar POST HTTPS");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int httpCode = https.POST(jsonPayload);
  response = https.getString();
  https.end();

  if (httpCode <= 0) {
    Serial.print("POST falhou: ");
    Serial.println(https.errorToString(httpCode));
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("POST HTTP code: ");
    Serial.println(httpCode);
    Serial.println(response);
    return false;
  }

  return true;
}

bool firebasePutJSON(const String &path, const String &jsonPayload, String &response) {
  HTTPClient https;
  String url = String(FIREBASE_HOST) + path + ".json";

  if (!https.begin(secureClient, url)) {
    Serial.println("Falha ao iniciar PUT HTTPS");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int httpCode = https.PUT(jsonPayload);
  response = https.getString();
  https.end();

  if (httpCode <= 0) {
    Serial.print("PUT falhou: ");
    Serial.println(https.errorToString(httpCode));
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("PUT HTTP code: ");
    Serial.println(httpCode);
    Serial.println(response);
    return false;
  }

  return true;
}

bool firebasePutString(const String &path, const String &value, String &response) {
  String jsonPayload = "\"" + jsonEscape(value) + "\"";
  return firebasePutJSON(path, jsonPayload, response);
}

String jsonEscape(const String &value) {
  String escaped = value;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  escaped.replace("\t", "\\t");
  return escaped;
}

String uidParaChave(const String &uid) {
  String chave = uid;
  chave.trim();
  chave.toUpperCase();
  chave.replace(" ", "");
  return chave;
}

// ================= SENSOR ULTRASSÔNICO =================
float medirDistancia() {
  float soma = 0;
  int validas = 0;

  for (int i = 0; i < NUM_LEITURAS; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duracao > 0) {
      float d = duracao * 0.034 / 2.0;
      if (d > 0 && d < 400) {
        soma += d;
        validas++;
      }
    }

    delay(10);
  }

  return validas > 0 ? (soma / validas) : 0;
}

// ================= RFID =================
void lerRFID() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Erro ao ler a tag.");
    return;
  }

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) {
      uid += " ";
    }
  }
  uid.toUpperCase();

  if (uid == ultimoUIDLido && (millis() - tempoUltimoUID) < TEMPO_BLOQUEIO_UID_REPETIDO) {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  ultimoUIDLido = uid;
  tempoUltimoUID = millis();
  ultimoUID = uid;

  Serial.print("UID lido: ");
  Serial.println(uid);

  piscarLED(2, 100);

  String uidKey = uidParaChave(uid);
  String nome = "DESCONHECIDO";
  bool autorizado = false;

  String tipoTag = "desconhecida";
  String patrimonioId = "";
  String patrimonioNome = "";
  String patrimonioCodigo = "";
  bool patrimonioEncontrado = false;

  // 1) Tenta resolver como tag de patrimônio
  if (firebaseOK) {
    String tagJson;
    bool tagEncontrada = firebaseGetJson("/tags_patrimonio/" + uidKey, tagJson);
    if (!tagEncontrada) {
      // Compatibilidade com registros antigos (chave com espacos).
      tagEncontrada = firebaseGetJson("/tags_patrimonio/" + uid, tagJson);
    }

    if (tagEncontrada) {
      StaticJsonDocument<256> tagDoc;
      DeserializationError tagErr = deserializeJson(tagDoc, tagJson);

      if (!tagErr) {
        patrimonioId = tagDoc["patrimonio_id"] | "";
        patrimonioNome = tagDoc["patrimonio_nome"] | "";
        patrimonioCodigo = tagDoc["patrimonio_codigo"] | "";

        if (patrimonioId.length() > 0) {
          patrimonioEncontrado = true;
          tipoTag = "patrimonio";
          autorizado = true;
          nome = patrimonioNome.length() > 0 ? patrimonioNome : "PATRIMONIO";
        }
      }
    }

    // 2) Se não for patrimônio, tenta resolver como tag de usuário
    if (!patrimonioEncontrado) {
      String nomeLido;
      bool usuarioEncontrado = firebaseGetString("/usuarios/" + uidKey + "/nome", nomeLido);
      if (!usuarioEncontrado) {
        // Compatibilidade com registros antigos (chave com espacos).
        usuarioEncontrado = firebaseGetString("/usuarios/" + uid + "/nome", nomeLido);
      }

      if (usuarioEncontrado) {
        nome = nomeLido;
        autorizado = true;
        tipoTag = "usuario";
      } else {
        tipoTag = "desconhecida";
      }
    }
  }

  ultimoNome = nome;
  ultimoTipoTag = tipoTag;
  ultimoPatrimonioNome = patrimonioNome;
  ultimoPatrimonioCodigo = patrimonioCodigo;

  Serial.print("Tipo da tag: ");
  Serial.println(tipoTag);

  if (tipoTag == "patrimonio") {
    Serial.print("Patrimonio identificado: ");
    Serial.print(patrimonioNome);
    Serial.print(" / ");
    Serial.println(patrimonioCodigo);
    piscarLED(4, 80);
  } else if (tipoTag == "usuario") {
    Serial.print("Usuario identificado: ");
    Serial.println(nome);
    piscarLED(3, 120);
  } else {
    Serial.println("Tag nao cadastrada.");
    piscarLED(2, 250);
  }

  StaticJsonDocument<512> doc;
  doc["uid"] = uid;
  doc["uid_key"] = uidKey;
  doc["nome"] = nome;
  doc["autorizado"] = autorizado;
  doc["tipo_tag"] = tipoTag;
  doc["distancia"] = distancia;
  doc["objeto_detectado"] = objetoDetectado;
  doc["timestamp"] = obterTimestamp();
  doc["processado"] = false;

  if (tipoTag == "patrimonio") {
    doc["patrimonio_id"] = patrimonioId;
    doc["patrimonio_nome"] = patrimonioNome;
    doc["patrimonio_codigo"] = patrimonioCodigo;
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  if (firebaseOK) {
    String response;

    // Histórico bruto das leituras
    if (firebasePostJSON("/acessos", jsonPayload, response)) {
      Serial.println("Leitura registrada em /acessos.");
    } else {
      Serial.println("Erro ao registrar leitura em /acessos.");
    }

    // Última leitura para o app processar
    if (firebasePutJSON("/leituras_recentes/" + String(millis()), jsonPayload, response)) {
      Serial.println("Leitura enviada para /leituras_recentes.");
    } else {
      Serial.println("Erro ao salvar em /leituras_recentes.");
    }
  } else {
    Serial.println("Firebase indisponivel: leitura local apenas.");
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// ================= TEMPO =================
long obterTimestamp() {
  time_t now = time(nullptr);
  if (now < 1000000000) {
    return millis() / 1000;
  }
  return now;
}

// ================= LED =================
void piscarLED(int vezes, int tempo) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(tempo);
    digitalWrite(LED_PIN, LOW);
    delay(tempo);
  }
}

// ================= SERVIDOR WEB =================
void enviarCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  enviarCORS();
  server.send(204);
}

void handleRoot() {
  enviarCORS();
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Monitor RFID</title>";
  html += "<style>body{font-family:Arial;text-align:center;margin-top:50px;} .card{background:#f0f0f0;border-radius:10px;padding:20px;display:inline-block;} p{margin:8px 0;}</style>";
  html += "</head><body><div class='card'>";
  html += "<h1>Monitor Patrimonial RFID</h1>";
  html += "<p>Distancia atual: <strong><span id='dist'>--</span> cm</strong></p>";
  html += "<p>Ultima UID: <span id='uid'>--</span></p>";
  html += "<p>Tipo da tag: <span id='tipo'>--</span></p>";
  html += "<p>Nome: <span id='nome'>--</span></p>";
  html += "<p>Patrimonio: <span id='pat'>--</span></p>";
  html += "<script>";
  html += "setInterval(()=>{";
  html += "fetch('/distancia').then(r=>r.json()).then(d=>document.getElementById('dist').innerText=d.distancia);";
  html += "fetch('/ultimo_rfid').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('uid').innerText=d.uid;";
  html += "document.getElementById('tipo').innerText=d.tipo_tag;";
  html += "document.getElementById('nome').innerText=d.nome;";
  html += "document.getElementById('pat').innerText=(d.patrimonio_nome || '--') + ' ' + (d.patrimonio_codigo ? '(' + d.patrimonio_codigo + ')' : '');";
  html += "});";
  html += "},1000);";
  html += "</script>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleDistancia() {
  enviarCORS();
  String json = "{\"distancia\":" + String(distancia, 1) + "}";
  server.send(200, "application/json", json);
}

void handleUltimoRFID() {
  enviarCORS();
  String json = "{";
  json += "\"uid\":\"" + ultimoUID + "\",";
  json += "\"nome\":\"" + ultimoNome + "\",";
  json += "\"tipo_tag\":\"" + ultimoTipoTag + "\",";
  json += "\"patrimonio_nome\":\"" + ultimoPatrimonioNome + "\",";
  json += "\"patrimonio_codigo\":\"" + ultimoPatrimonioCodigo + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  enviarCORS();
  String json = "{\"status\":\"" + String(objetoDetectado ? "detectado" : "aguardando") + "\"}";
  server.send(200, "application/json", json);
}
