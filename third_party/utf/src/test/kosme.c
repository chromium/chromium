/* See LICENSE file for copyright and license details. */
#include <utf.h>
#include "tap.h"

#define KOSME "\xCE\xBA\xE1\xBD\xB9\xCF\x83\xCE\xBC\xCE\xB5"

int
main(void)
{
	plan(7);

	if(is(utflen(KOSME), 5, "\""KOSME"\" is 5 runes long")) {
		Rune rbuf[6], *p;
		const char *s;
		int n;

		for(p = rbuf, s = KOSME; (n = chartorune(p, s)), *p != 0; p++, s += n)
			;

		is(rbuf[0], 0x03BA, "rune 0 of \""KOSME"\" is U+03BA GREEK SMALL LETTER KAPPA");
		is(rbuf[1], 0x1F79, "rune 1 of \""KOSME"\" is U+1F79 GREEK SMALL LETTER OMICRON WITH OXIA");
		is(rbuf[2], 0x03C3, "rune 2 of \""KOSME"\" is U+03C3 GREEK SMALL LETTER SIGMA");
		is(rbuf[3], 0x03BC, "rune 3 of \""KOSME"\" is U+03BC GREEK SMALL LETTER MU");
		is(rbuf[4], 0x03B5, "rune 4 of \""KOSME"\" is U+03B5 GREEK SMALL LETTER EPSILON");
		is(rbuf[5], 0x0000, "rune 5 of \""KOSME"\" is U+0000 NULL");
	}
	else
		skip(6, "\""KOSME"\" is an unexpected number of runes long");

	return 0;
}
