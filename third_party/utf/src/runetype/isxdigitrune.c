/* See LICENSE file for copyright and license details. */
#include "utf.h"

int
isxdigitrune(Rune r)
{
	return (r >= '0' && r <= '9') || (r >= 'A' && r <= 'F') || (r >= 'a' && r <= 'f');
}
