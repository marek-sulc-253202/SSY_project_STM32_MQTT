/*
 * ssy_cli.c
 *
 * Created on: 3. 3. 2026
 * Author: Ondra
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "ssy_cli.h"
#include "main.h"
#include "mqtt_app.h"

/* USER CODE BEGIN Includes */
// Deklarace funkce pro odeslání Pingu, která je definovaná v main.c
extern void Odesli_Ping(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4);

// Řekneme CLI, že huart3 existuje někde jinde (v main.c)
extern UART_HandleTypeDef huart3;

extern void Nastav_IP(uint8_t dhcp_on, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4);
extern void Vypis_IP_Status(void);
/* USER CODE END Includes */

/* -------------------------------------------------------------------------- */
/* Stavové proměnné                                                           */
/* -------------------------------------------------------------------------- */

/* RX kruhový buffer */
static volatile uint8_t  cli_rx_buf[CLI_RX_BUF_SIZE];
static volatile uint16_t cli_rx_head = 0;
static volatile uint16_t cli_rx_tail = 0;

/* Sestavovaný řádek (příkaz) */
static char     cli_line_buf[CLI_LINE_BUF_SIZE];
static uint16_t cli_line_len = 0;

/* Echo (0 = vypnuto, 1 = zapnuto) */
static uint8_t echo = 1;

/* 1-bajtový buffer pro HAL_UART_Receive_IT() */
static uint8_t cli_uart_rx1 = 0;

/* Dopředná deklarace parseru řádku */
static void CLI_HandleLine(char *line);




extern volatile uint8_t sensor_enabled;
extern GPIO_PinState last_state;

/* -------------------------------------------------------------------------- */
/* Pomocné funkce – transport přes UART                                       */
/* -------------------------------------------------------------------------- */

static void CLI_UART_Send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    while (HAL_UART_Transmit(&huart3, (uint8_t*)data, len, 1000) == HAL_BUSY) {
        HAL_Delay(1);
    }
}

void CLI_UART_Print(const char *s)
{
    if (s == NULL) return;
    CLI_UART_Send((const uint8_t*)s, (uint16_t)strlen(s));
}

void CLI_UART_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    CLI_UART_Print(buf);
}

/* -------------------------------------------------------------------------- */
/* Ring buffer – RX                                                           */
/* -------------------------------------------------------------------------- */

void CLI_UART_OnRxByte(uint8_t b)
{
    uint16_t next = (cli_rx_head + 1u) % CLI_RX_BUF_SIZE;

    if (next != cli_rx_tail) {
        cli_rx_buf[cli_rx_head] = b;
        cli_rx_head = next;
    }
}

static int CLI_RX_GetChar(void)
{
    if (cli_rx_head == cli_rx_tail) {
        return -1;
    }

    uint8_t c = cli_rx_buf[cli_rx_tail];
    cli_rx_tail = (cli_rx_tail + 1u) % CLI_RX_BUF_SIZE;
    return (int)c;
}

/* -------------------------------------------------------------------------- */
/* Inicializace a HAL callbacky                                               */
/* -------------------------------------------------------------------------- */

void CLI_UART_Init()
{
    cli_rx_head = 0;
    cli_rx_tail = 0;
    cli_line_len = 0;
    echo = 1;

    CLI_UART_Print("\r\nCLI ready (UART3). Type 'help' for commands.\r\n> ");

    (void)HAL_UART_Receive_IT(&huart3, &cli_uart_rx1, 1);
}

void CLI_UART_RxCpltHook(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return;

    if (huart->Instance == USART3) {
        CLI_UART_OnRxByte(cli_uart_rx1);
        (void)HAL_UART_Receive_IT(&huart3, &cli_uart_rx1, 1);
    }
}

#ifndef CLI_UART_USER_PROVIDES_HAL_UART_CALLBACKS
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    CLI_UART_RxCpltHook(huart);
}
#endif

/* -------------------------------------------------------------------------- */
/* Hlavní zpracování CLI                                                      */
/* -------------------------------------------------------------------------- */

