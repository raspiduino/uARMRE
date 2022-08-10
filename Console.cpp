#include "Console.h"
#include "vmmm.h"
#include "ProFont6x11.h"
#include "Console_io.h"

const char* Hex_ch = "0123456789ABCDF"; 

extern int scr_w; 
extern int scr_h;

const unsigned short maincolors[8] = 
{
	VM_COLOR_888_TO_565(0,0,0),
	VM_COLOR_888_TO_565(170,0,0),
	VM_COLOR_888_TO_565(0,170,0),
	VM_COLOR_888_TO_565(170,85,0),
	VM_COLOR_888_TO_565(0,0,170),
	VM_COLOR_888_TO_565(170,0,170),
	VM_COLOR_888_TO_565(0,170,170),
	VM_COLOR_888_TO_565(170,170,170)
};
const unsigned short brightcolors[8] = 
{
	VM_COLOR_888_TO_565(85,85,85),
	VM_COLOR_888_TO_565(255,85,85),
	VM_COLOR_888_TO_565(85,255,85),
	VM_COLOR_888_TO_565(255,255,85),
	VM_COLOR_888_TO_565(85,85,255),
	VM_COLOR_888_TO_565(255,85,255),
	VM_COLOR_888_TO_565(85,255,255),
	VM_COLOR_888_TO_565(255,255,255)
};

static bool isdigit(int c){
	return c>='0' && c<='9';
}

void Console::check_xy_on_curent_screan(){
	if(cursor_x<0)
		cursor_x=0;
	if(cursor_x>=terminal_w)
		cursor_x=terminal_w-1;
	if(cursor_y<0)
		cursor_y=0;
	if(cursor_y>=terminal_h)
		cursor_y=terminal_h-1;
}

int Console::get_n_param(int n, int a){
	return args[n]?args[n]:a;
}

void Console::attributes(){
	if(args[1]==2&&args[0]%10==8)
		if(args[0]/10==3){
			cur_textcolor=VM_COLOR_888_TO_565(args[2],args[3],args[4]);
			return;
		}else if(args[0]/10==4){
			cur_backcolor=VM_COLOR_888_TO_565(args[2],args[3],args[4]);
			return;
		}
	if(args[0]==0)
		cur_textcolor=0xFFFF, cur_backcolor=0x0000, bright=0; //Reset

	for(int i=0;i<narg;++i){
		int pp=get_n_param(i), p=pp%10;

		if(bright&&p!=8&&((pp/10)==3||(pp/10)==4))
			pp+=60;

		switch(pp/10){
			case 0:
				switch(p){
					case 1:
						bright=1;
						break;
					case 7:
						flgs=flgs|1;
						break;
				}
				break;
			case 1:
				switch(p){
					case 0:
						flgs=0, bright=0;
						break;
				}
				break;
			case 2:
				switch(p){
					case 1:
						bright=0;
						break;
				}
				break;
			case 3:
				if(p==9)
					cur_textcolor=0xFFFF;
				else
					cur_textcolor=maincolors[p];
				break;
			case 4:
				if(p==9)
					cur_backcolor=0x0000;
				else
					cur_backcolor=maincolors[p];
				break;
			case 9:
				if(p==9)
					cur_textcolor=0xFFFF;
				else
					cur_textcolor=brightcolors[p];
				break;
			case 10:
				if(p==9)
					cur_backcolor=0x0000;
				else
					cur_backcolor=brightcolors[p];
				break;
		}
	}
}

void Console::erase_display(int t){
	switch(t){
		case 0:
			erase_line(0);
			for(int i=cursor_y+1;i<terminal_h;++i)
				erase_line(2,i);
			break;
		case 1:
			erase_line(1);
			for(int i=0;i<cursor_y;++i)
				erase_line(2,i);
			break;
		case 3:
			for(int i=0;i<=count_of_lines;++i)
				for(int j=0;j<=terminal_h;++j)
					history_text[i][j].reset();
		case 2:
			for(int i=0;i<terminal_h;++i)
				erase_line(2,i);
			break;
	}
}

