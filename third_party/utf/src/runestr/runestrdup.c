/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include "utf.h"

Rune *
runestrdup(const Rune *s)
{
	size_t n;
	Rune *p;

	n = runestrlen(s) + 1;
	if(!(p = malloc(n * sizeof *p)))
		return NULL;

	return runestrncpy(p, s, n);
}
