#ifndef __BM_H__
#define __BM_H__

bool bm_draw_pixel(uint8_t* bm, uint16_t width, uint16_t height, uint16_t x, uint16_t y, bool val);
bool bm_draw_string(uint8_t* bm, uint8_t char_w, uint8_t char_h, uint16_t x, uint16_t y, char* str);
bool bm_printf(uint8_t* bm, uint16_t width, uint16_t height, uint16_t x, uint16_t y, const char* fmt, ...);
uint8_t* bmp_read(const char* name, uint16_t* width, uint16_t* height);
bool bmp_printf(const char* name, uint16_t x, uint16_t y, const char* fmt, ...);
bool bmp_draw(const char* name, uint16_t x, uint16_t y);
bool bmp_update(const char* name, uint16_t x, uint16_t y);
uint16_t bm_qr_printf(uint16_t x, uint16_t y, const char* fmt, ...);

#endif /* __BM_H__ */
