#pragma once
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"

#include "fifo.h" // fifo implementation

const unsigned short tr_color = VM_COLOR_888_TO_565(0, 255, 255);

#ifdef WIN32
#define my_printf(...) printf(__VA_ARGS__)
#else
#define my_printf(...)
#endif