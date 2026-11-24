#include <atari/console.h>

#pragma section(resident, 0)
#pragma region(resident, 0x8000, 0x8100, , , {resident})
#pragma section(startup, 0)
#pragma region(startup, 0x2800, 0x2880, , , {startup})
#pragma region(main, 0x2880, 0x7800, , , {code, data, bss, heap, stack})

#pragma data(resident)
static const byte hello[] = "Hello World\x9b";
#pragma data(data)

#pragma code(resident)
// Run this routine at $8000 with the E: editor open on channel zero.
__asm resident_hello
{
		lda #$0b
		sta $0342
		lda #<hello
		sta $0344
		lda #>hello
		sta $0345
		lda #12
		sta $0348
		lda #0
		sta $0349
		ldx #0
		jsr $e456
		rts
}
#pragma code(code)

int main(void)
{
	con_printf("Resident routine installed at $%04X.\n", (word)resident_hello);
	return 0;
}
