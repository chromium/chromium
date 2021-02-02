/* See LICENSE file for copyright and license details. */
#include <string.h>
#include "utf.h"

char *
utfecpy(char *to, char *end, const char *from)
{
	Rune r = -1;
	size_t i, n;

	for(i = 0; r != 0 && (n = charntorune(&r, &from[i], end - &to[i])) > 0; i += n)
		;
	memcpy(to, from, i);

	if(i > 0 && r != 0)
		to[i] = '\0';
	return &to[i];
}
