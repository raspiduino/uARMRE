#include "Console_io.h"
#include "fifo.h"

fifo_t serial_in = fifo_create(BUF_SIZE, sizeof(int));

Console console;
T2Input t2input;

extern "C" void console_char_in(char ch){
	console.put_c(ch);
}

extern "C" void console_str_in(const char* str){
	console.putstr(str);
}

extern "C" void console_str_with_length_in(const char* str, int length){
	console.putstr(str, length);
}

extern "C" void console_char_out(char ch){
	fifo_add(serial_in, &ch); // Add to FIFO buffer
}

extern "C" void console_str_out(const char* str){
	for(unsigned int i = 0; i < strlen(str); i++){
		console_char_out(str[i]);
	}
}

extern "C" void console_str_with_length_out(const char* str, int length){
	console_str_out((char*)str);
}

extern "C" void terminal_init(){
	console.init();
	t2input.init();
}

extern "C" void t2input_draw(VMUINT8* layerbuf1){
	int scr_w = vm_graphic_get_screen_width(); 
	int scr_h = vm_graphic_get_screen_height();

	vm_graphic_fill_rect(layerbuf1, 0, 0, scr_w, scr_h, tr_color, tr_color);
	vm_graphic_line(layerbuf1, console.cursor_x*char_width, (console.cursor_y+1)*char_height,
		(console.cursor_x+1)*char_width, (console.cursor_y+1)*char_height, console.cur_textcolor);
	t2input.draw();
}

extern "C" void set_layer_handler(VMUINT8* layerbuf0, VMUINT8* layerbuf1, VMINT layerhdl){
	console.scr_buf=layerbuf0;
	console.draw_all();

	t2input.scr_buf=layerbuf1;
	t2input.layer_handle=layerhdl;
}

extern "C" void t2input_handle_keyevt(int event, int keycode){
	t2input.handle_keyevt(event, keycode);
}

extern "C" void t2input_handle_penevt(int event, int x, int y){
	t2input.handle_penevt(event, x, y);
}