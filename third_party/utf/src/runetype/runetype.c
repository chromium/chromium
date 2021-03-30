/* See LICENSE file for copyright and license details. */
#include "runetype.h"

int
rune1cmp(const void *v1, const void *v2)
{
	Rune r1 = *(Rune *)v1;
	Rune r2 = *(Rune *)v2;

	return r1 - r2;
}

int
rune2cmp(const void *v1, const void *v2)
{
	Rune r = *(Rune *)v1;
	Rune *p = (Rune *)v2;

	return r >= p[0] && r <= p[1] ? 0 : r - p[0];
}
