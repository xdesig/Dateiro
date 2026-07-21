/*  Dateiro V002 (XDeSIG 2026)
sistema de adquisicion, visualizacion e rexistro de datos (datalogger) deseñado arredor do microcontrolador **ESP32-C3 SuperMini**. 
O dispositivo permite a medicion de voltaxe diferencial, corrente e voltaxe de bus, mostrando un grafico en tempo real (modo osciloscopio) nunha pantalla 
e almacenando os datos en ficheiros `.CSV` dentro duna tarxeta MicroSD.
- Xestion de Clicks:
  - Click Curto (>50ms): Confirmar / Avanzar menu.
  - Click Longo (>600ms): Voltar / Cancelar / Deter gravacion.
  - Dobre Click (<400ms): Menu de apagado rapido.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <SD.h>

// --- DEFINICIÓN DE PINS ---
#define PIN_SD_MOSI   0
#define PIN_MVBAT     1  
#define PIN_SD_MISO   2
#define PIN_SD_CS     3
#define PIN_MEDV      4  
#define PIN_ENC_B     5
#define PIN_REG_EN    6
#define PIN_ENC_A     7
#define PIN_STAT      8
#define PIN_ENC_S     9  
#define PIN_SD_CLK    10
#define PIN_SDA_I2C   20
#define PIN_SCL_I2C   21

#define OLED_ADDRESS  0x3C
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// --- CONSTANTES ---
const float V_OFFSET_ZERO = 0.021;
const float GANANCIA_REAL = 0.150;
const float FACTOR_CALIBRACION_BAT = 1.816;

// --- OBXECTOS ---
Adafruit_SH1106G pantalla = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_INA219 ina219(0x40);

enum EstadoApp {
  E_PRINCIPAL, E_MENU_ORIXE, E_MENU_HORAS, E_MENU_MINUTOS, 
  E_MENU_SEGUNDOS, E_MENU_METODO, E_MENU_VALOR, E_MENU_RESUMO, 
  E_GRAVANDO, E_FIN_GRAVACION, E_APAGAR
};
EstadoApp estadoActual = E_PRINCIPAL;

// --- VARIABLES DE CONFIGURACIÓN E CONTROL ---
int cfg_orixe = 0;   
int cfg_h = 0, cfg_m = 1, cfg_s = 0; 
int cfg_metodo = 0;  
int cfg_valor = 5;   

// --- VARIABLES DE GRAVACIÓN SD ---
File ficheiroDatos;
char nomeFicheiro[20];
uint32_t t_inicio_rec = 0;
uint32_t t_ultima_mostra = 0;
uint32_t intervalo_ms_rec = 0;
int max_puntos_rec = 0;
int puntos_gardados = 0;

// --- VARIABLES ENCODER ---
volatile int contadorEncoder = 0;
int ultimoEncoder = 0;

#define R_START     0x0
#define R_CW_FINAL  0x1
#define R_CW_BEGIN  0x2
#define R_CW_NEXT   0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT  0x6

const uint8_t ttable[7][4] = {
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},           
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | 0x10},    
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},           
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},           
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},           
  {R_CCW_FINAL, R_CCW_FINAL, R_START,     R_START | 0x20},    
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START}            
};
volatile uint8_t estadoEncoderSM = R_START;

// --- VARIABLES BOTÓN ---
uint32_t t_press = 0, t_release = 0;
bool estadoBotAnterior = HIGH, esperandoDobreClick = false;
bool clickCurto = false, clickLongo = false, dobreClick = false;

// --- BUFFER DO OSCILOSCOPIO ---
float historicoGrafica[128]; 
uint32_t ultimoRefrescoGrafica = 0;
const int INTERVALO_GRAFICA = 50; 

// --- INTERRUPCIÓN ENCODER ---
void IRAM_ATTR lerEncoder() {
  uint8_t pinstate = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  estadoEncoderSM = ttable[estadoEncoderSM & 0x0F][pinstate];
  uint8_t resultado = estadoEncoderSM & 0x30;
  
  if (resultado == 0x10) contadorEncoder--; 
  else if (resultado == 0x20) contadorEncoder++; 
}

// Pantalla de instruccións
void mostrarBenvida() {
  pantalla.clearDisplay();
  pantalla.setTextSize(1);
  pantalla.setTextColor(SH110X_WHITE);
  pantalla.setCursor(20, 0);
  pantalla.print("XDeSIG  DATADEIRO");
  pantalla.setCursor(0, 18);
  pantalla.print("Xirar: Mover o menu");
  pantalla.setCursor(0, 30);
  pantalla.print("Click: Confirmar / SI");
  pantalla.setCursor(0, 42);
  pantalla.print("Longo: Voltar / NON");
  pantalla.setCursor(0, 55);
  pantalla.print("Dobre Cli: Apagado");
  pantalla.display();
  delay(2000); 
}


void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_REG_EN, OUTPUT);
  digitalWrite(PIN_REG_EN, HIGH); 
  
  pinMode(PIN_STAT, INPUT_PULLDOWN);
  pinMode(PIN_ENC_S, INPUT); 
  pinMode(PIN_ENC_A, INPUT);
  pinMode(PIN_ENC_B, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), lerEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), lerEncoder, CHANGE);

  Wire.begin(PIN_SDA_I2C, PIN_SCL_I2C);
  Wire.setClock(400000);
  pantalla.begin(OLED_ADDRESS, false);
  ina219.begin(&Wire);

  SPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  SD.begin(PIN_SD_CS);

  for(int i=0; i<128; i++) historicoGrafica[i] = 0.0;

  mostrarBenvida();
  pantalla.clearDisplay();
  pantalla.setTextColor(SH110X_WHITE);
}

void lerBoton() {
  clickCurto = false; clickLongo = false; dobreClick = false;
  bool estadoBotActual = digitalRead(PIN_ENC_S);
  uint32_t agora = millis();

  if (estadoBotAnterior == HIGH && estadoBotActual == LOW) t_press = agora;
  
  if (estadoBotAnterior == LOW && estadoBotActual == HIGH) {
    t_release = agora;
    uint32_t duracion = t_release - t_press;

    if (duracion > 600) {
      clickLongo = true;
      esperandoDobreClick = false;
    } else if (duracion > 50) {
      if (esperandoDobreClick && (agora - t_press < 400)) {
        dobreClick = true;
        esperandoDobreClick = false;
      } else {
        esperandoDobreClick = true;
      }
    }
  }

  if (esperandoDobreClick && (agora - t_release > 250)) {
    clickCurto = true;
    esperandoDobreClick = false;
  }
  estadoBotAnterior = estadoBotActual;
}

void loop() {
  lerBoton();
  
  int deltaEncoder = contadorEncoder - ultimoEncoder;
  ultimoEncoder = contadorEncoder;

  if (estadoActual == E_PRINCIPAL || estadoActual == E_GRAVANDO) {
    if (millis() - ultimoRefrescoGrafica >= INTERVALO_GRAFICA) {
      ultimoRefrescoGrafica = millis();
      
      float novoValor = 0.0;
      if (cfg_orixe == 0) { 
        int rawMedv = analogRead(PIN_MEDV);
        float vMedvLocal = (rawMedv * 3.3) / 4095.0;
        novoValor = (vMedvLocal - V_OFFSET_ZERO) / GANANCIA_REAL; 
        if(novoValor < 0) novoValor = 0;
      } else { 
        novoValor = ina219.getCurrent_mA();
        if (isnan(novoValor)) novoValor = 0.0;
      }

      for (int i = 0; i < 127; i++) {
        historicoGrafica[i] = historicoGrafica[i + 1];
      }
      historicoGrafica[127] = novoValor;
    }
  }

  if (estadoActual == E_GRAVANDO) {
    if (millis() - t_ultima_mostra >= intervalo_ms_rec) {
      t_ultima_mostra = millis();
      
      float valorActual = historicoGrafica[127];
      if (ficheiroDatos) {
        ficheiroDatos.printf("%lu,%.3f\n", millis() - t_inicio_rec, valorActual);
        ficheiroDatos.flush(); 
      }
      
      puntos_gardados++;
      
      if (puntos_gardados >= max_puntos_rec) {
        if (ficheiroDatos) ficheiroDatos.close();
        estadoActual = E_FIN_GRAVACION;
      }
    }
  }

  switch (estadoActual) {
    case E_PRINCIPAL:
      if (clickCurto) estadoActual = E_MENU_ORIXE;
      if (dobreClick) estadoActual = E_APAGAR;
      break;

    case E_MENU_ORIXE:
      if (deltaEncoder > 0) cfg_orixe = 1;
      if (deltaEncoder < 0) cfg_orixe = 0;
      if (clickCurto) estadoActual = E_MENU_HORAS;
      if (clickLongo) estadoActual = E_PRINCIPAL;
      break;

    case E_MENU_HORAS:
      if (deltaEncoder != 0) {
        cfg_h += (deltaEncoder > 0) ? 1 : -1;
        if(cfg_h < 0) cfg_h = 0; if(cfg_h > 23) cfg_h = 23;
      }
      if (clickCurto) estadoActual = E_MENU_MINUTOS;
      if (clickLongo) estadoActual = E_PRINCIPAL;
      break;

    case E_MENU_MINUTOS:
      if (deltaEncoder != 0) {
        cfg_m += (deltaEncoder > 0) ? 1 : -1;
        if(cfg_m < 0) cfg_m = 0; if(cfg_m > 59) cfg_m = 59;
      }
      if (clickCurto) estadoActual = E_MENU_SEGUNDOS;
      if (clickLongo) estadoActual = E_MENU_HORAS;
      break;

    case E_MENU_SEGUNDOS:
      if (deltaEncoder != 0) {
        cfg_s += (deltaEncoder > 0) ? 1 : -1;
        if(cfg_s < 0) cfg_s = 0; if(cfg_s > 59) cfg_s = 59;
      }
      if (clickCurto) estadoActual = E_MENU_METODO;
      if (clickLongo) estadoActual = E_MENU_MINUTOS;
      break;

    case E_MENU_METODO:
      if (deltaEncoder > 0) cfg_metodo = 1;
      if (deltaEncoder < 0) cfg_metodo = 0;
      if (clickCurto) estadoActual = E_MENU_VALOR;
      if (clickLongo) estadoActual = E_MENU_SEGUNDOS;
      break;

    case E_MENU_VALOR:
      if (deltaEncoder != 0) {
        cfg_valor += (deltaEncoder > 0) ? 1 : -1;
        if(cfg_valor < 1) cfg_valor = 1;
      }
      if (clickCurto) estadoActual = E_MENU_RESUMO;
      if (clickLongo) estadoActual = E_MENU_METODO;
      break;

    case E_MENU_RESUMO:
      if (clickCurto) {
        if (!SD.begin(PIN_SD_CS)) {
          pantalla.clearDisplay();
          pantalla.drawRect(0, 10, 128, 44, SH110X_WHITE);
          pantalla.setCursor(12, 28);
          pantalla.print("ERRO: NON HAI SD");
          pantalla.display();
          delay(2000); 
          break; 
        }

        int total_segs = cfg_h * 3600 + cfg_m * 60 + cfg_s;
        if (cfg_metodo == 0) { 
          intervalo_ms_rec = cfg_valor * 1000UL;
          max_puntos_rec = (cfg_valor > 0) ? (total_segs / cfg_valor) : 0;
        } else { 
          max_puntos_rec = cfg_valor;
          intervalo_ms_rec = (cfg_valor > 0) ? ((total_segs * 1000UL) / cfg_valor) : 0;
        }

        int num = 1;
        snprintf(nomeFicheiro, sizeof(nomeFicheiro), "/DAT_%03d.CSV", num);
        while(SD.exists(nomeFicheiro) && num < 999) {
          num++;
          snprintf(nomeFicheiro, sizeof(nomeFicheiro), "/DAT_%03d.CSV", num);
        }

        ficheiroDatos = SD.open(nomeFicheiro, FILE_WRITE);
        if (ficheiroDatos) {
          ficheiroDatos.println(cfg_orixe == 0 ? "Tempo_ms,Diferencial_V" : "Tempo_ms,Corrente_mA");
        }

        puntos_gardados = 0;
        t_inicio_rec = millis();
        t_ultima_mostra = millis() - intervalo_ms_rec; 
        
        estadoActual = E_GRAVANDO;
      }
      if (clickLongo) estadoActual = E_MENU_VALOR;
      break;

    case E_GRAVANDO:
      if (clickLongo) {
        if (ficheiroDatos) ficheiroDatos.close();
        estadoActual = E_PRINCIPAL;
      }
      break;
      
    case E_FIN_GRAVACION:
      if (clickCurto || clickLongo) estadoActual = E_PRINCIPAL;
      break;

case E_APAGAR:
      if (clickCurto) {
        pantalla.clearDisplay();
        pantalla.setCursor(30, 20); 
        pantalla.print(" A Durmir!");
        pantalla.setCursor(5, 50); 
        pantalla.print("Rotar para despertar");
        pantalla.display();
        delay(3000);
        pantalla.clearDisplay();
        pantalla.display();
      
      // Enviamos o comando de apagado do xerador de tensión da pantalla
      Wire.beginTransmission(0x3C); 
      Wire.write(0x00); // 0x00 indica ao SH1106 que o seguinte é un comando
      Wire.write(0xAE); // Comando Display OFF
      Wire.endTransmission();
        
        // Creamos a máscara de bits para o GPIO 5 (1ULL << 5)
        uint64_t pin_mask = 1ULL << PIN_ENC_B; 
        
        // Activamos o espertar forzando o tipo de dato correcto para (LOW)
        // esperta o xirar o encoder
        esp_deep_sleep_enable_gpio_wakeup(pin_mask, (esp_deepsleep_gpio_wake_up_mode_t)0);

        // Entramos en Deep Sleep
        esp_deep_sleep_start();
      }
      if (clickLongo) estadoActual = E_PRINCIPAL;
      break;
  }

  debuxarPantalla();
  delay(5); 
}

// *** Icono Bateria ***
void debuxarBateriaPequena(int x, int y) {
  int rawBat = analogRead(PIN_MVBAT);
  float vBat = ((rawBat * 3.3) / 4095.0) * FACTOR_CALIBRACION_BAT;
  bool cargando = (digitalRead(PIN_STAT) == LOW);
  
  pantalla.drawRect(x, y, 12, 7, SH110X_WHITE);
  pantalla.fillRect(x+12, y+2, 1, 3, SH110X_WHITE);
  
  int nivel = map(vBat * 100, 340, 420, 0, 10);
  if (nivel < 0) nivel = 0; if (nivel > 10) nivel = 10;
  pantalla.fillRect(x+1, y+1, nivel, 5, SH110X_WHITE);
  
  if (cargando) {  
    pantalla.setCursor(x - 7, y - 1); 
    pantalla.print("+"); 
  }
}

void debuxarPantalla() {
  pantalla.clearDisplay();
  pantalla.setTextSize(1);
  
  if (estadoActual != E_APAGAR) debuxarBateriaPequena(114, 2);

  int total_segs = cfg_h * 3600 + cfg_m * 60 + cfg_s;
  int puntos_finais = (cfg_metodo == 0) ? ((cfg_valor > 0) ? (total_segs / cfg_valor) : 0) : cfg_valor;
  float intervalo_final = (cfg_metodo == 0) ? cfg_valor : ((cfg_valor > 0) ? ((float)total_segs / cfg_valor) : 0.0);
  float peso_estimado_kb = (puntos_finais * 25) / 1024.0; 

  switch (estadoActual) {
    
    case E_PRINCIPAL:
    case E_GRAVANDO:
     
     // --- 1. INFO SUPERIOR  ---
      pantalla.setTextSize(1);
      pantalla.setTextColor(SH110X_WHITE);
      
      if (cfg_orixe == 0) {
        float vActual = historicoGrafica[127];
        pantalla.setCursor(0, 0); 
        pantalla.printf("DIF:%.2fV", vActual);
        
        // Grade de puntos (cada 8px en horizontal)
        for (int v = 2; v <= 14; v += 2) {
          int yGrid = 63 - map(v * 100, 0, 1600, 0, 50); 
          for (int x = 0; x < 128; x += 8) pantalla.drawPixel(x, yGrid, SH110X_WHITE);
        }
        
        
        for (int i = 0; i < 127; i++) {
          int y1 = 63 - map(constrain(historicoGrafica[i], 0, 16) * 100, 0, 1600, 0, 50);
          int y2 = 63 - map(constrain(historicoGrafica[i+1], 0, 16) * 100, 0, 1600, 0, 50);
          pantalla.drawLine(i, y1, i+1, y2, SH110X_WHITE);
        }
      } 
      // --- 2. FONTE: SENSOR INA219 ---
      else {
        float vBus = ina219.getBusVoltage_V();
        float mAActual = historicoGrafica[127];
        pantalla.setCursor(0, 2); 
        pantalla.printf("%.1fV %.1fmA", vBus, mAActual);
        
        // Grade de puntos para INA219
        for (int ma = 200; ma <= 800; ma += 200) {
          int yGrid = 63 - map(ma, 0, 1000, 0, 50);
          for (int x = 0; x < 128; x += 8) pantalla.drawPixel(x, yGrid, SH110X_WHITE);
        }
        
        for (int i = 0; i < 127; i++) {
          int y1 = 63 - map(constrain(historicoGrafica[i], 0, 1000), 0, 1000, 0, 50);
          int y2 = 63 - map(constrain(historicoGrafica[i+1], 0, 1000), 0, 1000, 0, 50);
          pantalla.drawLine(i, y1, i+1, y2, SH110X_WHITE);
        }
      }
      
      // --- 3. INFORMACIÓN DE GRAVACIÓN (ZONA SUPERIOR CENTRAL) ---
      if (estadoActual == E_GRAVANDO) {
        
        // "REC" parpadea
        if ((millis() / 500) % 2 == 0) {
          pantalla.setCursor(0, 10);
          pantalla.print("REC");
        }
        
        // O progreso 
        pantalla.setCursor(66, 0);
        pantalla.printf("%d/%d", puntos_gardados, max_puntos_rec);
      }
      break;

// *** Textos dos Menús ***

    case E_MENU_ORIXE:
      pantalla.setCursor(0,0); pantalla.print("1. Sensor");
      pantalla.setCursor(10, 22); pantalla.printf("%s Amp diferencial", (cfg_orixe == 0) ? ">" : " ");
      pantalla.setCursor(10, 36); pantalla.printf("%s Sens. INA219", (cfg_orixe == 1) ? ">" : " ");
      break;

    case E_MENU_HORAS:
    case E_MENU_MINUTOS:
    case E_MENU_SEGUNDOS:
      pantalla.setCursor(0,0); pantalla.print("2. Tempo total");
      pantalla.setTextSize(2);
      pantalla.setCursor(16, 26);
      pantalla.printf("%02d:%02d:%02d", cfg_h, cfg_m, cfg_s);
      pantalla.setTextSize(1); 
      if(estadoActual == E_MENU_HORAS)   pantalla.drawFastHLine(16, 43, 22, SH110X_WHITE);
      if(estadoActual == E_MENU_MINUTOS) pantalla.drawFastHLine(52, 43, 22, SH110X_WHITE);
      if(estadoActual == E_MENU_SEGUNDOS) pantalla.drawFastHLine(88, 43, 22, SH110X_WHITE);
      break;

    case E_MENU_METODO:
      pantalla.setCursor(0,0); pantalla.print("3. Mostraxe x ");
      pantalla.setCursor(15, 24); pantalla.printf("%s Intervalo", (cfg_metodo == 0) ? ">" : " ");
      pantalla.setCursor(15, 38); pantalla.printf("%s Puntos", (cfg_metodo == 1) ? ">" : " ");
      break;

    case E_MENU_VALOR:
      pantalla.setCursor(0,0); pantalla.print(cfg_metodo == 0 ? "4. Intervalo" : "4. Puntos");
      pantalla.setTextSize(2);
      pantalla.setCursor(30, 24); pantalla.printf("%d %s", cfg_valor, (cfg_metodo == 0) ? "Seg" : "Pts");
      pantalla.setTextSize(1);
      break;

    case E_MENU_RESUMO:
      pantalla.setCursor(0,0);  pantalla.print("5. Resumo");
      pantalla.setCursor(0,10); pantalla.printf("Fonte:  %s", (cfg_orixe == 0) ? "Amp Diferenc." : "INA219");
      pantalla.setCursor(0,19); pantalla.printf("Tempo:  %02d:%02d:%02d", cfg_h, cfg_m, cfg_s);
      pantalla.setCursor(0,28); pantalla.printf("Interv: %.1f Seg", intervalo_final);
      pantalla.setCursor(0,37); pantalla.printf("Puntos: %d", puntos_finais);
      pantalla.setCursor(0,46); pantalla.printf("Tamanno:%.2f KB", peso_estimado_kb);
      pantalla.setCursor(0,55); pantalla.print("> CLICK CONFIRMA");
      break;

    // *** PANTALLA FIN DE GRAVACIÓN ***
    case E_FIN_GRAVACION:
      pantalla.drawRoundRect(2, 2, 124, 60, 4, SH110X_WHITE); 
      pantalla.setCursor(16, 8); pantalla.print("CAPTURA REMATADA");
      pantalla.drawFastHLine(16, 17, 96, SH110X_WHITE);
      
      pantalla.setCursor(10, 26); 
      pantalla.printf("Fich: %s", nomeFicheiro + 1);
      pantalla.setCursor(10, 38); 
      pantalla.printf("Pts Gardados: %d", puntos_gardados);
      
      pantalla.fillRect(10, 48, 108, 12, SH110X_WHITE);
      pantalla.setTextColor(SH110X_BLACK);
      pantalla.setCursor(15, 50); pantalla.print("> CLICK PARA SAIR");
      pantalla.setTextColor(SH110X_WHITE); 
      break;

    case E_APAGAR:
      pantalla.setCursor(10, 15); pantalla.print(" Apagar sistema?");
      pantalla.setCursor(0, 42); pantalla.print("[Click] Si [Long] Non");
      break;
  }
  pantalla.display();
}
