/* See LICENSE file for copyright and license details. */
#include <utf.h>
#include "tap.h"

#define CHECK(S,N,T,RS) do { \
	if(is(utflen(S), (N), RS" is "#N" runes long")) { \
		Rune r; \
		int i; \
		const char *p = (S); \
		for(i = 0; *p != '\0'; i++) { \
			p += chartorune(&r, p); \
			if(r != Runeerror && !((T) && i % 2 == 1 && r == ' ')) \
				break; \
		} \
		is(i, (N), RS" read as in error"); \
	} \
	else \
		skip(1, #S" is an unexpected number of runes long"); \
} while(0)

int
main(void)
{
	plan(46);

	CHECK("\x80", 1, 0, "lone smallest continuation byte");
	CHECK("\xBF", 1, 0, "lone largest continuation byte");

	CHECK("\x80\xBF", 2, 0, "2 continuation bytes");
	CHECK("\x80\xBF\x80", 3, 0, "3 continuation bytes");
	CHECK("\x80\xBF\x80\xBF", 4, 0, "4 continuation bytes");
	CHECK("\x80\xBF\x80\xBF\x80", 5, 0, "5 continuation bytes");
	CHECK("\x80\xBF\x80\xBF\x80\xBF", 6, 0, "6 continuation bytes");
	CHECK("\x80\xBF\x80\xBF\x80\xBF\x80", 7, 0, "7 continuation bytes");

	CHECK("\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
	      "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
	      "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
	      "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF", 64, 0, "all 64 continuation bytes");
	CHECK("\xC0 \xC1 \xC2 \xC3 \xC4 \xC5 \xC6 \xC7 \xC8 \xC9 \xCA \xCB \xCC \xCD \xCE \xCF "
	      "\xD0 \xD1 \xD2 \xD3 \xD4 \xD5 \xD6 \xD7 \xD8 \xD9 \xDA \xDB \xDC \xDD \xDE \xDF ", 64, 1, "all 32 2-leading bytes spaced");
	CHECK("\xE0 \xE1 \xE2 \xE3 \xE4 \xE5 \xE6 \xE7 \xE8 \xE9 \xEA \xEB \xEC \xED \xEE \xEF ", 32, 1, "all 16 3-leading bytes spaced");
	CHECK("\xF0 \xF1 \xF2 \xF3 \xF4 \xF5 \xF6 \xF7 ", 16, 1, "all 8 4-leading bytes spaced");
	CHECK("\xF8 \xF9 \xFA \xFB ", 8, 1, "all 4 5-leading bytes spaced");
	CHECK("\xFC \xFD ", 4, 1, "all 2 6-leading bytes spaced");

	CHECK("\xDF", 1, 0, "2-byte sequence U+000007FF with last byte missing");
	CHECK("\xEF\xBF", 1, 0, "3-byte sequence U+0000FFFF with last byte missing");
	CHECK("\xF7\xBF\xBF", 1, 0, "4-byte sequence U+001FFFFF with last byte missing");
	CHECK("\xFB\xBF\xBF\xBF", 1, 0, "5-byte sequence U+03FFFFFF with last byte missing");
	CHECK("\xFD\xBF\xBF\xBF\xBF", 1, 0, "6-byte sequence U+7FFFFFFF with last byte missing");

	CHECK("\xDF\xEF\xBF\xF7\xBF\xBF\xFB\xBF\xBF\xBF\xFD\xBF\xBF\xBF\xBF", 5, 0, "5 incomplete sequences");

	CHECK("\xFE", 1, 0, "impossible sequence \"\\xFE\"");
	CHECK("\xFF", 1, 0, "impossible sequence \"\\xFF\"");
	CHECK("\xFE\xFE\xFF\xFF", 4, 0, "impossible sequences \"\\xFE\\xFE\\xFF\\xFF\"");

	return 0;
}
