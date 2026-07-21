# Dateiro - Datalogger e visualización (ESP32-C3)

> [!NOTE]
> Documentacion en galego

**Dateiro** e un sistema de adquisicion, visualizacion e rexistro de datos (datalogger) deseñado arredor do microcontrolador **ESP32-C3 SuperMini**.
O dispositivo permite a medicion de voltaxe diferencial, corrente e voltaxe de bus, mostrando un grafico en tempo real (modo osciloscopio) nunha pantalla e almacenando os datos en ficheiros `.CSV`
dentro duna tarxeta MicroSD.

---

## Caracteristicas do Firmware

### 1. Gráfica en Tempo Real
- **Buffer Circular:** Mantén un historico de 128 puntos para debuxar a onda na pantalla OLED a intervalos de 50ms (refresh de 20 Hz).
- **Escalado e Grade:** Debuxo de grade de puntos para escala visual e axuste automatico de rango (hasta 16V en modo diferencial e ata 1000mA en modo INA219).

### 2. Datalogger Automático (Ficheiros CSV)
- **Nomenclatura Automática:** Crea ficheiros secuenciais no directorio raiz da tarxeta SD (`/DAT_001.CSV`, `/DAT_002.CSV`, etc.).
- **Modos de Mostraxe:**
  - **Por Intervalo:** Define o tempo entre mostras en segundos (ex: cada 5s).
  - **Por Cantidade de Puntos:** Distribue un numero fixo de puntos ao longo do tempo total configurado.
- **Formato do CSV:** Encabezado dinamico dependendo da orixe de datos (`Tempo_ms,Diferencial_V` ou `Tempo_ms,Corrente_mA`).

### 3. Navegación
- Interface baseada nun **Encoder Rotatorio** con descodificacion por matriz de estados (anti-rebotes por interruptor de hardware/software).
- **Xestion de Clicks:**
  - **Click Curto (>50ms):** Confirmar / Avanzar menu.
  - **Click Longo (>600ms):** Voltar / Cancelar / Deter gravacion.
  - **Dobre Click (<400ms):** Menu de apagado rapido.

### 4. Xestión de Enerxía e Deep Sleep
- **Lendo Batería:** Indicador de bateria con icono grafico na barra superior calculando o nivel mediante o pin `MVBAT` e detectando se esta en carga polo pin `STAT`.
- **Modo Apagado (Deep Sleep):** Apaga a pantalla OLED mediante comandos I2C (`0xAE`) e pon o ESP32-C3 en modo de soño profundo, incluida a OLED.
- **Despertado por Hardware:** Configura o pin do encoder (`PIN_ENC_B` / GPIO5) para espertar o sistema ao xirar o mando.

---

## Asignación de Pins (ESP32-C3 SuperMini)

| GPIO | Rede / Sinal | Descricion |
| :---: | :--- | :--- |
| **GPIO0** | `MOSI` | Bus SPI (Tarxeta SD) |
| **GPIO1** | `MVBAT` | Medicion de voltaxe da Bateria (Factor 1.816) |
| **GPIO2** | `MISO` | Bus SPI (Tarxeta SD) |
| **GPIO3** | `CS` | Chip Select SPI (Tarxeta SD) |
| **GPIO4** | `MEDV` | Entrada Analoxica Amplificada (LMC7101) |
| **GPIO5** | `ENC_B` | Encoder Rotatorio - Canle B (Wakeup GPIO) |
| **GPIO6** | `REG_EN` | Habilitacion do Regulador de Tensión |
| **GPIO7** | `ENC_A` | Encoder Rotatorio - Canle A |
| **GPIO8** | `STAT` | Estado da carga de Bateria (MCP73831) |
| **GPIO9** | `ENC_S` | Pulsador do Encoder Rotatorio |
| **GPIO10**| `CLK` | Reloxo Bus SPI (Tarxeta SD) |
| **GPIO20**| `SDA` | Bus I2C (OLED SH1106 + INA219) |
| **GPIO21**| `SCL` | Bus I2C (OLED SH1106 + INA219) |

---

## Librarías Requiridas

Para compilar este proxecto na IDE de Arduino ou PlatformIO, precísanse as seguintes librarías:

- `Wire.h` e `SPI.h` (Incluídas no core de ESP32)
- `SD.h` (Incluída no core de ESP32)
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit_SH110X](https://github.com/adafruit/Adafruit_SH110X) (Para pantallas OLED SH1106G)
- [Adafruit_INA219](https://github.com/adafruit/Adafruit_INA219)

---

## Bill of Materials (BOM)

| Qty | Value / Component | Footprint / Package | Designator | Description |
| :---: | :--- | :--- | :--- | :--- |
| **2** | 10uF | C0603 | `C1`, `C3` | Capacitor |
| **1** | 100nF | C0603 | `C2` | Capacitor |
| **1** | SS14 | SOD-323 | `D1` | Diodo Schottky |
| **1** | Conn_01x02 | PinHeader 1x02 (2.54mm) | `J1` | Conector |
| **2** | Conn_01x04 | PinHeader 1x04 (2.54mm) | `J2`, `J3` | Conector |
| **1** | S8050 | SOT-23 | `Q1` | Transistor NPN |
| **4** | 10k | R0603 | `R1`, `R2`, `R4`, `R5` | Resistencia |
| **1** | 1k | R0603 | `R3` | Resistencia |
| **1** | SW_Push | 4.5x4.5mm | `SW1` | Pulsador |
| **1** | ESP32-C3-SuperMini | Board / Module | `U1` | Modulo Microcontrolador ESP32-C3 |
| **1** | HX711 | Module | `U2` | Modulo Amplificador Celula de Carga |
| **1** | DS1307 | SOP-8 | `U3` | Reloxo en Tempo Real (RTC) |
| **1** | 32.768kHz | Crystal D2.0mm L6.0mm | `Y1` | Cristal Oscilador RTC |

---
Deseñado en Extrimia con agarimo.

Saudos.
2026