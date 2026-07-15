// =========================================================================
// MÓDULO DE SENSORES (Sensores.ino)
// Leitura do HC-SR04 com filtro anti-ruído (confirmação por N leituras)
// =========================================================================

#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>

// -------------------------------------------------------------------------
// PINOS DO SENSOR ULTRASSÓNICO HC-SR04
// -------------------------------------------------------------------------
// Mantidos em D14 (Trigger) e D12 (Echo) - não conflitam com os pinos da
// tela (SCK=18, MOSI=23, MISO=19, TFT_CS=17, TFT_DC=16, TOUCH_CS=21).
//
// ATENÇÃO: GPIO12 é um "strapping pin" (MTDI) - o ESP32 lê o nível dele no
// exato momento do boot/reset para decidir a voltagem da flash. Aqui ele é
// usado como ECHO (entrada) e o HC-SR04 mantém esse sinal em LOW quando não
// está medindo, então na prática raramente dá problema - mas se o boot
// ficar instável/reiniciando sozinho com o sensor conectado, este é o
// primeiro pino a testar trocar (ex: GPIO35, que também é só entrada e
// está livre).
const int PIN_TRIGGER = 14;
const int PIN_ECHO    = 12;

// Distância (cm) abaixo da qual consideramos a vaga ocupada
const int LIMITE_DISTANCIA_CM = 30;

// Timeout do pulso em microssegundos (~5m de alcance) - evita que o
// pulseIn() fique travado esperando um eco que nunca chega
const unsigned long TIMEOUT_PULSO_US = 30000UL;

// Quantas leituras seguidas e concordantes confirmam uma MUDANÇA de estado.
// Filtra ecos ruidosos/reflexos falsos sem precisar de delay() nenhum.
const int LEITURAS_PARA_CONFIRMAR = 3;

static bool estadoConfirmado    = false;
static bool ultimaLeituraBruta  = false;
static int  contadorConfirmacao = 0;

void initSensores() {
  pinMode(PIN_TRIGGER, OUTPUT);
  digitalWrite(PIN_TRIGGER, LOW);
  pinMode(PIN_ECHO, INPUT);
  Serial.println("[SENSORES] HC-SR04 inicializado (Trigger=D14, Echo=D12).");
}

// Uma leitura bruta do sensor (sem filtro). true = carro dentro do limite.
static bool lerDistanciaBruta() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  long duracao = pulseIn(PIN_ECHO, HIGH, TIMEOUT_PULSO_US);

  if (duracao == 0) {
    return false; // timeout - nenhum eco voltou, fora de alcance
  }

  int distancia = duracao * 0.034 / 2;

  if (distancia <= 0 || distancia > 400) {
    return false; // leitura fisicamente implausível, ignora
  }

  return (distancia <= LIMITE_DISTANCIA_CM);
}

// Função pública chamada no loop(): só muda o estado depois de
// LEITURAS_PARA_CONFIRMAR leituras seguidas concordando entre si. Isso
// evita que UM eco espúrio dispare uma atualização falsa no Firebase.
bool verificarVagaOcupada() {
  bool leituraAtual = lerDistanciaBruta();

  if (leituraAtual == ultimaLeituraBruta) {
    if (contadorConfirmacao < LEITURAS_PARA_CONFIRMAR) {
      contadorConfirmacao++;
    }
  } else {
    ultimaLeituraBruta = leituraAtual;
    contadorConfirmacao = 1;
  }

  if (contadorConfirmacao >= LEITURAS_PARA_CONFIRMAR) {
    estadoConfirmado = leituraAtual;
  }

  return estadoConfirmado;
}

#endif