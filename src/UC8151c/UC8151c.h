/*
 * See .c file for descriptions and comments.
 *
 * File:   UC8151c.h
 * Author: tommy
 *
 * Created on 2 December 2019, 4:57 PM
 */

#ifndef __UC8151c_H__
#define __UC8151c_H__

// Hard-coded dimensions of the display
#define UC8151_WIDTH (296)
#define UC8151_HEIGHT (128)

void uc8151_init(void);
void uc8151_reset();
void uc8151_draw_bitmap(uint8_t* data, uint16_t width, uint16_t height, uint16_t x, uint16_t y);
void uc8151_fill_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t colour);
void uc8151_update(uint8_t* data);
void uc8151_clear();
void uc8151_refresh();
void uc8151_sleep();

#endif /* __UC8151c_H__ */
