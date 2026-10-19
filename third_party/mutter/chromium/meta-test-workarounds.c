/*
 * Copyright 2026 The Chromium Authors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Workarounds for upstream mutter behavior that makes Chromium's tests flaky.
 *
 * Each function here replaces a libmutter function via -Wl,--wrap (see the
 * executable("mutter") target in //third_party/mutter/BUILD.gn). This keeps
 * //third_party/mutter/src a pristine upstream checkout. Note that --wrap only
 * affects calls from other translation units than the one defining the
 * wrapped function.
 */

#include "config.h"

#include <gio/gio.h>

#include "compositor/compositor-private.h"
#include "core/window-private.h"

void __wrap_meta_compositor_size_change_window (MetaCompositor *compositor,
                                                MetaWindow     *window,
                                                MetaSizeChange  which_change,
                                                MtkRectangle   *old_frame_rect,
                                                MtkRectangle   *old_buffer_rect);
GFile *__wrap_meta_window_get_unit_cgroup (MetaWindow *window);
gboolean __wrap_meta_window_unit_cgroup_equal (MetaWindow *window1,
                                               MetaWindow *window2);

/**
 * Suppresses the size-change window effect (used for maximize, fullscreen,
 * etc.).
 *
 * The default plugin's size_change() freezes the window actor and only
 * completes the effect from size_changed(), which runs on the next geometry
 * sync of the window. If the window is destroyed before that happens (e.g. a
 * test closes a browser right after it enters fullscreen), the effect never
 * completes, so the destroyed window actor is never removed from the stage.
 * The actor is fully transparent but still pickable, so it steals pointer
 * focus from every window below it for the rest of the session.
 *
 * The effect is purely cosmetic and only delays geometry updates, so skip it
 * entirely.
 **/
void
__wrap_meta_compositor_size_change_window (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           MetaSizeChange  which_change,
                                           MtkRectangle   *old_frame_rect,
                                           MtkRectangle   *old_buffer_rect)
{
}

/**
 * Disables the systemd unit cgroup lookup, which is only used to tag cgroups
 * with the xdg.inactive-since xattr on focus changes.
 *
 * meta_window_get_unit_cgroup() reads the client's cgroup and systemd user
 * unit with two separate calls and assumes the unit name is a substring of the
 * cgroup path. Chrome moves itself into a new app-*.scope unit shortly after
 * startup; if that happens between the two calls, strstr() returns NULL and
 * mutter crashes, failing every remaining test in the shard.
 *
 * meta_window_unit_cgroup_equal() is wrapped too, since its internal call to
 * meta_window_get_unit_cgroup() is in the same translation unit and therefore
 * is not affected by --wrap.
 *
 * Upstream bug: https://gitlab.gnome.org/GNOME/mutter/-/issues/5066
 * Upstream fix: https://gitlab.gnome.org/GNOME/mutter/-/merge_requests/5343
 * TODO(crbug.com/575566102): Remove these wrappers once a mutter release
 * containing the fix is rolled.
 **/
GFile *
__wrap_meta_window_get_unit_cgroup (MetaWindow *window)
{
  return NULL;
}

gboolean
__wrap_meta_window_unit_cgroup_equal (MetaWindow *window1,
                                      MetaWindow *window2)
{
  return FALSE;
}
