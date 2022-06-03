/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
runetochar(char *s, const Rune *p)
{
	unsigned char i, n, x;
	Rune r = *p;

	n = runelen(r);

	if(n == 1) {
		s[0] = r;
		return 1;
	}

	if(n == 0)
		return 0;

	for(i = n; --i > 0; r >>= 6)
		s[i] = 0200 | (r & 077);

	x = 0377 >> n;
	s[0] = ~x | r;

	return n;
}
