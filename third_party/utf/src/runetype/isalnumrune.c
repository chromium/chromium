/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
isalnumrune(Rune r)
{
	return isalpharune(r) || isdigitrune(r);
}
