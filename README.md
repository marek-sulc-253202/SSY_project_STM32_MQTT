# STM32 MQTT + CLI + TFT Display Project + IR Proximity senzor

Tento projekt je založen na mikrokontroléru STM32 (HAL + LwIP) a kombinuje:
- UART CLI rozhraní
- MQTT komunikaci přes Ethernet
- IR proximity senzor (GPIO/EXTI)
- TFT displej ILI9341

---

## Funkce projektu

### CLI (UART konzole)
Zařízení obsahuje textové rozhraní přes UART (USART3), které umožňuje:

- konfiguraci IP adresy
- MQTT připojení a komunikaci
- testování sítě (ping)
- zapínání/vypínání senzoru

---

### MQTT komunikace
Používá knihovnu **lwIP MQTT client**.

Podporované operace:
- připojení k brokeru
- odpojení
- publish zpráv
- subscribe na topic

Defaultní nastavení:
- Client ID: `STM32_CLIENT_01`
- Subscribe topic: `mqtt/4`
- Publish topic: `mqtt/4`

---

### IR Proximity senzor
- Připojen na **PF14**
- Konfigurován jako **GPIO External Interrupt (EXTI)**
- Reaguje na změnu stavu (detekce objektu)

Funkce:
- při aktivaci posílá MQTT zprávu
- lze zapnout/vypnout přes CLI

CLI příkazy:

SENZOR ON

SENZOR OFF

---

### TFT Display (ILI9341)
Použit displej řízený přes SPI.

Funkce:
- vykreslování MQTT zpráv

Použitá knihovna:
- vlastní driver `ILI9341_STM32_Driver`
- grafická vrstva `ILI9341_GFX`

---

## 🧠 Architektura projektu

### Hlavní komponenty:

main.c

├── CLI (ssy_cli.c)

├── MQTT (mqtt_app.c)

├── Sensor (main.c)

└── Display (ILI9341_STM32_Driver.c + ILI9341_GFX.c + fonts.c)

---

## 🔌 CLI příkazy

### Síť

ip status

ip dhcp

ip 192 168 1 50

### Ping

ping 192 168 1 50


### MQTT

mqtt_connect 192 168 53 15

mqtt_disconnect

mqtt_send mqtt/4 Hello world

### Sensor

SENZOR ON

SENZOR OFF

---

## 📡 Sensor logika

IR senzor digitální výstup je připojen na GPIO PF14.

- využívá EXTI interrupt
- při změně stavu se volá callback
- pokud je senzor aktivní (`sensor_enabled = 1`), odesílá MQTT zprávu

Příklad:

DETECTED → MQTT publish "mqtt/4"


---

## ⚙️ CubeMX konfigurace

### PINY
Displej:

- PB5 (D22) → SPI_B_MOSI
- PB3 (D23) → SPI_B_SCK
- PB4 (D25) → SPI_B_MISO
- PF13 (D7) → GPIO_Output DC
- PE9  (D6) → GPIO_Output RESET
- PE11 (D5) → GPIO_Output CS

Senzor:
- PF14 (D4) → GPIO_EXTI14 Pull-up doporučen (dle senzoru)

### Periferie
- USART3 → CLI komunikace
- SPI_3 → TFT displej
- IR Proximity senzor
- ETH + LwIP → MQTT stack

---

## Foto zapojení
![Schéma systému](/zapojeni.jpg)

---

## 🚀 Možné rozšíření

- MQTT TLS (secure connection)
- RTOS (FreeRTOS)
- JSON messaging
- web server (HTTP debug)
- grafické UI na TFT
- filtrace senzoru (debounce)

---

## 👨‍💻 Autor

Marek Šulc ID:253202
