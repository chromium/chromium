# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Constant definitions for GRIT.
'''


# This is the Icelandic noun meaning "grit" and is used to check that our
# input files are in the correct encoding.  The middle character gets encoded
# as two bytes in UTF-8, so this is sufficient to detect incorrect encoding.
ENCODING_CHECK = 'm\u00f6l'

# A special language, translations into which are always "TTTTTT".
CONSTANT_LANGUAGE = 'x_constant'

PSEUDOLOCALE_LONG_STRINGS = 'en-XA'
PSEUDOLOCALE_RTL = 'ar-XB'
PSEUDOLOCALES = [PSEUDOLOCALE_LONG_STRINGS, PSEUDOLOCALE_RTL]

# Magic number added to the header of resources brotli compressed by grit. Used
# to easily identify resources as being brotli compressed. See
# ui/base/resource/resource_bundle.h for decompression usage.
BROTLI_CONST = b'\x1e\x9b'


# Gender-related constants.
GENDER_OTHER = 'OTHER'
GENDER_MASCULINE = 'MASCULINE'
GENDER_FEMININE = 'FEMININE'
GENDER_NEUTER = 'NEUTER'

DEFAULT_GENDER = GENDER_OTHER
TRANSLATED_GENDERS = (GENDER_MASCULINE, GENDER_FEMININE, GENDER_NEUTER)
ALL_GENDERS = (DEFAULT_GENDER, ) + TRANSLATED_GENDERS
