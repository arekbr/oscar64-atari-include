#include <atari/console.h>
#include <atari/dos.h>
#include <atari/input.h>

int main(void)
{
	con_printf("Press a key to return to DOS\n");
	key_getc();
	dos_exit();
	return 0;
}
