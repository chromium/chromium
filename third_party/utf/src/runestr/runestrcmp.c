/* See LICENSE file for copyright and license details. */
#include <stdint.h>
#include "utf.h"

int
runestrncmp(const Rune *s1, const Rune *s2, size_t n)
{
	Rune r1, r2;
	size_t i;

	for(i = 0; i < n && *s1 != 0; s1++, s2++, i++) {
		r1 = *s1;
		r2 = *s2;
		if(r1 != r2)
			return r1 - r2;
	}

	return *s2;
}

int
runestrcmp(const Rune *s1, const Rune *s2)
{
	return runestrncmp(s1, s2, (size_t)-1);
}
