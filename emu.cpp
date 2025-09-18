#include "emu.h"


/*
*	Composantes d'emulations
*
	|CD roms|
	|CPU|
	|GPU|
	|GTE (Geometry transformation Engine)|
	|MDEC (Macroblock Decoder|
	|SPU|
	|Timer|
	|Address Bus|

*/

static emu_context ctx;

emu_context* emu_get_cintext() {
	return &ctx;
}

void delay(u32 ms) {
	
}


