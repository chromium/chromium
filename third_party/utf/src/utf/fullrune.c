/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
fullrune(const char *s, size_t len)
{
	unsigned char c, i, m, n, x;
	Rune r;

	if(len == 0) /* can't even look at s[0] */
		return 0;

	c = *s++;

	if((c & 0300) != 0300) /* not a leading byte */
		return 1;

	n = utftab[c & 077];

	if(len >= n) /* must be long enough */
		return 1;

	if(len == 1) /* reached len limit */
		return 0;

	/* check if an error means this rune is full */

	if((*s & 0300) != 0200) /* not a continuation byte */
		return 1;

	x = 0377 >> n;
	r = c & x;

	r = (r << 6) | (*s++ & 077);

	if(r <= x) /* overlong sequence */
		return 1;

	m = len; /* we already know that len < n */

	for(i = 2; i < m; i++) {
		if((*s & 0300) != 0200) /* not a continuation byte */
			return 1;

		s++;
	}

	return 0;
}
