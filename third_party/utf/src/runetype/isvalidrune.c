#include "utf.h"

int
isvalidrune(Rune r)
{
	if(r < 0)
		return 0; /* negative value */

	if(r >= 0xD800 && r <= 0xDFFF)
		return 0; /* surrogate pair range */

	if(r >= 0xFDD0 && r <= 0xFDEF)
		return 0; /* non-character range */

	if((r & 0xFFFE) == 0xFFFE)
		return 0; /* non-character at end of plane */

	if(r > 0x10FFFF)
		return 0; /* too large, thanks to UTF-16 */

	return 1;
}