void Console::clear_args(){
	for(int i=0; i<narg; ++i)
		args[i]=0;
	narg=0;
}

void Console::analise_args(char c){
	if(narg==16)
		status=MAIN;
	else if(isdigit(c))
		args[narg]=args[narg]*10+(c-'0');
	else if(c==';')
		++narg;
	else{
		++narg;
		status=last_status;
		put_c(c);
	}
}


void Console::analise_CSI(char c){
	if(isdigit(c)||c==';'){
		clear_args();
		last_status=CSI;
		status=ARGS;
		analise_args(c);
	}else
		switch(c){
			case '@':
				//for(int i=0; i<args[0]; ++i)
				//	put_char(' ');
				status=MAIN;
				break;
			case 'A':
				cursor_y -= get_n_param(0,1);
				if(cursor_y<=0)
					cursor_y=0;
				status=MAIN;
				break;
			case 'B':
				cursor_y += get_n_param(0,1);
				if(cursor_y>terminal_h)
					cursor_y=terminal_h;
				status=MAIN;
				break;
			case 'C':
				cursor_x += get_n_param(0,1);
				if(cursor_x>terminal_w)
					cursor_x=terminal_w;
				status=MAIN;
				break;
			case 'D':
				cursor_x -= get_n_param(0,1);
				if(cursor_x<=0)
					cursor_x=0;
				status=MAIN;
				break;
			case 'E':
				cursor_y+=get_n_param(0,1);
				cursor_x=0;
				check_xy_on_curent_screan();
				status=MAIN;
				break;
			case 'F':
				cursor_y-=get_n_param(0,1);
				cursor_x=0;
				check_xy_on_curent_screan();
				status=MAIN;
				break;
			case 'G':
				cursor_x=get_n_param(0,1)-1;
				check_xy_on_curent_screan();
				status=MAIN;
				break;
			case 'H':
			case 'f':
				cursor_y=get_n_param(0,1)-1;
				cursor_x=get_n_param(1,1)-1;
				check_xy_on_curent_screan();
				status=MAIN;
				break;
			case 'J':
				erase_display(args[0]);
				status=MAIN;
				break;
			case 'K':
				erase_line(args[0]);
				status=MAIN;
				break;

			case 'P':
				{
					int p=get_n_param(0,1);
					for(int i=cursor_x+1;i<terminal_w-p;++i){
						main_text[cursor_y][i]=main_text[cursor_y][i+p];
					}
					for(int j=(cursor_x+1)*char_width; j<(terminal_w-p)*char_width; ++j)
						for(int i=0; i<char_height; ++i)
							((unsigned short*)scr_buf)[(i+cursor_y*char_height)*scr_w+j]=
								((unsigned short*)scr_buf)[(i+cursor_y*char_height)*scr_w+j+p*char_width];

					for(int i=terminal_w-p;i<terminal_w;++i)
						main_text[cursor_y][i].reset();
					for(int j=(terminal_w-p)*char_width; j<(terminal_w)*char_width; ++j)
						for(int i=0; i<char_height; ++i)
							((unsigned short*)scr_buf)[(i+cursor_y*char_height)*scr_w+j]=0;
					status=MAIN;
				}
				break;

			case 'c':
				console_str_out("\033[?1;0c");
				status=MAIN;
				break;
			case 'd':
				cursor_y=get_n_param(0,1)-1;
				check_xy_on_curent_screan();
				status=MAIN;
				break;

			case 'm':
				attributes();
				status=MAIN;
				break;

			case 'r':
				if(narg == 2 && get_n_param(0,1) < get_n_param(1,1)){
					scroll_start_row=get_n_param(0,1)-1;
					scroll_end_row=get_n_param(1,1);
				}
				status=MAIN;
				break;

			case '?':
				status=ESCAPE_QUESTION;
				clear_args();
				break;

			default:
				my_printf("CSI %c\n", c);
				status=MAIN;
				break;
		}
}

