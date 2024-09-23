/*

Copyright 1985, 1987, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

// Ported from XkbTranslateKeycode:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/xkb/XKBBind.c#L288
uint8_t AdjustGroup(uint8_t group, uint8_t group_info) {
  const uint8_t n_groups = group_info & 0x0f;
  const uint8_t out_of_range_group = (group_info & 0x30) >> 4;
  const auto groups_wrap = static_cast<Xkb::GroupsWrap>(group_info & 0xc0);

  if (group < n_groups)
    return group;

  switch (groups_wrap) {
    case Xkb::GroupsWrap::WrapIntoRange:
      return group % n_groups;
    case Xkb::GroupsWrap::ClampIntoRange:
      return n_groups - 1;
    case Xkb::GroupsWrap::RedirectIntoRange:
      return out_of_range_group < n_groups ? out_of_range_group : 0;
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

// Ported from XkbTranslateKeycode:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/xkb/XKBBind.c#L265
uint32_t KeycodeToKeysymXkbImpl(KeyCode key,
                                uint32_t modifiers,
                                const Xkb::GetMapReply& map) {
  const uint8_t keycode = static_cast<uint8_t>(key);
  const auto first_keycode = static_cast<size_t>(map.firstKeySym);
  if (keycode < first_keycode || keycode >= first_keycode + map.nKeySyms)
    return 0;

  const auto& key_sym_map = map.syms_rtrn->at(keycode - first_keycode);
  uint8_t n_groups = key_sym_map.groupInfo & 0x0f;
  if (!n_groups)
    return 0;

  auto group =
      AdjustGroup(GetXkbGroupFromState(modifiers), key_sym_map.groupInfo);
  unsigned col = group * key_sym_map.width;
  auto type_index = key_sym_map.kt_index[group];
  if (type_index >= map.types_rtrn->size())
    return 0;
  const auto& type = map.types_rtrn->at(type_index);

  for (size_t i = 0; i < type.map.size(); i++) {
    const auto& entry = type.map[i];
    if (entry.active && (static_cast<x11::ModMask>(modifiers) &
                         type.mods_mask) == entry.mods_mask) {
      col += entry.level;
      if (type.hasPreserve)
        break;
    }
  }

  if (col >= key_sym_map.syms.size())
    return 0;
  return static_cast<uint32_t>(key_sym_map.syms.at(col));
}
