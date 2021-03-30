/* See LICENSE file for copyright and license details. */
#include <string.h>
#include "utf.h"

char *
utfrrune(const char *s, Rune r)
{
	const char *p = NULL;
	Rune r0;
	int n;

	if(r < Runeself)
		return strrchr(s, r);

	for(; *s != '\0'; s += n) {
		n = chartorune(&r0, s);
		if(r == r0)
			p = s;
	}
	return (char *)p;
}
