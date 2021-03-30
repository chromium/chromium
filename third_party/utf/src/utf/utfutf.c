/* See LICENSE file for copyright and license details. */
#include <string.h>
#include "utf.h"

char *
utfutf(const char *s1, const char *s2)
{
	const char *p;
	int n1, n2;
	Rune r;

	n1 = chartorune(&r, s2);
	if(r < Runeself)
		return strstr(s1, s2);

	n2 = strlen(s2);
	for(p = s1; (p = utfrune(p, r)); p += n1)
		if(!strncmp(p, s2, n2))
			return (char *)p;

	return NULL;
}
