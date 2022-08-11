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

#ifdef WIN32
#define CYCLES_PER_CALL 1000000 // Cycles excuted per call to socRun
#else
#define CYCLES_PER_CALL 2000 // Cycles excuted per call to socRun
#endif

#define SCREEN_FPS 20

// Global variables
int scr_w; 
int scr_h;
unsigned int last_wr_addr = 0, last_rd_addr = 0;

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

typedef VMINT(*vm_get_sym_entry_t)(char* symbol);
extern vm_get_sym_entry_t vm_get_sym_entry;

typedef VMINT (*vm_file_seek_t)(VMFILE handle, VMINT offset, VMINT base);
typedef VMINT (*vm_file_read_t)(VMFILE handle, void* data, VMUINT length, VMUINT* nread);
typedef VMINT (*vm_file_write_t)(VMFILE handle, void* data, VMUINT length, VMUINT* written);

vm_file_seek_t vm_file_seek_opt = 0;
vm_file_read_t vm_file_read_opt = 0;
vm_file_write_t vm_file_write_opt = 0;

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
	if(!fifo_is_empty(serial_in)) {
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
			if(vm_file_seek_opt(sd, sector * BLK_DEV_BLK_SZ, BASE_BEGIN) < 0) // Seek
				vm_exit_app(); // If error -> exit app

			VMUINT r;
			vm_file_read(sd, buf, BLK_DEV_BLK_SZ, &r);
			return r == BLK_DEV_BLK_SZ;
		
		case BLK_OP_WRITE:
			// Commit data to file
			//vm_file_commit_opt(sd);

			// Write to a block
			if(vm_file_seek_opt(sd, sector * BLK_DEV_BLK_SZ, BASE_BEGIN) < 0) // Seek
				vm_exit_app(); // If error -> exit app

			VMUINT w;
			vm_file_write_opt(sd, buf, BLK_DEV_BLK_SZ, &w);
			return w == BLK_DEV_BLK_SZ;
	}
	return 0;	
}

Boolean coRamAccess(_UNUSED_ CalloutRam* ram, UInt32 addr, UInt8 size, Boolean write, void* bufP){

	UInt8* b = bufP;

	addr &= 0xFFFFFF;
	
	if(write) {
		last_wr_addr = addr;
		// Commit data to file
		//vm_file_commit(vram);

		// Write to a block
		if(vm_file_seek_opt(vram, addr, BASE_BEGIN) < 0) // Seek
			vm_exit_app(); // If error -> exit app
		
		VMUINT w;
		vm_file_write_opt(vram, b, size, &w);
	}
	else {
		last_rd_addr = addr;
		// Read from a block
		if(vm_file_seek_opt(vram, addr, BASE_BEGIN) < 0) // Seek
			vm_exit_app(); // If error -> exit app
		
		VMUINT r;
		vm_file_read_opt(vram, b, size, &r);
	}

	return true;
}

