#include "disk_demo.h"

static CIODirEnt entry;

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, "D:*.*", CIO_DIR, 0);
	if (status != CIO_OK)
		return disk_result(status);

	while ((status = cio_dirread(channel, &entry)) < 128)
	{
		if (entry.kind == CIO_DE_FILE)
			con_printf("%s %s %u\n", entry.name, entry.ext, entry.sectors);
		else if (entry.kind == CIO_DE_FREE)
			con_printf("%u FREE SECTORS\n", entry.sectors);
	}
	byte closed = cio_close(channel);
	return disk_result(status == CIO_EOF ? closed : status);
}
