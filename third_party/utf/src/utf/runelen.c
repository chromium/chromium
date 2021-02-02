/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
runelen(Rune r)
{
	if(!isvalidrune(r))
		return 0;
	else if(r < RUNE_C(1) << 7)
		return 1;
	else if(r < RUNE_C(1) << 11)
		return 2;
	else if(r < RUNE_C(1) << 16)
		return 3;
	else if(r < RUNE_C(1) << 21)
		return 4;
	else if(r < RUNE_C(1) << 26)
		return 5;
	else
		return 6;
}

size_t
runenlen(const Rune *p, size_t len)
{
	size_t i, k = 0;

	for(i = 0; i < len; i++)
		k += runelen(*p++);

	return k;
}
