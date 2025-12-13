// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_UTIL_MAC_MAC_UTIL_H_
#define CRASHPAD_UTIL_MAC_MAC_UTIL_H_

#include <string>

namespace crashpad {

//! \brief Returns the version of the running operating system.
//!
//! \return The version of the operating system, such as `15'04'01` for macOS
//!     15.4.1.
//!
//! This function returns the major, minor, and bugfix components combined into
//! a single number. The format of the return value matches what is used by the
//! <Availability.h> `__MAC_OS_X_VERSION_MIN_REQUIRED`,
//! `__MAC_OS_X_VERSION_MAX_ALLOWED`, and per-version `__MAC_*` macros, for
//! versions since OS X 10.10.
//!
//! \note This is similar to the base::mac::IsOS*() family of functions, but is
//!     provided for situations where the caller needs to obtain version
//!     information beyond what is provided by Chromium’s base, or for when the
//!     caller needs the actual minor version value.
int MacOSVersionNumber();

//! \brief Returns the version of the running operating system.
//!
//! All parameters are required. No parameter may be `nullptr`.
//!
//! \param[out] major The major version of the operating system, such as `15`
//!     for macOS 15.4.1.
//! \param[out] minor The major version of the operating system, such as `4`
//!     for macOS 15.4.1.
//! \param[out] bugfix The bugfix version of the operating system, such as `1`
//!     for macOS 15.4.1.
//! \param[out] build The operating system’s build string, such as `"24E263"`
//!     for macOS 15.4.1.
//! \param[out] version_string A string representing the full operating system
//!     version, such as `"macOS 15.4.1 (24E263)"`. If \a bugfix is 0, it will
//!     not be included in \a version_string: `"macOS 15.5 (24F74)"`.
//!
//! \return `true` on success, `false` on failure, with an error message logged.
//!     A failure is considered to have occurred if any element could not be
//!     determined. When this happens, their values will be untouched, but other
//!     values that could be determined will still be set properly.
bool MacOSVersionComponents(int* major,
                            int* minor,
                            int* bugfix,
                            std::string* build,
                            std::string* version_string);

//! \brief Returns the model name and board ID of the running system.
//!
//! \param[out] model The system’s model name. A mid-2012 15\" MacBook Pro would
//!     report “MacBookPro10,1”, and a 2021 16\" M1 Max MacBook Pro would report
//!     “MacBookPro18,2”.
//! \param[out] board_id The system’s board ID or target type. An x86_64 system
//!     reports a board ID: a mid-2012 15\" MacBook Pro would report
//!     “Mac-C3EC7CD22292981F”. An arm64 system reports a target subtype or
//!     target type: a 2021 16\" M1 Max MacBook Pro would report “J316cAP”.
//!
//! If a value cannot be determined, its string is cleared.
void MacModelAndBoard(std::string* model, std::string* board_id);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MAC_MAC_UTIL_H_
