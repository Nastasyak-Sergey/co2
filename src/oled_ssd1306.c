


#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/i2c.h>

#include "../inc/oled_ssd1306.h"
#include "../inc/ssd1306_fonts.h"
#include "../inc/board.h"
#include "../inc/common.h"
#include "../inc/errors.h"
#include "../inc/i2c.h"

#include "libprintf/printf.h"

#include <stdint.h>

// Screenbuffer
static uint8_t ssd1306_buff[OLED_BUFF_LEN];

// Screen objct
//static oled_ssd1306_t oled_ssd1306;

uint32_t oled_i2c;

void ssd1306_reset(oled_ssd1306_t *obj) {
    /* for I2C - do nothing */
	obj->x_pos = 0;
	obj->y_pos = 0;
	obj->inverted = 0;
	obj->initialized = 0;
	obj->display_on = 0;

}

// Send a byte to the command register
int  ssd1306_write_cmd(uint8_t byte) {
	int ret;

	ret = i2c_write_buf_poll(SSD1306_I2C_ADDR, COMMAND, &byte, 1);
	if (ret != 0) {
		printf("Error: Can't write data; err = %d\n", ret);
		return ret;
	}
}

// Send data
int ssd1306_write_data(uint8_t* buffer, uint16_t buff_size) {
	int ret;

	ret = i2c_write_buf_poll(SSD1306_I2C_ADDR, DATAONLY, buffer, buff_size);
	if (ret != 0) {
		printf("Error: Can't write data; err = %d\n", ret);
		return ret;
	}
}

//Alternative init
//static uint8_t cmds[] =  
//			 0xAE, 0x00, 0x10, 0x40, 0x81, 0xCF, 0xA1, 0xA6 
//			 0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1 
//			0xDA, 0x12, 0xDB, 0x40, 0x8D, 0x14, 0xAF, 0xFF  
//
//	 gpio_clear(GPIOC,GPIO13 
//	 oled_reset 
//	 for ( unsigned ux=0; cmds[ux] != 0xFF; ++ux  
//	 	oled_command(cmds[ux     