void  Console::erase_line(int t, int y){
	int st=(t==0?cursor_x:0);
	int end=(t==1?cursor_x:terminal_w-1);
	for(int i=st;i<=end;++i)
		main_text[y][i].reset();
	for(int j=st*char_width; j<(end+1)*char_width; ++j)
		for(int i=0; i<char_height; ++i)
			((unsigned short*)scr_buf)[(i+y*char_height)*scr_w+j]=0x0000;
}
void  Console::erase_line(int t){
	int st=(t==0?cursor_x:0);
	int end=(t==1?cursor_x:terminal_w-1);
	for(int i=st;i<=end;++i)
		main_text[cursor_y][i].reset();
	for(int j=st*char_width; j<(end+1)*char_width; ++j)
		for(int i=0; i<char_height; ++i)
			((unsigned short*)scr_buf)[(i+cursor_y*char_height)*scr_w+j]=0x0000;
}

void Console::analise_escape_hash(char c){
	switch(c){
		default:
			my_printf("ESCAPE_HASH %c\n", c);
			status=MAIN;
			break;
	}
}

void Console::analise_escape_left_br(char c){
	switch(c){
		default:
			my_printf("ESCAPE_LEFT_BR %c\n", c);
			status=MAIN;
			break;
	}
}

void Console::analise_escape_right_br(char c){
	switch(c){
		default:
			my_printf("ESCAPE_RIGTH_BR %c\n", c);
			status=MAIN;
			break;
	}
}

void Console::analise_escape_question(char c){
	if(isdigit(c)||c==';'){
		clear_args();
		last_status=ESCAPE_QUESTION;
		status=ARGS;
		analise_args(c);
	}else
		switch(c){
			default:
				my_printf("ESCAPE_QUESTION %c\n", c);
				status=MAIN;
				break;
		}
}

void Console::analise_escape(char c){
	switch(c){
		case '[':
			status=CSI;
			clear_args();
			break;
		case '%':
			status=MAIN;
			my_printf("%%!!!\n");
			clear_args();
			break;
		case '#':
			status=ESCAPE_HASH;
			clear_args();
			break;
		case '(':
			status=ESCAPE_LEFT_BR;
			clear_args();
			break;
		case ')':
			status=ESCAPE_RIGTH_BR;
			clear_args();
			break;
		case '7':
			saved_cursor_x=cursor_x, saved_cursor_y=cursor_y;
			status=MAIN;
			break;
		case '8':
			cursor_x=saved_cursor_x, cursor_y=saved_cursor_y;
			status=MAIN;
			break;
		default:
			my_printf("%c\n", c);
			status=MAIN;
			break;
	}
}

void Console::put_char(char c){//todo UTF-8
	switch(c){
		case 0x07:
			vm_vibrator_once();
			break;
		case 0x08:
			main_text[cursor_y][cursor_x].reset();
			previos_p();
			break;
		case 0x09:
		case 0x0B:
		case 0x0C:
		case 0x7F:
			putchar('0');
			putchar('x');
			putchar(Hex_ch[(c&0xF0)>>8]);
			putchar(Hex_ch[c&0x0F]);
			break;
		case 0x0A:
			new_line();
			break;
		case 0x0D:
			cursor_x=0;
			break;
		case 0x1B://ESC
			status = ESCAPE;
			break;
		default:
			if(c==0)
				break;
			main_text[cursor_y][cursor_x].ch=c;
			main_text[cursor_y][cursor_x].flgs=flgs;
			main_text[cursor_y][cursor_x].textcolor=cur_textcolor;
			main_text[cursor_y][cursor_x].backcolor=cur_backcolor;
			draw_cur_char();
			next_p();
			
			break;

	}
	//vm_graphic_flush_layer(layer_hdls, 1);
}

void Console::put_c_(char c){
	switch(status){
		case MAIN:
			put_char(c);
			break;
		case ESCAPE:
			analise_escape(c);
			break;
		case ARGS:
			analise_args(c);
			break;
		case CSI:
			analise_CSI(c);
			break;
		case ESCAPE_HASH:
			analise_escape_hash(c);
			break;
		case ESCAPE_LEFT_BR:
			analise_escape_left_br(c);
			break;
		case ESCAPE_RIGTH_BR:
			analise_escape_right_br(c);
			break;
		case ESCAPE_QUESTION:
			analise_escape_question(c);
			break;
	}
	//vm_graphic_flush_layer_non_block(layer_hdls, 1);
	//vm_graphic_flush_layer(layer_hdls, 1);
}

