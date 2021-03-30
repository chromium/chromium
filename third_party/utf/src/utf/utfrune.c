/* See LICENSE file for copyright and license details. */
#include <string.h>
#include "utf.h"

char *
utfrune(const char *s, Rune r)
{
	char buf[UTFmax+1];
	int n;

	if(r < Runeself)
		return strchr(s, r);

	if(!(n = runetochar(buf, &r)))
		return NULL;

	buf[n] = '\0';
	return strstr(s, buf);
}
