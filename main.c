/*
 * uARMRE: uARM emulator on MRE platform
 * Date: 4/8/2022
 * Details: This is a combination of uARM by Dmitry Grinberg
 * and TelnetVXP's terminal by Ximik Boda.
 * Port to MRE by giangvinhloc610
 */

// MRE headers
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "ResID.h"
#include "string.h"
#include "vmtimer.h"

// C++ to C call :)
#include "Console_io.h"

// uARM's header
#include "SoC.h"
#include "callout_RAM.h"
#include "types.h"

// FIFO
#include "fifo.h"

// Macros
#define SD_FILE "e:\\uARMRE\\jaunty.rel.v2"
#define VRAM_FILE "e:\\uARMRE\\vram.bin"

// Global variables
int scr_w; 
int scr_h;

VMUINT8 *layer_bufs[2] = {0,0};
VMINT layer_hdls[2] = {-1,-1};

int screen_timer_id = -1;

int soc_cycle_timer_id = -1; // SoC cycle timer id
SoC soc; // uARM virtual SoC
UInt32 cycles = 0; // uARM's CPU cycle. Dmitry's note: make 64 if you REALLY need it... later

// File handlers for virtual SD card and virtual RAM disk
VMFILE sd;
VMFILE vram;

extern fifo_t serial_in;

// If native MRE -> binding functions
#ifndef WIN32
#define malloc vm_malloc
#define free vm_free

void _sbrk(){}
void _write(){}
void _close(){}
void _lseek(){}
void _open(){}
void _read(){}
void _exit(){}
void _getpid(){}
void _kill(){}
void _fstat(){}
void _isatty(){}
#endif

// Event handlers
void handle_sysevt(VMINT message, VMINT param);
void handle_keyevt(VMINT event, VMINT keycode);
void handle_penevt(VMINT event, VMINT x, VMINT y);

// uARM's function to read a *single* character from input
static int readchar(void) {
	if(!fifo_is_empty) {
		// FIFO buffer available to read
		int ret;
		fifo_get(serial_in, &ret);
		return ret;
	}

	return CHAR_NONE; // None available -> return CHAR_NONE
}

// uARM's function to write a *single* character to console
void writechar(int chr) {
	console_char_in((char)chr); // Write to console
}

// uARM's function to read sector from SD card (in our case a file)
int rootOps(_UNUSED_ void* userData, UInt32 sector, void* buf, UInt8 op){
	switch(op){
		case BLK_OP_SIZE:
			if(sector == 0){
				// get num blocks
				VMUINT sz;
				vm_file_getfilesize(sd, &sz);
				*(unsigned long*)buf = (unsigned long)sz / (unsigned long)BLK_DEV_BLK_SZ;
			}
			else if(sector == 1){
				// block size
				*(unsigned long*)buf = BLK_DEV_BLK_SZ; // Default 512 bytes
			}
			else return 0;
			return 1;
		
		case BLK_OP_READ:
			// Read from a block
			if(vm_file_seek(sd, sector * BLK_DEV_BLK_SZ, BASE_BEGIN) < 0) // Seek
				vm_exit_app(); // If error -> exit app

			VMUINT r;
			return vm_file_read(sd, buf, BLK_DEV_BLK_SZ, &r) == BLK_DEV_BLK_SZ;
		
		case BLK_OP_WRITE:
			// Commit data to file
			vm_file_commit(sd);

			// Write to a block
			if(vm_file_seek(sd, sector * BLK_DEV_BLK_SZ, BASE_BEGIN) < 0) // Seek
				vm_exit_app(); // If error -> exit app

			VMUINT w;
			return vm_file_write(sd, buf, BLK_DEV_BLK_SZ, &w) == BLK_DEV_BLK_SZ;
	}
	return 0;	
}

Boolean coRamAccess(_UNUSED_ CalloutRam* ram, UInt32 addr, UInt8 size, Boolean write, void* bufP){

	UInt8* b = bufP;

	addr &= 0xFFFFFF;
	
	if(write) {
		// Commit data to file
		vm_file_commit(vram);

		// Write to a block
		if(vm_file_seek(vram, addr, BASE_BEGIN) < 0) // Seek
			vm_exit_app(); // If error -> exit app
		
		VMUINT w;
		vm_file_write(vram, b, size, &w);
	}
	else {
		// Read from a block
		if(vm_file_seek(vram, addr, BASE_BEGIN) < 0) // Seek
			vm_exit_app(); // If error -> exit app
		
		VMUINT r;
		vm_file_read(vram, b, size, &r);
	}

	return true;
}

// Main MRE entry point
void vm_main(void){
	scr_w = vm_graphic_get_screen_width();
	scr_h = vm_graphic_get_screen_height();

	terminal_init();

	vm_reg_sysevt_callback(handle_sysevt);
	vm_reg_keyboard_callback(handle_keyevt);
	vm_reg_pen_callback(handle_penevt);
}

// Render terminal
void draw(){
	t2input_draw(layer_bufs[1]); // Call to C++
	vm_graphic_flush_layer(layer_hdls, 2); // Flush layer
} 

// Refresh screen
void timer(int tid){
	draw();
}

// SoC cycle
void socRun(int tid){
	cycles++; // Increase the cycle
	
	// Check if it's needed to update the devices' status
	if(!(cycles & 0x000007UL)) pxa255timrTick(&soc.timr);
	if(!(cycles & 0x0000FFUL)) pxa255uartProcess(&soc.ffuart);
	if(!(cycles & 0x000FFFUL)) pxa255rtcUpdate(&soc.rtc);
	if(!(cycles & 0x01FFFFUL)) pxa255lcdFrame(&soc.lcd);
	
	cpuCycle(&soc.cpu); // Emulate a single CPU cycle
}