void Console::put_c(char c){
	if(UTF_l==0) 
		if(c&0x80)
			if((c&0xE0)==0xC0)
				UTF_l=2, UTF_i=1, UTF[0]=c;
			else if((c&0xF0)==0xE0)
				UTF_l=3, UTF_i=1, UTF[0]=c;
			else if((c&0xF8)==0xF0)
				UTF_l=4, UTF_i=1, UTF[0]=c;
			else
				put_c_(c);
		else
			put_c_(c);
	else{
		if((c&0xC0)==0x80){
			UTF[UTF_i++]=c;
			if(UTF_i==UTF_l){
				unicode=0;
				switch(UTF_l){
					case 2:
						unicode=((UTF[0]&0x1F)<<6)|UTF[1]&0x3F;
						break;
					case 3:
						unicode=((((UTF[0]&0xF)<<6)|UTF[1]&0x3F)<<6)|UTF[2]&0x3F;
						break;
					case 4:
						unicode=((((((UTF[0]&0x7)<<6)|UTF[1]&0x3F)<<6)|UTF[2]&0x3F)<<6)|UTF[3]&0x3F;
						break;
				}
				if(unicode==0x2584)
					put_c_(220);
				else
					put_c_(unicode&0xFF);
				UTF_l=0;
			}
		}else{
			for(int i=0; i<UTF_i; ++i)
				put_c_(UTF[i]);
			put_c_(c);
		}
	}
}

void Console::putstr(const char *str, int len){
	if(len==-1)
		for(int i=0;str[i];++i)
			put_c(str[i]);
	else
		for(int i=0;i<len;++i)
			put_c(str[i]);
}


void Console::previos_p(){
	if(cursor_x>0)
		cursor_x--;
}
void Console::next_p(){
	if(cursor_x>=terminal_w-1){//TODO: fix this
		new_line();
	}else{
		cursor_x++;
	}
}

void Console::new_line(){
	cursor_y=cursor_y+1;
	cursor_x=0;
	if(cursor_y>=scroll_end_row){
		cursor_y=scroll_end_row-1;
		scroll(1);
	}
}

void Console::scroll(int v){
	if(scroll_value+v<0)
		v=-scroll_value;

	if(scroll_value+v>=count_of_lines)//todo
		v=count_of_lines-1-scroll_value;
	
	int scroll_height = scroll_end_row - scroll_start_row;

	if(v>scroll_height){
		scroll(v-scroll_height);//todo
		scroll(v);
		return;
		//v=scroll_height;
	}
	if(v<-scroll_height){
		scroll(+scroll_height+v);//todo
		scroll(v);
		return;
		//v=-scroll_height;
	}
	if(v>0){
		for(int i=0; i<v; ++i){
			scroll_temp_text[i]=history_text[i+scroll_value];  //1
			history_text[i+scroll_value]=main_text[i+scroll_start_row]; //2
		}
		for(int i=0; i<scroll_height-v; ++i)
			main_text[i+scroll_start_row]=main_text[scroll_start_row+i+v];//3
		
		for(int i=0; i<v; ++i)
			main_text[i+scroll_end_row-v]=scroll_temp_text[i];//4

		for(int i=scroll_start_row*char_height;i <(scroll_end_row-v)*char_height; ++i) //redraw
			for(int j=0; j<scr_w; ++j)
				((unsigned short*)scr_buf)[i*scr_w+j]=((unsigned short*)scr_buf)[(i+v*char_height)*scr_w+j];

		for(int i=0; i<v; ++i)
			for(int j=0; j<terminal_w; ++j)
				draw_xy_char(j, i+scroll_end_row-v);

		scroll_value+=v;

	}else if(v<0){
		v=-v;
		for(int i=0; i<v; ++i){
			scroll_temp_text[i]=history_text[i-v+scroll_value];  //1
			history_text[i-v+scroll_value]=main_text[i+scroll_end_row-v]; //2
		}
		for(int i=0; i<scroll_height-v; ++i)
			main_text[scroll_end_row-i-1]=main_text[scroll_end_row-i-1-v];//3
		
		for(int i=0; i<v; ++i)
			main_text[i+scroll_start_row]=scroll_temp_text[i];//4

		for(int i=(scroll_end_row)*char_height-1; i>=(scroll_start_row+v)*char_height; --i) //redraw
			for(int j=0; j<scr_w; ++j)
				((unsigned short*)scr_buf)[i*scr_w+j]=((unsigned short*)scr_buf)[(i-v*char_height)*scr_w+j];

		for(int i=0; i<v; ++i)
			for(int j=0; j<terminal_w; ++j)
				draw_xy_char(j, i);

		scroll_value-=v;
		//todo
	}

}

