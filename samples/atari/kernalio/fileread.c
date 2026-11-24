#include "scores.h"

static Score scores[SCORE_COUNT];
static word transferred;

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, SCORE_PATH, CIO_READ, 0);
	if (status != CIO_OK)
		return disk_result(status);
	status = cio_read(channel, scores, sizeof(scores), &transferred);
	byte closed = cio_close(channel);
	if (cio_failed(status))
		return disk_result(status);
	if (closed != CIO_OK)
		return disk_result(closed);
	if (transferred != sizeof(scores))
	{
		con_printf("Incomplete score file\n");
		return 1;
	}
	for (byte i = 0; i < SCORE_COUNT; ++i)
	{
		scores[i].name[4] = 0;
		con_printf("%s : %u\n", scores[i].name, scores[i].points);
	}
	return 0;
}
