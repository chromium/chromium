/* See LICENSE file for copyright and license details. */
#include "utf.h"

/* lookup table for the number of bytes expected in a sequence */
const unsigned char utftab[64] = {
	0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 1100xxxx */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 1101xxxx */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 1110xxxx */
	4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 0, 0, /* 1111xxxx */
};

int
charntorune(Rune *p, const char *s, size_t len)
{
	unsigned char c, i, m, n, x;
	Rune r;

	if(len == 0) /* can't even look at s[0] */
		return 0;

	c = *s++;

	if(!(c & 0200)) /* basic byte */
		return (*p = c, 1);

	if(!(c & 0100)) /* continuation byte */
		return (*p = Runeerror, 1);

	n = utftab[c & 077];

	if(n == 0) /* illegal byte */
		return (*p = Runeerror, 1);

	if(len == 1) /* reached len limit */
		return 0;

	if((*s & 0300) != 0200) /* not a continuation byte */
		return (*p = Runeerror, 1);

	x = 0377 >> n;
	r = c & x;

	r = (r << 6) | (*s++ & 077);

	if(r <= x) /* overlong sequence */
		return (*p = Runeerror, 2);

	m = (len < n) ? len : n;

	for(i = 2; i < m; i++) {
		if((*s & 0300) != 0200) /* not a continuation byte */
			return (*p = Runeerror, i);

		r = (r << 6) | (*s++ & 077);
	}

	if(i < n) /* must have reached len limit */
		return 0;

	if(!isvalidrune(r))
		return (*p = Runeerror, i);

	return (*p = r, i);
}

int
chartorune(Rune *p, const char *s)
{
	return charntorune(p, s, UTFmax);
}