// Main MRE entry point
void vm_main(void){
	vm_file_seek_opt = vm_get_sym_entry("vm_file_seek");
	vm_file_read_opt = vm_get_sym_entry("vm_file_read");
	vm_file_write_opt = vm_get_sym_entry("vm_file_write");

	serial_in = fifo_create(BUF_SIZE, sizeof(int));

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

// SoC cycle
void socRun(int tid){
	unsigned long i = 0;
	for (i=0; i < CYCLES_PER_CALL; ++i) {
		cycles++; // Increase the cycle

		// Check if it's needed to update the devices' status
		if (!(cycles & 0x000007UL)) pxa255timrTick(&soc.timr);
		if (!(cycles & 0x0000FFUL)) pxa255uartProcess(&soc.ffuart);
		if (!(cycles & 0x000FFFUL)) pxa255rtcUpdate(&soc.rtc);
		if (!(cycles & 0x01FFFFUL)) pxa255lcdFrame(&soc.lcd);

		cpuCycle(&soc.cpu); // Emulate a single CPU cycle
	}
}

// Refresh screen
void timer(int tid) {
	draw();
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

		if (message == VM_MSG_CREATE) {
			// Convert file path to ucs2
			vm_gb2312_to_ucs2(sd_path, 1000, SD_FILE);
			vm_gb2312_to_ucs2(vram_path, 1000, VRAM_FILE);

			// Open SD file and RAM file
			sd = vm_file_open(sd_path, // Virtual disk file named "jaunty.rel.v2" (you can change yourself)
				MODE_APPEND,           // Open in append mode
				VM_TRUE);              // Open in binary mode

			wchar_t* sd_path2 = sd_path;
			
			// If old RAM file available -> we just use it, don't need to generate again
			vram = vm_file_open(vram_path, // Virtual ram file named "ram.bin" (you can change yourself)
				MODE_APPEND,               // Open in append mode
				VM_TRUE);                  // Open in binary mode

			if(vram<0) // If the file is not existed
				vram = vm_file_open(vram_path, // Virtual ram file named "ram.bin" (you can change yourself)
					MODE_CREATE_ALWAYS_WRITE,  // Create a new file and open it in read/write mode
					VM_TRUE);                  // Open in binary mode

			
			console_str_in("\n");
			{
				vm_file_seek(vram, 0, BASE_END);
				int file_size = vm_file_tell(vram);
				vm_file_seek(vram, 0, BASE_BEGIN);

				if (file_size != 1024 * 1024 * 16) {
					VMUINT writen;
					{
						int i = 0;
						for (; i < 16 * 1024; ++i) {
							vm_file_write_opt(vram, zero_array, 1024, &writen);
							if (i % 1024 == 0) {
								char tmp[100];
								sprintf(tmp, "\rGenerating ram %d/%d MB", (i / 1024)+1, 16);
								console_str_in(tmp);
								draw();
							}
						}
					}
					{
						char tmp[100];
						sprintf(tmp, "... Done\n");
						console_str_in(tmp);
						draw();
					}
				}
				else {
					char tmp[100];
					sprintf(tmp, "Found old RAM file\n");
					console_str_in(tmp);
					draw();
				}
			}

			if (sd < 0 || vram < 0) {
				console_str_in("Cannot open SD card file or VRAM file\n");
				vm_exit_app(); // Error -> exit :)
			}

			socInit(&soc, socRamModeCallout, coRamAccess, readchar, writechar, rootOps, NULL); // Init SoC

			if (1) {	//hack for faster boot in case we know all variables & button is pressed
				UInt32 i, s = 786464UL;
				UInt32 d = 0xA0E00000;
				UInt16 j;
				UInt8* b = (UInt8*)soc.blkDevBuf;

				for (i = 0; i < 4096; i++) {
					if (vm_file_seek_opt(sd, (s++) * BLK_DEV_BLK_SZ, BASE_BEGIN) < 0) // Seek
						vm_exit_app(); // If error -> exit app

					VMUINT r;
					vm_file_read_opt(sd, b, BLK_DEV_BLK_SZ, &r);

					if (i % 512 == 0) {
						char tmp[100];
						sprintf(tmp, "\rBoost launch  %d/%d", i+512, 4096);
						console_str_in(tmp);
						draw();
					}

					//sdSecRead(&sd, s++, b);
					for (j = 0; j < 512; j += 32, d += 32) {
						VMUINT w;
						if (vm_file_seek_opt(vram, d & 0xFFFFFF, BASE_BEGIN) < 0) // Seek
							vm_exit_app(); // If error -> exit app

						vm_file_write_opt(vram, b + j, 32, &w);
					}
				}
				soc.cpu.regs[15] = 0xA0E00000UL + 512UL;
				console_str_in("... Done\n");
			}
		}

		if(soc_cycle_timer_id == -1)
			soc_cycle_timer_id = vm_create_timer(0, socRun);

		if(screen_timer_id==-1)
			screen_timer_id = vm_create_timer(1000/SCREEN_FPS, timer); // terminal refresh
		break;
		
	case VM_MSG_PAINT:
		draw();
		break;
		
	case VM_MSG_INACTIVE:
		vm_switch_power_saving_mode(turn_on_mode);
		if( layer_hdls[0] != -1 ){
			vm_graphic_delete_layer(layer_hdls[1]);
			vm_graphic_delete_layer(layer_hdls[0]);
			layer_hdls[0] = -1;
		}

		// Delete timers
		if(soc_cycle_timer_id != -1)
			vm_delete_timer(soc_cycle_timer_id);
		soc_cycle_timer_id = -1;
		if(screen_timer_id!=-1)
			vm_delete_timer(screen_timer_id);
		screen_timer_id = -1;

		// Commit data to file
		//vm_file_commit(sd);

		// Close file handlers
		//vm_file_close(sd);
		//vm_file_close(vram);
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
