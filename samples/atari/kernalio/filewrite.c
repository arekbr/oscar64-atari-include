#include "scores.h"

static const Score scores[SCORE_COUNT] = {
	{"AAA", 10000}, {"BBB", 9000}, {"CCC", 8000}, {"DDD", 4000}
};
static word transferred;

int main(void)
{
	byte channel = cio_free();
	byte status = open_new(channel, SCORE_PATH);
	if (status != CIO_OK)
		return disk_result(status);
	status = cio_write(channel, scores, sizeof(scores), &transferred);
	byte closed = cio_close(channel);
	if (status != CIO_OK)
		return disk_result(status);
	if (closed != CIO_OK)
		return disk_result(closed);
	if (transferred != sizeof(scores))
	{
		con_printf("Incomplete score write\n");
		return 1;
	}
	con_printf("Wrote SCORE.DAT\n");
	return 0;
}
