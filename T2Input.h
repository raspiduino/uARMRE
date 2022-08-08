#pragma once

#ifdef __cplusplus

#include "main.h"

class T2Input
{
public:

	enum Input_mode{
		SMALL,
		FIRST_BIG,
		BIG,
		NUM,
	};

	enum State{
		MAIN,
		SECOND_CLICK,
		F_NUM,
		SET_MENU,
		CTRL,
		CTRL_SECOND_CLICK
	};

	Input_mode cur_input_mode;
	int last_input_time;
	int last_imput_key;

	State state;
	
	VMUINT8* scr_buf;
	int layer_handle;
	
	int key_w, key_h;
	int keyboard_h;

	bool draw_kb;

	int current_key;

	int squares[8][2];

	int get_keycode(int x, int y);

	void send_c(const char*str);

	void numpad_input(int keycode);

	void show_current_pressed_key();

	void handle_penevt(VMINT event, VMINT x, VMINT y);
	void handle_keyevt(VMINT event, VMINT keycode);

	void draw_xy_char(int x, int y, const char*str);
	void draw_xy_str(int x, int y, const char*str);
	void draw_xy_str_color(int x, int y, unsigned short textcolor,  unsigned short backcolor, const char*str);

	void draw();
	void init();
	~T2Input(void);
};

#endif