void CLI_UART_Process(void)
{
    int ch;

    while ((ch = CLI_RX_GetChar()) >= 0) {
        char c = (char)ch;

        if (c == '\r' || c == '\n') {
            if (echo) CLI_UART_Print("\r\n");

            if (cli_line_len > 0) {
                cli_line_buf[cli_line_len] = '\0';
                CLI_HandleLine(cli_line_buf);
                cli_line_len = 0;
                CLI_UART_Print("> ");
            } else {
                CLI_UART_Print("> ");
            }
            continue;
        }

        if (c == '\b' || c == 0x7F) {
            if (cli_line_len > 0) {
                cli_line_len--;
                if (echo) CLI_UART_Print("\b \b");
            }
            continue;
        }

        if (cli_line_len < (CLI_LINE_BUF_SIZE - 1u)) {
            cli_line_buf[cli_line_len++] = c;
            if (echo) CLI_UART_Send((uint8_t*)&c, 1);
        } else {
            cli_line_len = 0;
            CLI_UART_Print("\r\nLine too long, clearing.\r\n> ");
        }
    }

    // === PŘIDEJ TOTO NA ÚPLNÝ KONEC FUNKCE ===
    // Ochrana: Pokud HAL knihovna potichu vypla příjem (kvůli kolizi), nahodíme ho zpět
    if (huart3.RxState == HAL_UART_STATE_READY) {
        HAL_UART_Receive_IT(&huart3, &cli_uart_rx1, 1);
    }

    // === VYLEPŠENÁ OCHRANA NA KONCI FUNKCE ===
    // Pokud HAL vyhlásí chybu (např. Overrun), zrušíme ji a nahodíme příjem
    if (huart3.ErrorCode != HAL_UART_ERROR_NONE) {
        huart3.ErrorCode = HAL_UART_ERROR_NONE;
        huart3.RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart3, &cli_uart_rx1, 1);
    }
    // Nebo pokud je příjem jen potichu vypnutý (původní oprava)
    else if (huart3.RxState == HAL_UART_STATE_READY) {
    	HAL_UART_Receive_IT(&huart3, &cli_uart_rx1, 1);
    }
}

/* -------------------------------------------------------------------------- */
/* HELP                                                                       */
/* -------------------------------------------------------------------------- */

static void CLI_ShowHelp(void)
{
    CLI_UART_Print(
    		"  --- AVAILABLE COMMANDS ---\r\n"
            "  help, ?                      - show this help\r\n"
            "  echo                         - enable/disable console echo\r\n"
            "  ping <IP>                    - Send ICMP Ping (e.g., ping 192 168 1 50)\r\n"
            "  ip status                    - Show current network status (IP, Mask, GW)\r\n"
            "  ip dhcp                      - Enable DHCP client to get IP automatically\r\n"
            "  ip <IP>                      - Set static IP address (e.g., ip 192 168 53 21)\r\n"
    		"  mqtt_connect <IP>            - Connects to the MQTT broker (e.g., mqtt_connect 192.168.53.15)\r\n"
    		"  mqtt_disconnect              - Safely terminates the broker connection\r\n"
    		"  mqtt_send <topic> <message>  - Sends text to the topic (e.g., mqtt_send mqtt/4 Hello world)\r\n"
    		"  senzor [on/off]				- Turns on or off sending data from senzor to mqtt topic\r\n"
    		"  -----------------------------------------------------------------------------\r\n"
    );
}

/* -------------------------------------------------------------------------- */
/* Parsování příkazů                                                          */
/* -------------------------------------------------------------------------- */