// Initialize the oled screen
void ssd1306_init(oled_ssd1306_t *obj)
{
	int ret;

	uint32_t oled_i2c = obj->i2c;

	i2c_init(oled_i2c);

	ret = i2c_detect_device(SSD1306_I2C_ADDR);
	if (ret != 0)
		printf("Cant find device on I2C bus, err = %d\n", ret);

    // Reset OLED
    ssd1306_reset(obj);

    // Wait for the screen
    mdelay(100);

    // Init OLED
    ssd1306_set_display_on(obj, 0); //display off

    ssd1306_write_cmd(0x20); //Set Memory Addressing Mode

    ssd1306_write_cmd(0x00); // 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
                                // 10b,Page Addressing Mode (RESET); 11b,Invalid

    ssd1306_write_cmd(0xB0); //Set Page Start Address for Page Addressing Mode,0-7

#ifdef SSD1306_MIRROR_VERT
    ssd1306_write_cmd(0xC0); // Mirror vertically
#else
    ssd1306_write_cmd(0xC8); //Set COM Output Scan Direction
#endif

    ssd1306_write_cmd(0x00); //---set low column address
    ssd1306_write_cmd(0x10); //---set high column address

    ssd1306_write_cmd(0x40); //--set start line address - CHECK

    ssd1306_set_contrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_write_cmd(0xA0); // Mirror horizontally
#else
    ssd1306_write_cmd(0xA1); //--set segment re-map 0 to 127 - CHECK
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_write_cmd(0xA7); //--set inverse color
#else
    ssd1306_write_cmd(0xA6); //--set normal color
#endif

// Set multiplex ratio.
#if (SSD1306_HEIGHT == 128)
    // Found in the Luma Python lib for SH1106.
    ssd1306_write_cmd(0xFF);
#else
    ssd1306_write_cmd(0xA8); //--set multiplex ratio(1 to 64) - CHECK
#endif

#if (SSD1306_HEIGHT == 32)
    ssd1306_write_cmd(0x1F); //
#elif (SSD1306_HEIGHT == 64)
    ssd1306_write_cmd(0x3F); //
#elif (SSD1306_HEIGHT == 128)
    ssd1306_write_cmd(0x3F); // Seems to work for 128px high displays too.
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif
	
	ssd1306_write_cmd(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content

    ssd1306_write_cmd(0xD3); //-set display offset - CHECK
    ssd1306_write_cmd(0x00); //-not offset

    ssd1306_write_cmd(0xD5); //--set display clock divide ratio/oscillator frequency
    ssd1306_write_cmd(0xF0); //--set divide ratio

    ssd1306_write_cmd(0xD9); //--set pre-charge period
    ssd1306_write_cmd(0x22); //

    ssd1306_write_cmd(0xDA); //--set com pins hardware configuration - CHECK
#if (SSD1306_HEIGHT == 32)
    ssd1306_write_cmd(0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_write_cmd(0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_write_cmd(0x12);
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_write_cmd(0xDB); //--set vcomh
    ssd1306_write_cmd(0x20); //0x20,0.77xVcc

    ssd1306_write_cmd(0x8D); //--set DC-DC enable
    ssd1306_write_cmd(0x14); //

	ssd1306_set_display_on(obj, 1); //--turn on SSD1306 panel

    // Clear screen
    ssd1306_fill(BLACK);
    // Flush buffer to screen
    ssd1306_update_screen();
    
    // Set default values for screen object
    obj->x_pos = 0;
    obj->y_pos = 0;
	obj->initialized = 1;
}



void ssd1306_set_display_on(oled_ssd1306_t * obj, const uint8_t on) {
    uint8_t value;
    if (on) {
        value = 0xAF;   // Display on
        obj->display_on  = 1;
    } else {
        value = 0xAE;   // Display off
        obj->display_on = 0;
    }
    ssd1306_write_cmd(value);
}

void ssd1306_set_contrast(const uint8_t value) {
    const uint8_t set_contrast_control_register = 0x81;
    ssd1306_write_cmd(set_contrast_control_register);
    ssd1306_write_cmd(value);
}

// Fill the whole screen with the given color
void ssd1306_fill(oled_color_t color) {
    /* Set memory */
    uint32_t i;

    for(i = 0; i < OLED_BUFF_LEN; i++) {
        ssd1306_buff[i] = (color == BLACK) ? 0x00 : 0xFF;
    }
}

// Write the screenbuffer with changed to the screen
void ssd1306_update_screen(void) {
    // Write data to each page of RAM. Number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        ssd1306_write_cmd(0xB0 + i); // Set the current RAM page address.
        ssd1306_write_cmd(0x00);
        ssd1306_write_cmd(0x10);
        ssd1306_write_data(&ssd1306_buff[SSD1306_WIDTH*i],SSD1306_WIDTH);
    }
}
//    Draw one pixel in the screenbuffer
//    X => X Coordinate
//    Y => Y Coordinate
//    color => Pixel color
void ssd1306_draw_pixel(uint8_t x, uint8_t y, oled_color_t color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        // Don't write outside the buffer
        return;
    }

//    // Check if pixel should be inverted
//    if(SSD1306.Inverted) {
//        color = (SSD1306_COLOR)!color;
//    }
    
    // Draw in the right color
    if(color == WHITE) {
        ssd1306_buff[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else { 
        ssd1306_buff[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

// Draw 1 char to the screen buffer
// ch       => char om weg te schrijven
// Font     => Font waarmee we gaan schrijven
// color    => Black or White
char ssd1306_write_char(oled_ssd1306_t *obj, char ch, font_def_t font, oled_color_t color) {
    uint32_t i, b, j;

    // Check if character is valid
    if (ch < 32 || ch > 126)
        return 0;

    // Check remaining space on current line
    if (SSD1306_WIDTH < (obj->x_pos + font.font_width) ||
        SSD1306_HEIGHT < (obj->y_pos + font.font_height))
    {
        // Not enough space on current line
        return 0;
    }

    // Use the font to write
    for(i = 0; i < font.font_height; i++) {
        b = font.data[(ch - 32) * font.font_height + i];
        for(j = 0; j < font.font_width; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_draw_pixel(obj->x_pos + j, (obj->y_pos + i), (oled_color_t) color);
            } else {
                ssd1306_draw_pixel(obj->x_pos + j, (obj->y_pos + i), (oled_color_t)!color);
            }
        }
    }

    // The current space is now taken
    obj->x_pos += font.font_width;

    // Return written char for validation
    return ch;
}

// Write full string to screenbuffer
char ssd1306_write_string(oled_ssd1306_t *obj, char* str, font_def_t font, oled_color_t color) {
    // Write until null-byte
    while (*str) {
        if (ssd1306_write_char(obj, *str, font, color) != *str) {
            // Char could not be written
            return *str;
        }

        // Next char
        str++;
    }

    // Everything ok
    return *str;
}

// Position the cursor
void ssd1306_set_cursor(oled_ssd1306_t * obj, uint8_t x, uint8_t y) {
    obj->x_pos = x;
    obj->y_pos = y;
}
