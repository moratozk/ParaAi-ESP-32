// =========================================================================
// MÓDULO DE INTERFACE GRÁFICA COM TOUCH (DisplayUI.ino)
// Controlo do Display ILI9341 (2.4"/2.8") + Touch resistivo XPT2046
// =========================================================================

#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// -------------------------------------------------------------------------
// PINOS DA TELA + TOUCH
// -------------------------------------------------------------------------
// Barramento SPI compartilhado entre tela e touch (VSPI padrão do ESP32).
// Se a sua fiação física usar pinos diferentes da referência que você me
// mandou, ajuste só os números abaixo - o resto do código não muda.
#define TFT_SCLK   18   // SCK  -> "SCK" do módulo
#define TFT_MOSI   23   // MOSI -> "SDI(MOSI)" do módulo
#define TFT_MISO   19   // MISO -> "SDO(MISO)" do módulo

#define TFT_CS     17   // CS exclusivo da tela  -> "CS" do módulo
#define TFT_DC     16   // Data/Command          -> "DC" do módulo
#define TFT_RST    -1   // RST do módulo ligado direto no 3.3V (sem GPIO)

#define TOUCH_CS   21   // CS exclusivo do touch -> "T_CS" do módulo
// T_IRQ do módulo não é usado neste firmware (leitura por polling via
// ts.touched()). Não afeta o funcionamento; se um dia quiser detecção por
// interrupção, ligue T_IRQ a um GPIO livre e ajuste o construtor do touch.

// 40MHz já é o padrão da biblioteca no ESP32; deixamos explícito aqui só
// para ficar fácil de reduzir (ex: 20000000 ou 10000000) se aparecer
// ruído/glitch na imagem - comum quando os fios (jumpers) são longos.
#define TFT_SPI_FREQ 40000000UL

SPIClass spiTela(VSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&spiTela, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);

// Coordenadas dos botões virtuais (tela 320x240, rotação 3 - paisagem)
#define BTN_Y        140
#define BTN_W        120
#define BTN_H        40
#define BTN_ABRIR_X  20
#define BTN_FECHAR_X 180

// Calibração do touch (valores brutos do XPT2046). Se os toques caírem
// deslocados dos botões, rode um sketch de calibração e ajuste estes 4.
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3700

const unsigned long TOQUE_COOLDOWN_MS = 400; // evita repetir comando com o dedo parado na tela
static unsigned long ultimoToqueMillis = 0;

void desenharBotoesTeste() {
  tft.fillRoundRect(BTN_ABRIR_X, BTN_Y, BTN_W, BTN_H, 6, ILI9341_DARKGREEN);
  tft.drawRoundRect(BTN_ABRIR_X, BTN_Y, BTN_W, BTN_H, 6, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BTN_ABRIR_X + 25, BTN_Y + 12);
  tft.print("ABRIR");

  tft.fillRoundRect(BTN_FECHAR_X, BTN_Y, BTN_W, BTN_H, 6, ILI9341_RED);
  tft.drawRoundRect(BTN_FECHAR_X, BTN_Y, BTN_W, BTN_H, 6, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BTN_FECHAR_X + 20, BTN_Y + 12);
  tft.print("FECHAR");
}

void initUI() {
  spiTela.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_FREQ);
  tft.setRotation(3);

  ts.begin(spiTela);
  ts.setRotation(3);

  tft.fillScreen(ILI9341_BLACK);

  tft.fillRect(0, 0, 320, 35, ILI9341_NAVY);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(45, 10);
  tft.print("SMARTPARK TCC - IoT");

  desenharBotoesTeste();

  tft.fillRect(0, 215, 320, 25, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 223);
  tft.print("Status: Conectando NUVEM...");

  Serial.println("[UI] Display e Touch XPT2046 inicializados.");
}

void atualizarStatusConexao(String statusTexto) {
  static String ultimoStatus = "";
  if (statusTexto == ultimoStatus) return; // já está escrito, evita redesenhar à toa
  ultimoStatus = statusTexto;

  tft.fillRect(0, 215, 320, 25, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 223);
  tft.print(statusTexto);
}

void desenharEstadoVaga(bool ocupada) {
  static int ultimoEstado = -1; // -1 = nunca desenhado ainda
  if ((int)ocupada == ultimoEstado) return;
  ultimoEstado = (int)ocupada;

  tft.fillRect(0, 40, 320, 90, ILI9341_BLACK);

  if (ocupada) {
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(3);
    tft.setCursor(50, 55);
    tft.print("VAGA OCUPADA");

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(50, 100);
    tft.print("Sincronizando com Firestore...");
  } else {
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(3);
    tft.setCursor(70, 55);
    tft.print("VAGA LIVRE");

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(75, 100);
    tft.print("Aguardando aproximacao...");
  }
}

// Retorna: 1 = botão ABRIR, 2 = botão FECHAR, 0 = nenhum toque válido
int verificarToquePainel() {
  if (!ts.touched()) return 0;

  unsigned long agora = millis();
  if (agora - ultimoToqueMillis < TOQUE_COOLDOWN_MS) {
    return 0; // ainda em cooldown do último toque - retorna na hora, sem travar o loop
  }

  TS_Point p = ts.getPoint();
  int toqueX = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320);
  int toqueY = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240);

  int comando = 0;
  if (toqueX >= BTN_ABRIR_X && toqueX <= (BTN_ABRIR_X + BTN_W) &&
      toqueY >= BTN_Y && toqueY <= (BTN_Y + BTN_H)) {
    comando = 1;
  } else if (toqueX >= BTN_FECHAR_X && toqueX <= (BTN_FECHAR_X + BTN_W) &&
             toqueY >= BTN_Y && toqueY <= (BTN_Y + BTN_H)) {
    comando = 2;
  }

  if (comando != 0) {
    ultimoToqueMillis = agora;
  }

  return comando;
}

#endif