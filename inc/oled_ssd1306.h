#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include "common.h"
#include "ssd1306_fonts.h"
#include "errors.h"
#include <stdint.h>


#define SSD1306_WIDTH 		128
#define SSD1306_HEIGHT 		64
#define OLED_BUFF_LEN 		SSD1306_WIDTH * SSD1306_HEIGHT / 8


// I2c slave address
#define SSD1306_I2C_ADDR 	0x3C//0x78
//#define SSD1306_I2C_ADDR 	0x3D//0x7e

// Include only needed fonts
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18
#define SSD1306_INCLUDE_FONT_16x26

// Mirror the screen if needed
// #define SSD1306_MIRROR_VERT
// #define SSD1306_MIRROR_HORIZ

#define DATAONLY (uint8_t)0x40 //0b01000000
#define COMMAND  (uint8_t)0x00 //0b00000000

// Singleton object
extern uint32_t oled_i2c;

// Screen colors
typedef enum {
    BLACK = 0x00, // Black color, no pixel
    WHITE = 0x01  // Pixel is set. Color depends on OLED
} oled_color_t;

// Errors enum
typedef enum {
    OLED_OK = 0x00,
    OLED_ERR = 0x01  // Generic error.
} oled_err_t;


typedef struct {
    uint32_t i2c;
	uint32_t addr;
	uint16_t x_pos;
    uint16_t y_pos;
    uint8_t inverted;
    uint8_t initialized;
	 uint8_t display_on;
} oled_ssd1306_t;


typedef struct {
    uint8_t x;
    uint8_t y;
} oledd_vertex_t;


// Procedure definitions
void ssd1306_init(oled_ssd1306_t * obj);
void ssd1306_fill(oled_color_t color);
void ssd1306_update_screen(void);
void ssd1306_draw_pixel(uint8_t x, uint8_t y, oled_color_t color);
char ssd1306_write_char(oled_ssd1306_t *obj, char ch, font_def_t font, oled_color_t color);
char ssd1306_write_string(oled_ssd1306_t *obj, char* str, font_def_t font, oled_color_t color);
void ssd1306_set_cursor(oled_ssd1306_t *obj, uint8_t x, uint8_t y);
void ssd1306_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, oled_color_t color);
void ssd1306_draw_arc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, oled_color_t color);
void ssd1306_draw_circle(uint8_t par_x, uint8_t par_y, uint8_t par_r, oled_color_t color);
void ssd1306_polyline(const oledd_vertex_t *par_vertex, uint16_t par_size, oled_color_t color);
void ssd1306_draw_rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, oled_color_t color);
/**
 * @brief Sets the contrast of the display.
 * @param[in] value contrast to set.
 * @note Contrast increases as the value increases.
 * @note RESET = 7Fh.
 */
void ssd1306_set_contrast(const uint8_t value);
/**
 * @brief Set Display ON/OFF.
 * @param[in] on 0 for OFF, any for ON.
 */
void ssd1306_set_display_on(oled_ssd1306_t * obj, const uint8_t on);
/**
 * @brief Reads DisplayOn state.
 * @return  0: OFF.
 *          1: ON.
 */
uint8_t ssd1306_get_display_on(void);

// Low-level procedures
void ssd1306_reset(oled_ssd1306_t * obj);
int  ssd1306_write_cmd(uint8_t byte);
int ssd1306_write_data(uint8_t* buffer, uint16_t buff_size);
oled_err_t ssd1306_fill_buffer(uint8_t* buf, uint16_t len);

#endif
