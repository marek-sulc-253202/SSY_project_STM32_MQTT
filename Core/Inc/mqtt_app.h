#ifndef MQTT_APP_H
#define MQTT_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ===============================
// ⚙️ KONFIGURACE (můžeš změnit)
// ===============================
#define MQTT_CLIENT_NAME   "STM32_CLIENT_01"
#define MQTT_SUB_TOPIC     "mqtt/4"
#define MQTT_PUB_TOPIC     "mqtt/4"

#define MQTT_RX_TOPIC_LEN   64
#define MQTT_RX_PAYLOAD_LEN 128

// ===============================
// 📦 VEŘEJNÉ PROMĚNNÉ
// ===============================

// Flag – nová zpráva připravena
extern volatile uint8_t mqtt_new_msg;

// ===============================
// 🔌 API FUNKCE
// ===============================

// Připojení k brokeru
void MQTT_Connect(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4);

// Odpojení
void MQTT_Disconnect(void);

// Odeslání zprávy
void MQTT_Publish(const char *topic, const char *msg);

// Zpracování (volat v main loop)
void MQTT_Process(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_APP_H
