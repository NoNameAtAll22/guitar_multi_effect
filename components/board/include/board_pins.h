#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/* ================= SPI BUS ================= */
#define LCD_HOST     SPI3_HOST
#define PIN_MOSI     23
#define PIN_MISO     20
#define PIN_SCLK     22

/* ================= LCD ================= */
#define PIN_LCD_CS   32
#define PIN_LCD_DC   51
#define PIN_LCD_RST  33
#define PIN_LCD_LED  21

/* ================= TOUCH ================= */
#define PIN_TP_CS    36
#define PIN_TP_IRQ   35

/* ================= ADC/DAC ================*/

#define PIN_BCLK   7
#define PIN_WS     8
#define PIN_MCLK   28   
#define PIN_ADC    30   
#define PIN_DAC    29  

/* ================= BUTTONS ================= */
#define PIN_BTN_CLR  50
#define BTN_HOLD_MS  1000

/* ================= CALIBRATION ================= */
#define CAL_X_MIN    10
#define CAL_X_MAX    230
#define CAL_Y_MIN    10
#define CAL_Y_MAX    318

/* ================= RESOLUTION ================= */
#define LCD_H_RES    240
#define LCD_V_RES    320

#define SHDN_PIN    34