void Console::draw_xy_char(int x, int y){
	const unsigned char *font_ch=ProFont6x11+5 + 12*main_text[y][x].ch + 1;
	unsigned short textcolor = main_text[y][x].textcolor , backcolor = main_text[y][x].backcolor; 
	if(main_text[y][x].flgs&1)
		textcolor=~textcolor, backcolor=~backcolor;

	for(int i=0;i<char_height;++i){
		unsigned short* scr_buf= (unsigned short*)this->scr_buf + x*char_width+(y*char_height+i)*scr_w;
		for(int j=0;j<char_width;++j)
				scr_buf[j]=((((*font_ch)>>j)&1)?textcolor:backcolor);
		++font_ch;
	}
}

void Console::draw_cur_char(){
	const unsigned char *font_ch=ProFont6x11+5 + 12*main_text[cursor_y][cursor_x].ch + 1;
	unsigned short textcolor = cur_textcolor , backcolor = cur_backcolor; 
	if(flgs&1)
		textcolor=~textcolor, backcolor=~backcolor;

	for(int i=0;i<char_height;++i){
		unsigned short* scr_buf= (unsigned short*)this->scr_buf + cursor_x*char_width+(cursor_y*char_height+i)*scr_w;
		for(int j=0;j<char_width;++j)
				scr_buf[j]=((((*font_ch)>>j)&1)?cur_textcolor:cur_backcolor);
		++font_ch;
	}
}

void Console::draw_all(){
	vm_graphic_fill_rect(scr_buf, 0, 0, scr_w, scr_h, main_color, main_color);
	for (int i = 0; i < terminal_h; ++i)
		for (int j = 0; j < terminal_w; ++j)
			draw_xy_char(j, i);
}



void Console::reset(){
	cursor_x = cursor_y = saved_cursor_x = saved_cursor_y = 0;
	last_status=status=MAIN;
	cur_textcolor=0xFFFF, cur_backcolor=0x0000; 
	bright=0;
}

void Console::init(){
	terminal_w=scr_w/char_width;
	terminal_h=scr_h/char_height;

	scroll_start_row = 0;
	scroll_end_row = terminal_h;
	scroll_value = 0;

	UTF_l=0;

	main_color = 0x0000;

	narg=16;
	clear_args();

	scr_buf = 0;

	reset();

	main_text=(Symbol**)vm_malloc((terminal_h+1)*sizeof(Symbol*));

	for(int i=0; i<=terminal_h; ++i){
		main_text[i]=(Symbol*)vm_malloc((terminal_w+1)*sizeof(Symbol));
		for(int j=0; j<= terminal_w; ++j)
			main_text[i][j].reset();
	}

	scroll_temp_text=(Symbol**)vm_malloc((terminal_h+1)*sizeof(Symbol*));
		

	for(int i=0; i<count_of_lines; ++i){
		history_text[i]=(Symbol*)vm_malloc((terminal_w+1)*sizeof(Symbol));
		for(int j=0; j<= terminal_w; ++j)
			history_text[i][j].reset();
	}
}

Console::~Console(void){
	for(int i=0;i<terminal_h;++i)
		vm_free(main_text[i]);
	vm_free(main_text);

	vm_free(scroll_temp_text);
	
	for(int i=0;i<count_of_lines;++i)
		vm_free(history_text[i]);
}
