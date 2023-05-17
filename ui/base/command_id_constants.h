// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COMMAND_ID_CONSTANTS_H_
#define UI_BASE_COMMAND_ID_CONSTANTS_H_

// Starting command ID for showing an arbitrarily high (variable) number of
// items. NOTE: This id is shared with Chrome command IDs. Therefore, this ID
// must be greater than any bounding Chrome command ID.
#define COMMAND_ID_FIRST_UNBOUNDED 0xE000

#endif  // UI_BASE_COMMAND_ID_CONSTANTS_H_
