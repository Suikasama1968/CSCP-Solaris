/*
  Minimal EMU services for the Solaris + SDL2 direct host.
  Copyright (c) 2026 M.Yoshiyama
 */

#include "osd_compat.h"
#include "../emu.h"
#include "../vm/vm.h"
#include "../vm/vm_template.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

EMU::EMU()
	: vm_(NULL), screen_(NULL), screen_w_(640), screen_h_(400), sound_rate_(44100), now_waiting_in_debugger(false)
{
	memset(key_, 0, sizeof(key_));
	memset(joy_, 0, sizeof(joy_));
#ifdef SCREEN_WIDTH
	screen_w_ = SCREEN_WIDTH;
#endif
#ifdef SCREEN_HEIGHT
	screen_h_ = SCREEN_HEIGHT;
#endif
	screen_ = (scrntype_t*)calloc(screen_w_ * screen_h_, sizeof(scrntype_t));
	if(screen_ == NULL) {
		fprintf(stderr, "screen allocation failed\n");
		exit(1);
	}
}

EMU::~EMU()
{
	free(screen_);
}

void EMU::lock_vm()
{
}

void EMU::unlock_vm()
{
}

void EMU::force_unlock_vm()
{
}

bool EMU::is_vm_locked()
{
	return false;
}

void EMU::start_waiting_in_debugger()
{
	now_waiting_in_debugger = true;
}

void EMU::process_waiting_in_debugger()
{
}

void EMU::finish_waiting_in_debugger()
{
	now_waiting_in_debugger = false;
}

const uint8_t* EMU::get_key_buffer()
{
	return key_;
}

#ifdef USE_JOYSTICK
const uint32_t* EMU::get_joy_buffer()
{
	return joy_;
}
#endif

void EMU::set_vm_screen_size(int screen_width, int screen_height, int, int, int, int)
{
	if(screen_width <= 0 || screen_height <= 0) return;
	if(screen_width == screen_w_ && screen_height == screen_h_) return;

	scrntype_t* next = (scrntype_t*)calloc(screen_width * screen_height, sizeof(scrntype_t));
	if(next == NULL) return;
	free(screen_);
	screen_ = next;
	screen_w_ = screen_width;
	screen_h_ = screen_height;
}

void EMU::set_vm_screen_lines(int)
{
}

#ifdef USE_SCREEN_FILTER
void EMU::screen_skip_line(bool)
{
}
#endif

scrntype_t* EMU::get_screen_buffer(int y)
{
	if(y < 0) y = 0;
	if(y >= screen_h_) y = screen_h_ - 1;
	return screen_ + screen_w_ * y;
}

void EMU::sleep(uint32_t ms)
{
	solaris_sleep_ms(ms);
}

double EMU::get_frame_rate()
{
	return (vm_ != NULL) ? vm_->get_frame_rate() : 59.94;
}

int EMU::get_sound_rate()
{
	return sound_rate_;
}

void EMU::set_sound_rate(int rate)
{
	if(rate > 0) sound_rate_ = rate;
}

static void vprint_line(const _TCHAR* format, va_list ap)
{
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
}

void EMU::out_message(const _TCHAR* format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprint_line(format, ap);
	va_end(ap);
}

void EMU::out_debug_log(const _TCHAR* format, ...)
{
#ifdef _DEBUG_LOG
	va_list ap;
	va_start(ap, format);
	vprint_line(format, ap);
	va_end(ap);
#else
	(void)format;
#endif
}

void EMU::force_out_debug_log(const _TCHAR* format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprint_line(format, ap);
	va_end(ap);
}

#ifdef USE_PRINTER
void EMU::create_bitmap(bitmap_t *bitmap, int width, int height)
{
	if(bitmap == NULL) return;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->lpBmp = (scrntype_t*)calloc(width * height, sizeof(scrntype_t));
}

void EMU::release_bitmap(bitmap_t *bitmap)
{
	if(bitmap == NULL) return;
	free(bitmap->lpBmp);
	bitmap->lpBmp = NULL;
	bitmap->width = bitmap->height = 0;
}

void EMU::create_font(font_t *font, const _TCHAR *family, int width, int height, int rotate, bool bold, bool italic)
{
	if(font == NULL) return;
	memset(font, 0, sizeof(font_t));
	my_tcscpy_s(font->family, 64, family != NULL ? family : _T(""));
	font->width = width;
	font->height = height;
	font->rotate = rotate;
	font->bold = bold;
	font->italic = italic;
}

void EMU::release_font(font_t*)
{
}

void EMU::create_pen(pen_t *pen, int width, uint8_t r, uint8_t g, uint8_t b)
{
	if(pen == NULL) return;
	pen->width = width;
	pen->r = r;
	pen->g = g;
	pen->b = b;
}

void EMU::release_pen(pen_t*)
{
}

static scrntype_t printer_color(uint8_t r, uint8_t g, uint8_t b)
{
	return (scrntype_t)((0xffu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
}

void EMU::clear_bitmap(bitmap_t *bitmap, uint8_t r, uint8_t g, uint8_t b)
{
	if(bitmap == NULL || bitmap->lpBmp == NULL) return;
	scrntype_t c = printer_color(r, g, b);
	for(int i = 0; i < bitmap->width * bitmap->height; i++) bitmap->lpBmp[i] = c;
}

int EMU::get_text_width(bitmap_t*, font_t *font, const char *text)
{
	int len = (text != NULL) ? (int)strlen(text) : 0;
	int width = (font != NULL && font->width > 0) ? font->width : 8;
	return len * width;
}

void EMU::draw_text_to_bitmap(bitmap_t*, font_t*, int, int, const char*, uint8_t, uint8_t, uint8_t)
{
}

void EMU::draw_line_to_bitmap(bitmap_t *bitmap, pen_t *pen, int sx, int sy, int ex, int ey)
{
	if(bitmap == NULL || pen == NULL) return;
	int x = min(sx, ex);
	int y = min(sy, ey);
	int w = abs(ex - sx) + 1;
	int h = abs(ey - sy) + 1;
	draw_rectangle_to_bitmap(bitmap, x, y, w, h, pen->r, pen->g, pen->b);
}

void EMU::draw_rectangle_to_bitmap(bitmap_t *bitmap, int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	if(bitmap == NULL || bitmap->lpBmp == NULL) return;
	scrntype_t c = printer_color(r, g, b);
	int x0 = max(0, x);
	int y0 = max(0, y);
	int x1 = min(bitmap->width, x + width);
	int y1 = min(bitmap->height, y + height);
	for(int yy = y0; yy < y1; yy++) {
		for(int xx = x0; xx < x1; xx++) {
			bitmap->lpBmp[yy * bitmap->width + xx] = c;
		}
	}
}

void EMU::draw_point_to_bitmap(bitmap_t *bitmap, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	draw_rectangle_to_bitmap(bitmap, x, y, 1, 1, r, g, b);
}

void EMU::stretch_bitmap(bitmap_t*, int, int, int, int, bitmap_t*, int, int, int, int)
{
}

void EMU::write_bitmap_to_file(bitmap_t*, const _TCHAR*)
{
}
#endif

#ifdef USE_MIDI
void EMU::send_to_midi(uint8_t)
{
}

bool EMU::recv_from_midi(uint8_t*)
{
	return false;
}
#endif
