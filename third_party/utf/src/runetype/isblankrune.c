/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
isblankrune(Rune r)
{
	return r == ' ' || r == '\t';
}
