
#ifndef __SSD1306_FONTS_H__
#define __SSD1306_FONTS_H__

#define SSD1306_INCLUDE_FONT_16x26
#define SSD1306_INCLUDE_FONT_7x10

#include <stdint.h>

typedef struct {
	const uint8_t font_width;    /*!< Font width in pixels */
	uint8_t font_height;   /*!< Font height in pixels */
	const uint16_t *data; /*!< Pointer to data font data array */
} font_def_t;

#ifdef SSD1306_INCLUDE_FONT_6x8
extern font_def_t font_6x8;
#endif
#ifdef SSD1306_INCLUDE_FONT_7x10
extern font_def_t font_7x10;
#endif
#ifdef SSD1306_INCLUDE_FONT_11x18
extern font_def_t font_11x18;
#endif
#ifdef SSD1306_INCLUDE_FONT_16x26
extern font_def_t font_16x26;
#endif
#endif // __SSD1306_FONTS_H__
