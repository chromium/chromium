/* See LICENSE file for copyright and license details. */
#include "utf.h"

/* lookup table for the number of bytes expected in a sequence */
const unsigned char utftab[64] = {
        0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 1100xxxx */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 1101xxxx */
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 1110xxxx */
        4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 0, 0, /* 1111xxxx */
};

