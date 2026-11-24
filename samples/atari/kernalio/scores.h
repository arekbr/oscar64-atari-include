#ifndef ATARI_SAMPLE_SCORES_H
#define ATARI_SAMPLE_SCORES_H

#include "disk_demo.h"

struct Score
{
	char name[5];
	word points;
};

#define SCORE_COUNT 4
#define SCORE_PATH "D:SCORE.DAT"

#endif
