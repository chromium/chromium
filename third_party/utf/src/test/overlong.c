/* See LICENSE file for copyright and license details. */
#include <utf.h>
#include "tap.h"

#define CHECK(S,N,RS) do { \
	if(is(utflen(S), (N), RS" is "#N" runes long")) { \
		Rune r; \
		int i; \
		const char *p = (S); \
		for(i = 0; *p != '\0'; i++) { \
			p += chartorune(&r, p); \
			if(r != Runeerror) \
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
	plan(30);

	CHECK("\xC0\xAF", 2, "2-byte overlong U+002F SLASH");
	CHECK("\xE0\x80\xAF", 2, "3-byte overlong U+002F SLASH");
	CHECK("\xF0\x80\x80\xAF", 3, "4-byte overlong U+002F SLASH");
	CHECK("\xF8\x80\x80\x80\xAF", 4, "5-byte overlong U+002F SLASH");
	CHECK("\xFC\x80\x80\x80\x80\xAF", 5, "6-byte overlong U+002F SLASH");

	CHECK("\xC1\xBF", 2, "2-byte overlong U+0000007F DELETE");
	CHECK("\xE0\x9F\xBF", 2, "3-byte overlong U+000007FF");
	CHECK("\xF0\x8F\xBF\xBF", 3, "4-byte overlong U+0000FFFF <not a character>");
	CHECK("\xF8\x87\xBF\xBF\xBF", 4, "5-byte overlong U+001FFFFF <not a character>");
	CHECK("\xFC\x83\xBF\xBF\xBF\xBF", 5, "6-byte overlong U+03FFFFFF <not a character>");

	CHECK("\xC0\x80", 2, "2-byte overlong U+0000 NULL");
	CHECK("\xE0\x80\x80", 2, "3-byte overlong U+0000 NULL");
	CHECK("\xF0\x80\x80\x80", 3, "4-byte overlong U+0000 NULL");
	CHECK("\xF8\x80\x80\x80\x80", 4, "5-byte overlong U+0000 NULL");
	CHECK("\xFC\x80\x80\x80\x80\x80", 5, "6-byte overlong U+0000 NULL");

	return 0;
}
