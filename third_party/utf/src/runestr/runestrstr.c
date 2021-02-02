/* See LICENSE file for copyright and license details. */
#include "utf.h"

Rune *
runestrstr(const Rune *s1, const Rune *s2)
{
	const Rune *p1, *p2;
	Rune r0, r1, r2;

	r0 = *s2;
	for(s1 = runestrchr(s1, r0); *s1 != 0; s1 = runestrchr(s1+1, r0))
		for(p1 = s1, p2 = s2;; p1++, p2++) {
			r2 = *p2;
			if(r2 == 0)
				return (Rune *)s1;
			r1 = *p1;
			if(r1 != r2)
				break;
		}

	return NULL;
}
