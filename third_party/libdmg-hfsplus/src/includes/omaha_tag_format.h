#ifndef __LIBDMG_HFSPLUS_OMAHA_TAG_DATA_FORMAT_H
#define __LIBDMG_HFSPLUS_OMAHA_TAG_DATA_FORMAT_H

#include "sizedbuf.h"

// Returns a SizedBuf* containing the binary-format Omaha tag containing the
// provided string, zero-padded to the maximum Omaha tag size (to allow space
// for future re-tagging). Terminates the program with a nonzero exit code if
// `arg` is too long to store in an Omaha tag. `arg` may be the empty string,
// producing an Omaha tag encoding no data.
//
// Ownership of the returned SizedBuf* is transfered to the caller.
//
// This function is intended for parsing command line arguments. See
// parse_data_param.h.
SizedBuf* ParseOmahaTagZone(const char* arg);

#endif  // __LIBDMG_HFSPLUS_OMAHA_TAG_DATA_FORMAT_H
