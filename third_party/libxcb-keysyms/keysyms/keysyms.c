/*
 * Copyright © 2008 Ian Osgood <iano@quirkster.com>
 * Copyright © 2008 Jamey Sharp <jamey@minilop.net>
 * Copyright © 2008 Josh Triplett <josh@freedesktop.org>
 * Copyright © 2008 Ulrich Eckhardt <doomster@knuut.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or
 * their institutions shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization from the authors.
 */

// Ported from xcb_key_symbols_get_keysym
// https://gitlab.freedesktop.org/xorg/lib/libxcb-keysyms/-/blob/691515491a4a3c119adc6c769c29de264b3f3806/keysyms/keysyms.c#L189
uint32_t KeycodeColumnToKeysym(KeyCode keycode,
                               int column,
                               const GetKeyboardMappingReply& keyboard_mapping,
                               uint8_t min_keycode,
                               uint8_t max_keycode) {
  uint8_t key = static_cast<uint8_t>(keycode);
  uint8_t n_keysyms = keyboard_mapping.keysyms_per_keycode;

  if (column < 0 || (column >= n_keysyms && column > 3) || key < min_keycode ||
      key > max_keycode) {
    return 0;
  }

  const auto* syms = &keyboard_mapping.keysyms[(key - min_keycode) * n_keysyms];
  if (column < 4) {
    if (column > 1) {
      while ((n_keysyms > 2) && (syms[n_keysyms - 1] == kNoSymbol))
        n_keysyms--;
      if (n_keysyms < 3)
        column -= 2;
    }
    if ((n_keysyms <= (column | 1)) || (syms[column | 1] == kNoSymbol)) {
      KeySym lsym, usym;
      ConvertCase(syms[column & ~1], &lsym, &usym);
      if (!(column & 1))
        return static_cast<uint32_t>(lsym);
      if (usym == lsym)
        return 0;
      return static_cast<uint32_t>(usym);
    }
  }
  return static_cast<uint32_t>(syms[column]);
}
