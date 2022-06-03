/* See LICENSE file for copyright and license details. */
#include "utf.h"

Rune *
runestrchr(const Rune *s, Rune r)
{
	for(; *s != 0; s++)
		if(*s == r)
			return (Rune *)s;

	return NULL;
}
