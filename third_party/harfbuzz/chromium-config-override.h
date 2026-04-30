/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CHROMIUM_CONFIG_OVERRIDE_H
#define CHROMIUM_CONFIG_OVERRIDE_H

/* HarfBuzz 14.2.0 introduced a relationship between HB_NO_DRAW and
 * HB_NO_OT_FONT_CFF in hb-config.hh. Chromium wants HB_NO_DRAW to reduce
 * binary size, but still needs CFF support for variable font subsetting.
 * This override unsets HB_NO_OT_FONT_CFF after hb-config.hh has set it.
 * TODO(crbug.com/507839557): Need to remove config override workaround after
 * https://github.com/harfbuzz/harfbuzz/issues/5955 is resolved.
 */
#undef HB_NO_OT_FONT_CFF

#endif  /* CHROMIUM_CONFIG_OVERRIDE_H */
