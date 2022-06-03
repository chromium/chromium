/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
ispunctrune(Rune r)
{
	return isgraphrune(r) && !isalnumrune(r);
}
