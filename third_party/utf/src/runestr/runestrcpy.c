/* See LICENSE file for copyright and license details. */
#include <stdint.h>
#include "utf.h"

Rune *
runestrncpy(Rune *s1, const Rune *s2, size_t n)
{
	size_t i;

	for(i = 0; i < n && *s2 != 0; s1++, s2++, i++)
		*s1 = *s2;
	return s1;
}

Rune *
runestrcpy(Rune *s1, const Rune *s2)
{
	return runestrncpy(s1, s2, (size_t)-1);
}
