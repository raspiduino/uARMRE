#pragma once
#include "main.h"
#include "Console.h"
#include "T2Input.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int scr_w;
extern int scr_h;

extern int vmstate;

void console_char_in(char ch);
void console_str_in(const char* str);
void console_str_with_length_in(const char* str, int length);

void console_char_out(char ch);
void console_str_out(const char* str);
void console_str_with_length_out(const char* str, int length);

void terminal_init();
void t2input_draw(VMUINT8* layerbuf1);
void set_layer_handler(VMUINT8* layerbuf0, VMUINT8* layerbuf1, VMINT layerhdl);
void t2input_handle_keyevt(int event, int keycode);
void t2input_handle_penevt(int event, int x, int y);

extern fifo_t serial_in;

void save_state();
void load_state();

#ifdef __cplusplus
}
#endif
