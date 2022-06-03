/* See LICENSE file for copyright and license details. */
#include <stdint.h>
#include "utf.h"

Rune *
runestrncat(Rune *s0, const Rune *s2, size_t n)
{
	size_t i;
	Rune *s1;

	s1 = runestrchr(s0, 0);

	for(i = 0; i < n && *s2 != 0; s1++, s2++, i++)
		*s1 = *s2;
	*s1 = 0;

	return s0;
}

Rune *
runestrcat(Rune *s0, const Rune *s2)
{
	return runestrncat(s0, s2, (size_t)-1);
}
