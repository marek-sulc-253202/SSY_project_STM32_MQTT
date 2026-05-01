#include "mqtt_app.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include <stdio.h>
#include "lwip/apps/mqtt.h"
#include "lwip/err.h"

// displej includy
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"

extern void CLI_UART_Printf(const char *format, ...);

// ===============================
// 📦 PROMĚNNÉ
// ===============================
static mqtt_client_t *client = NULL;

static char rx_topic[MQTT_RX_TOPIC_LEN];
static char rx_payload[MQTT_RX_PAYLOAD_LEN];

static uint16_t rx_len = 0;
volatile uint8_t mqtt_new_msg = 0;

// ===============================
// 📤 PUBLISH CALLBACK
// ===============================
static void mqtt_pub_cb(void *arg, err_t result) {
    CLI_UART_Printf("MQTT PUB result: %d\r\n", result);
}

// ===============================
// 📥 RX - TOPIC
// ===============================
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {

    if (topic == NULL) return;

    // bezpečné kopírování topicu
    strncpy(rx_topic, topic, MQTT_RX_TOPIC_LEN - 1);
    rx_topic[MQTT_RX_TOPIC_LEN - 1] = '\0';

    rx_len = 0;

    CLI_UART_Printf("\r\n[MQTT RX] Topic: %s, len=%lu\r\n", topic, tot_len);
}

// ===============================
// 📥 RX - DATA
// ===============================
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {

    if (data == NULL || len == 0) return;

    // ochrana bufferu
    if (rx_len + len > MQTT_RX_PAYLOAD_LEN - 1) {
        len = MQTT_RX_PAYLOAD_LEN - 1 - rx_len;
    }

    memcpy(&rx_payload[rx_len], data, len);
    rx_len += len;

    // poslední fragment
    if (flags & MQTT_DATA_FLAG_LAST) {
        rx_payload[rx_len] = '\0';
        mqtt_new_msg = 1;
    }
}

// ===============================
// 📡 SUB CALLBACK
// ===============================
static void mqtt_sub_cb(void *arg, err_t result) {
    CLI_UART_Printf("MQTT SUB result: %d\r\n", result);
}

// ===============================
// 🔌 CONNECTION CALLBACK
// ===============================
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {

    if (status == MQTT_CONNECT_ACCEPTED) {

        CLI_UART_Printf("MQTT: CONNECTED\r\n");

        // nastavení RX callbacků (DŮLEŽITÉ!)
        mqtt_set_inpub_callback(client,
                                mqtt_incoming_publish_cb,
                                mqtt_incoming_data_cb,
                                NULL);

        // subscribe
        err_t err = mqtt_subscribe(client, MQTT_SUB_TOPIC, 0, mqtt_sub_cb, NULL);

        if (err != ERR_OK) {
            CLI_UART_Printf("MQTT: SUB error %d\r\n", err);
        }

    } else {
        CLI_UART_Printf("MQTT: DISCONNECTED (%d)\r\n", status);
    }
}

// ===============================
// 🔌 CONNECT
// ===============================
void MQTT_Connect(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {

    if (client == NULL) {
        client = mqtt_client_new();
        if (client == NULL) {
            CLI_UART_Printf("MQTT: no memory\r\n");
            return;
        }
    }

    if (mqtt_client_is_connected(client)) {
        CLI_UART_Printf("MQTT: already connected\r\n");
        return;
    }

    ip_addr_t ip;
    IP4_ADDR(&ip, ip1, ip2, ip3, ip4);

    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));

    ci.client_id = MQTT_CLIENT_NAME;
    ci.keep_alive = 60;

    CLI_UART_Printf("MQTT: connecting to %d.%d.%d.%d...\r\n", ip1, ip2, ip3, ip4);

    err_t err = mqtt_client_connect(client, &ip, MQTT_PORT, mqtt_connection_cb, NULL, &ci);

    if (err != ERR_OK) {
        CLI_UART_Printf("MQTT connect error: %d\r\n", err);
    }
}

// ===============================
// 🔌 DISCONNECT
// ===============================
void MQTT_Disconnect(void) {
    if (client && mqtt_client_is_connected(client)) {
        mqtt_disconnect(client);
        CLI_UART_Printf("MQTT: disconnected\r\n");
    }
}

// ===============================
// 📤 PUBLISH
// ===============================
void MQTT_Publish(const char *topic, const char *msg) {

    if (!client || !mqtt_client_is_connected(client)) {
        CLI_UART_Printf("MQTT: not connected\r\n");
        return;
    }

    err_t err = mqtt_publish(client,
                             topic,
                             msg,
                             strlen(msg),
                             0,
                             0,
                             mqtt_pub_cb,
                             NULL);

    if (err != ERR_OK) {
        CLI_UART_Printf("MQTT publish error: %d\r\n", err);
    }
}

// ===============================
// 🔄 ZPRACOVÁNÍ (VOLAT V MAIN LOOP)
// ===============================
void MQTT_Process(void) {

    if (mqtt_new_msg) {

        CLI_UART_Printf("[MQTT RX] %s -> %s\r\n", rx_topic, rx_payload);

        char text[200];
        snprintf(text, sizeof(text), "[MQTT RX] %s -> %s", rx_topic, rx_payload);
        ILI9341_DrawText(text, FONT1, 10, 10, WHITE, BLACK);

        mqtt_new_msg = 0;
    }
}
