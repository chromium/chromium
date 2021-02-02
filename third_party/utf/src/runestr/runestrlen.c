/* See LICENSE file for copyright and license details. */
#include "utf.h"

size_t
runestrlen(const Rune *s)
{
	size_t i = 0;

	for(i = 0; *s != 0; s++, i++)
		;
	return i;
}
