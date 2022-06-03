/* See LICENSE file for copyright and license details. */
#include "utf.h"

Rune *
runestrecpy(Rune *to, Rune *end, Rune *from)
{
	Rune *p = to, *q = from;

	while(p < end && *q != 0)
		*p++ = *q++;

	if(p != to)
		*p = 0;

	return p;
}
