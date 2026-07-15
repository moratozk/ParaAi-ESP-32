// =========================================================================
// Firmware TCC - SmartPark IoT (Principal)
// Integra Sensores, Display ILI9341 Touch, Servo Motor e Firestore
// =========================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <ESP32Servo.h>

#include "Credenciais.h"
#include "Sensores.ino"
#include "DisplayUI.ino"

#define PIN_SERVO 2
// GPIO2 é "strapping pin" no boot do ESP32 (precisa estar LOW/flutuando).
// Como só escrevemos nele dentro do setup(), depois do boot, normalmente
// não dá problema - mas é o primeiro pino a suspeitar se o boot ficar
// instável com o servo conectado.
Servo catraca;
const int ANGULO_ABERTO  = 90;
const int ANGULO_FECHADO = 0;

const unsigned long WIFI_TIMEOUT_MS = 15000;
const unsigned long WIFI_RETRY_MS   = 10000;
bool firebaseConfigurado = false;
unsigned long ultimaTentativaWifi = 0;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool estadoVagaAnterior = false;

bool conectarWiFi(unsigned long timeoutMs);
void configurarFirebase();
void abrirCatraca(const char* origem);
void fecharCatraca(const char* origem);
void atualizarVagaFirestore(int numeroVaga, bool ocupada);

void setup() {
  Serial.begin(115200);

  catraca.setPeriodHertz(50);
  catraca.attach(PIN_SERVO, 500, 2400);
  catraca.write(ANGULO_FECHADO);
  Serial.println("[CATRACA] Servo inicializado e fechado (0 graus).");

  initSensores();

  initUI();
  atualizarStatusConexao("Status: Conectando ao WiFi...");

  WiFi.setTxPower(WIFI_POWER_8_5dBm); // reduz pico de corrente no boot (evita brownout/reset)
  if (conectarWiFi(WIFI_TIMEOUT_MS)) {
    configurarFirebase();
    atualizarStatusConexao("Status: ONLINE E SINCRONIZADO");
  } else {
    Serial.println("\n[WIFI] Nao conectou a tempo - seguindo em modo OFFLINE.");
    atualizarStatusConexao("Status: OFFLINE (sem WiFi)");
  }

  desenharEstadoVaga(false);
  Serial.println("TUDO PRONTO! A entrar no Loop Principal...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long agora = millis();
    if (agora - ultimaTentativaWifi >= WIFI_RETRY_MS) {
      ultimaTentativaWifi = agora;
      if (!firebaseConfigurado) {
        Serial.println("[WIFI] Tentando conectar em segundo plano...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      atualizarStatusConexao("Status: OFFLINE (sem WiFi)");
    }
  } else if (!firebaseConfigurado) {
    configurarFirebase();
    atualizarStatusConexao("Status: ONLINE E SINCRONIZADO");
  }

  if (Serial.available() > 0) {
    char caractereRecebido = Serial.read();
    if (caractereRecebido == 'A' || caractereRecebido == 'a') {
      abrirCatraca("SERIAL");
    } else if (caractereRecebido == 'F' || caractereRecebido == 'f') {
      fecharCatraca("SERIAL");
    }
  }

  int comandoToque = verificarToquePainel();
  if (comandoToque == 1) {
    abrirCatraca("TOUCH");
  } else if (comandoToque == 2) {
    fecharCatraca("TOUCH");
  }

  bool carroAtualmenteDetetado = verificarVagaOcupada();
  if (carroAtualmenteDetetado != estadoVagaAnterior) {
    estadoVagaAnterior = carroAtualmenteDetetado;
    desenharEstadoVaga(estadoVagaAnterior);
    atualizarVagaFirestore(1, estadoVagaAnterior);
  }

  delay(20); // ciclo rápido - touch e sensor já têm debounce próprio
}

void abrirCatraca(const char* origem) {
  catraca.write(ANGULO_ABERTO);

  String status = "Status: CATRACA ABERTA (";
  status += origem;
  status += ")";
  atualizarStatusConexao(status);

  Serial.print("[");
  Serial.print(origem);
  Serial.println("] Comando: ABRIR CATRACA");
}

void fecharCatraca(const char* origem) {
  catraca.write(ANGULO_FECHADO);

  if (WiFi.status() == WL_CONNECTED) {
    atualizarStatusConexao("Status: ONLINE E SINCRONIZADO");
  } else {
    atualizarStatusConexao("Status: OFFLINE (sem WiFi)");
  }

  Serial.print("[");
  Serial.print(origem);
  Serial.println("] Comando: FECHAR CATRACA");
}

bool conectarWiFi(unsigned long timeoutMs) {
  Serial.print("A ligar ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicio > timeoutMs) {
      return false;
    }
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi Conectado!");
  return true;
}

void configurarFirebase() {
  Serial.println("A iniciar o Firebase. Aguarde...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  fbdo.setResponseSize(1024);

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  firebaseConfigurado = true;
  Serial.println("Firebase OK!");
}

void atualizarVagaFirestore(int numeroVaga, bool ocupada) {
  if (!firebaseConfigurado || !Firebase.ready()) {
    Serial.println("[FIRESTORE] Ignorado - Firebase ainda nao esta pronto (modo offline).");
    return;
  }

  String caminhoDocumento = "vagas/" + String(numeroVaga);

  FirebaseJson content;
  content.set("fields/ocupada/booleanValue", ocupada);

  String updateMask = "ocupada";

  Serial.print("A enviar para o Firestore... ");
  if (Firebase.Firestore.patchDocument(&fbdo, PROJECT_ID, "", caminhoDocumento.c_str(), content.raw(), updateMask)) {
    Serial.println("SUCESSO!");
  } else {
    Serial.println("ERRO!");
    Serial.println(fbdo.errorReason());
  }
}