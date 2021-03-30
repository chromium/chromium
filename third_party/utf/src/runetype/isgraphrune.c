/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
isgraphrune(Rune r)
{
	return !isspacerune(r) && isprintrune(r);
}