void err_str(const char* str){
	
	char c;
	
	while((c = *str++) != 0) writechar(c);
}

UInt32 rtcCurTime(void){
	VMUINT ret;
	vm_get_utc(&ret); // Get time since Epoch
	return (UInt32)ret;
}

void* emu_alloc(_UNUSED_ UInt32 size){
	
	err_str("No allocations in MRE mode please!");
	
	return 0;
}

// System event handler
void handle_sysevt(VMINT message, VMINT param) {
	VMWCHAR sd_path[100];
	VMWCHAR vram_path[100];
	unsigned char zero_array[1024] = {0};
	switch (message) {
	case VM_MSG_CREATE:
	case VM_MSG_ACTIVE:
		layer_hdls[0] = vm_graphic_create_layer(0, 0, scr_w, scr_h, -1);
		layer_hdls[1] = vm_graphic_create_layer(0, 0, scr_w, scr_h, tr_color);
		
		vm_graphic_set_clip(0, 0, scr_w, scr_h);

		layer_bufs[0]=vm_graphic_get_layer_buffer(layer_hdls[0]);
		layer_bufs[1]=vm_graphic_get_layer_buffer(layer_hdls[1]);

		vm_switch_power_saving_mode(turn_off_mode);

		set_layer_handler(layer_bufs[0], layer_bufs[1], layer_hdls[1]); // Call to C++

		// uARM init code

		// Convert file path to ucs2
		

		vm_gb2312_to_ucs2(sd_path, 1000, SD_FILE);
		vm_gb2312_to_ucs2(vram_path, 1000, VRAM_FILE);	

		// Open SD file and RAM file
		sd = vm_file_open(sd_path, // Virtual disk file named "jaunty.rel.v2" (you can change yourself)
			MODE_APPEND,      // Read-Write mode, create file if not exist
			VM_TRUE);                         // Open in binary mode

		wchar_t* sd_path2 = sd_path;
		// Delete old "vram.bin" (if any)
		vm_file_delete(vram_path);
		vram = vm_file_open(vram_path, // Virtual ram file named "ram.bin" (you can change yourself)
			MODE_CREATE_ALWAYS_WRITE,  // Read-Write mode, create file if not exist
			VM_TRUE);                     // Open in binary mode

		VMUINT writen; 
		{
			int i = 0;
			for (; i < 16 * 1024; ++i) {
				vm_file_write(vram, zero_array, 1024, &writen);
				if (i % 1024 == 0) {
					char tmp[100];
					sprintf(tmp, "Generating ram %d/%d MB\n", i/1024, 16);
					console_str_in(tmp);
					draw();
				}
			}
		}
		{
			char tmp[100];
			sprintf(tmp, "RAM is generating\n");
			console_str_in(tmp);
			draw();
		}

		if (sd < 0 && vram < 0)
			vm_exit_app(); // Error -> exit :)

		socInit(&soc, socRamModeCallout, coRamAccess, readchar, writechar, rootOps, NULL); // Init SoC

		if(soc_cycle_timer_id == -1)
			soc_cycle_timer_id = vm_create_timer(1, socRun); // 1 miliseconds each cycle -> 1KHz (slower than an AVR (5KHz :D)

		if(screen_timer_id==-1)
			screen_timer_id = vm_create_timer(1000/15, timer); // 15 fps (terminal refresh rate)
		break;
		
	case VM_MSG_PAINT:
		draw();
		break;
		
	case VM_MSG_INACTIVE:
		vm_switch_power_saving_mode(turn_on_mode);
		if( layer_hdls[0] != -1 ){
			vm_graphic_delete_layer(layer_hdls[0]);
			vm_graphic_delete_layer(layer_hdls[1]);
		}

		// Delete timers
		if(soc_cycle_timer_id != -1)
			vm_delete_timer(soc_cycle_timer_id);
		if(screen_timer_id!=-1)
			vm_delete_timer(screen_timer_id);

		// Commit data to file
		vm_file_commit(sd);

		// Close file handlers
		vm_file_close(sd);
		vm_file_close(vram);
		break;	
	case VM_MSG_QUIT:
		if( layer_hdls[0] != -1 ){
			vm_graphic_delete_layer(layer_hdls[0]);
			vm_graphic_delete_layer(layer_hdls[1]);
		}

		if(soc_cycle_timer_id != -1)
			vm_delete_timer(soc_cycle_timer_id);
		if(screen_timer_id!=-1)
			vm_delete_timer(screen_timer_id);

		// Commit data to file
		vm_file_commit(sd);

		// Close file handlers
		vm_file_close(sd);
		vm_file_close(vram);
		break;	
	}
}

// Keyboard event handler
void handle_keyevt(VMINT event, VMINT keycode) {
#ifdef WIN32
	if(keycode>=VM_KEY_NUM1&&keycode<=VM_KEY_NUM3)
		keycode+=6;
	else if(keycode>=VM_KEY_NUM7&&keycode<=VM_KEY_NUM9)
		keycode-=6;
#endif
	t2input_handle_keyevt(event, keycode);
}

// Touch event handler
void handle_penevt(VMINT event, VMINT x, VMINT y){
	t2input_handle_penevt(event, x, y);
	draw();
}