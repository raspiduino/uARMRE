#pragma once

#ifdef __cplusplus

#include "main.h"

const int char_width = 6, char_height = 11;
const int count_of_lines = 500;

struct Symbol{
	char ch;
	unsigned char flgs;
	unsigned short textcolor, backcolor;
	Symbol(){
		(*(short*)&ch)=0;
		textcolor=backcolor=0;
	}
	void reset(){
		(*(short*)&ch)=0;
		textcolor=backcolor=0;
	}
};

class Console
{
public:
	Symbol **main_text;
	Symbol **scroll_temp_text;
	Symbol *history_text[count_of_lines];

	enum Status{
		MAIN,
		ESCAPE,
		ARGS,
		CSI,
		ESCAPE_HASH,
		ESCAPE_LEFT_BR,
		ESCAPE_RIGTH_BR,
		ESCAPE_QUESTION
	};

	int cursor_x, cursor_y;
	int saved_cursor_x, saved_cursor_y;
	int terminal_w, terminal_h;

	int scroll_start_row, scroll_end_row, scroll_value; 

	bool bright;

	int UTF_l, UTF_i;
	char UTF[4];
	int unicode;

	unsigned short main_color;
	unsigned short cur_textcolor, cur_backcolor; 
	unsigned char flgs;

	VMUINT8* scr_buf;

	int narg, args[16];

	Status status, last_status;

	void check_xy_on_curent_screan();

	int get_n_param(int n, int a=0);
	void attributes();
	void erase_display(int t);
	void erase_line(int t);
	void erase_line(int t, int y);

	void clear_args();

	void analise_args(char c);
	void analise_CSI(char c);
	void analise_escape(char c);
	void analise_escape_hash(char c);
	void analise_escape_left_br(char c);
	void analise_escape_right_br(char c);
	void analise_escape_question(char c);
	void put_char(char c);

	void put_c_(char c);
	void put_c(char c);
	void putstr(const char *str, int len = -1);


	void previos_p();
	void next_p();
	void new_line();
	void scroll(int v);

	void draw_xy_char(int x, int y);
	void draw_cur_char();

	
	void draw_all();

	void init();
	void reset();
	~Console(void);
};

#endif