static void CLI_HandleLine(char *line)
{
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    char *end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }

    if (*p == '\0') return;

    char *cmd = strtok(p, " ");
    if (cmd == NULL) return;

    /* help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        CLI_ShowHelp();
        return;
    }

    /* ECHO */
    if (strcmp(cmd, "echo") == 0) {
        echo ^= 1;
        CLI_UART_Printf("Echo %d\r\n", echo);
        return;
    }

    /* ----------------------------------------------------------------- */
    /* ETHERNET: Odeslání Pingu                                          */
    /* ----------------------------------------------------------------- */
    if (strcmp(cmd, "ping") == 0) {
        // Získáme všechny 4 části IP adresy (oddělené tečkou nebo mezerou)
        char *ip1_str = strtok(NULL, " .");
        char *ip2_str = strtok(NULL, " .");
        char *ip3_str = strtok(NULL, " .");
        char *ip4_str = strtok(NULL, " .");

        if (!ip1_str || !ip2_str || !ip3_str || !ip4_str) {
            CLI_UART_Print("Pouziti: ping <IP adresa> (napriklad: ping 192 168 1 50)\r\n");
            return;
        }

        uint8_t ip1 = atoi(ip1_str);
        uint8_t ip2 = atoi(ip2_str);
        uint8_t ip3 = atoi(ip3_str);
        uint8_t ip4 = atoi(ip4_str);

        // Zavoláme funkci z main.c
        Odesli_Ping(ip1, ip2, ip3, ip4);
        return;
    }

    if (strcmp(cmd, "ip") == 0) {
        char *arg1 = strtok(NULL, " ");

        if (arg1 == NULL || strcmp(arg1, "status") == 0) {
            Vypis_IP_Status();
            return;
        }

        if (strcmp(arg1, "dhcp") == 0) {
            Nastav_IP(1, 0, 0, 0, 0);
        } else {
            // Rozsekáme IP (bere to tečky i mezery)
            char *ip1_s = arg1;
            char *ip2_s = strtok(NULL, " .");
            char *ip3_s = strtok(NULL, " .");
            char *ip4_s = strtok(NULL, " .");

            if (ip1_s && ip2_s && ip3_s && ip4_s) {
                Nastav_IP(0, atoi(ip1_s), atoi(ip2_s), atoi(ip3_s), atoi(ip4_s));
            } else {
                CLI_UART_Print("Chyba! Zkus: 'ip dhcp', 'ip status' nebo 'ip 192.168.1.50'\r\n");
            }
        }
        return;
    }

    // ==========================================
    // 1. Prikaz: Připojení k brokeru
    // ==========================================
    if (strcmp(cmd, "mqtt_connect") == 0) {
        char *ip1_s = strtok(NULL, " .");
        char *ip2_s = strtok(NULL, " .");
        char *ip3_s = strtok(NULL, " .");
        char *ip4_s = strtok(NULL, " .");

        if (ip1_s && ip2_s && ip3_s && ip4_s) {
            MQTT_Connect(atoi(ip1_s), atoi(ip2_s), atoi(ip3_s), atoi(ip4_s));
        } else {
            CLI_UART_Print("Chyba! Pouziti: mqtt_connect <IP>\r\n");
            CLI_UART_Print("Napr: mqtt_connect 192 168 53 15\r\n");
        }
        return;
    }

    // ==========================================
    // 2. Prikaz: Odpojení od brokera
    // ==========================================
    if (strcmp(cmd, "mqtt_disconnect") == 0) {
        MQTT_Disconnect();
        return;
    }

    // ==========================================
    // 3. Prikaz: Odeslání zprávy (na cokoliv, co následuje)
    // ==========================================
    if (strcmp(cmd, "mqtt_send") == 0) {
    	char *topic = strtok(NULL, " ");
        char *zprava = strtok(NULL, ""); // Vezme cely zbytek radku az do konce

        if (zprava != NULL && strlen(zprava) > 0 && topic != NULL && strlen(topic) > 0) {
            MQTT_Publish(topic, zprava);
        } else {
            CLI_UART_Print("Chyba! Pouziti: mqtt_send <topic> <zprava>\r\n");
            CLI_UART_Print("Napr: mqtt_send mqtt/4 Tlacitko bylo stisknuto!\r\n");
        }
        return;
    }

    if (strcmp(cmd, "senzor") == 0) {
        char *arg = strtok(NULL, " ");

        if (arg == NULL) {
            CLI_UART_Print("Pouziti: senzor on/off\r\n");
            return;
        }

        if (strcmp(arg, "on") == 0) {
            sensor_enabled = 1;
            last_state = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14);
            CLI_UART_Print("Senzor zapnut\r\n");
        }
        else if (strcmp(arg, "off") == 0) {
            sensor_enabled = 0;
            CLI_UART_Print("Senzor vypnut\r\n");
        }
        else {
            CLI_UART_Print("Pouziti: senzor on/off\r\n");
        }
        return;
    }

    /* Neznámý příkaz */
    CLI_UART_Print("Unknown command. Type 'help'.\r\n");
}
