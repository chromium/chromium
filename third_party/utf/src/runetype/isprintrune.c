/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
isprintrune(Rune r)
{
	return !iscntrlrune(r) && r != 0x2028 && r != 0x2029 && !(r >= 0xFFF9 && r <= 0xFFFB);
}
