/*
 * ssy_cli.h
 *
 *  Created on: 3. 3. 2026
 *      Author: Ondra
 */

#ifndef INC_SSY_CLI_H_
#define INC_SSY_CLI_H_

#include <stdint.h>
#include "main.h"
/** Velikost RX kruhového bufferu (znaky přijaté v ISR) */
#define CLI_RX_BUF_SIZE    256

/** Velikost řádkového bufferu (sestavovaný příkaz) */
#define CLI_LINE_BUF_SIZE  128

//#define CLI_UART_USER_PROVIDES_HAL_UART_CALLBACKS


void CLI_UART_Print(const char *s);
void CLI_UART_Printf(const char *fmt, ...);
void CLI_UART_OnRxByte(uint8_t b);
void CLI_UART_Init();
void CLI_UART_RxCpltHook(UART_HandleTypeDef *huart);
void CLI_UART_Process(void);


#endif /* INC_SSY_CLI_H_ */
