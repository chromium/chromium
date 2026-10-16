/*
 * Copyright 2025 The Chromium Authors
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
 * Implementation of the Chromium zcr_ui_controls_v2 Wayland protocol for
 * mutter, used by interactive_ui_tests to inject input and to know when the
 * injected input has been processed.
 *
 * This file is built by Chromium's GN build into the mutter executable and is
 * intentionally kept out of the upstream mutter tree so that
 * //third_party/mutter/src can track a pristine upstream release. It is
 * hooked into the compositor by wrapping meta_wayland_compositor_new() at
 * link time (see -Wl,--wrap in //third_party/mutter/BUILD.gn).
 */

#include "config.h"

#include <glib.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <stdint.h>
#include <wayland-server-core.h>

#include "clutter/clutter.h"
#include "compositor/compositor-private.h"
#include "compositor/meta-window-actor-private.h"
#include "compositor/meta-window-drag.h"
#include "glib-object.h"
#include "meta/compositor.h"
#include "meta/display.h"
#include "meta/meta-context.h"
#include "meta/meta-debug.h"
#include "meta/meta-window-actor.h"
#include "ui-controls-unstable-v2-server-protocol.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-types.h"
#include "wayland/meta-wayland-xdg-shell.h"

#define META_UI_CONTROLS_V2_VERSION 1

typedef struct
{
  struct wl_resource *resource;
  uint32_t id;
  ClutterEventType type;
  uint64_t time_us;
  bool wait_for_pointer_focus;
} MetaWaylandUiControlsRequest;

typedef struct
{
  ClutterEventType type;
  uint64_t time_us;
} MetaWaylandUiControlsSeenEvent;

/* A key or mouse button request whose modifier key presses have been injected,
 * but whose remaining events are deferred until the modifier presses have been
 * dispatched. See notify_modifiers(). */
typedef struct
{
  struct wl_resource *resource;
  bool is_key;
  uint32_t key_or_button;
  uint32_t state;
  uint32_t pressed_modifiers;
  uint32_t id;

  /* The last injected modifier key press. */
  uint64_t modifier_press_time_us;
} MetaWaylandUiControlsPendingInput;

typedef struct
{
  MetaWaylandCompositor *compositor;
  bool has_client;
  ClutterStage *stage;
  struct wl_global *global;
  ClutterVirtualInputDevice *virtual_pointer;
  ClutterVirtualInputDevice *virtual_keyboard;
  MetaWindowDrag *window_drag;
  GQueue *requests;
  GQueue *pointer_move_requests_during_grab;
  GQueue *pointer_release_requests_during_grab;

  /* Input events observed by the event filter that mutter has (or will have,
   * by the time the idle callback runs) finished processing, but that have not
   * yet been matched against pending requests. */
  GArray *seen_events;
  guint seen_events_idle_id;
  guint event_filter_id;

  /* Input events observed by the event filter while the stage had a grab.
   * These are processed during the next stage update, so they are moved to
   * seen_events once that update has completed. */
  GArray *deferred_events;

  /* Number of motion events observed during a window drag whose resulting
   * move/resize has not yet been applied. */
  guint pending_drag_motions;
  gulong after_update_handler_id;

  MetaWaylandUiControlsPendingInput *pending_input;
  guint pending_input_idle_id;
  guint pending_input_timeout_id;
} MetaWaylandUiControls;

/* Prototypes for the link-time wrapper of meta_wayland_compositor_new(). */
MetaWaylandCompositor *__real_meta_wayland_compositor_new (MetaContext *context);
MetaWaylandCompositor *__wrap_meta_wayland_compositor_new (MetaContext *context);

static void schedule_continue_pending_input (MetaWaylandUiControls *ui_controls);

static void
notify_request_done (MetaWaylandUiControlsRequest *request)
{

  meta_topic (META_DEBUG_INPUT, "%s %u", __func__, request->id);
  zcr_ui_controls_v2_send_request_processed (request->resource, request->id);
  g_free (request);
}

static gboolean
set_true (gpointer user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  return G_SOURCE_REMOVE;
}

static void
on_input_event_processed (MetaWaylandUiControls *ui_controls,
                          ClutterEventType       event_type,
                          uint64_t               event_time_us)
{
  int n;
  gboolean timed_out;
  MetaWaylandUiControlsRequest *request;
  MetaWaylandPointer *pointer = ui_controls->compositor->seat->pointer;

  for (n = 0; n < g_queue_get_length (ui_controls->requests); n++)
    {
      request = g_queue_peek_nth (ui_controls->requests, n);
      if (request->type == event_type &&
          request->time_us == event_time_us)
        {
          if (request->wait_for_pointer_focus)
            {
              if (!meta_wayland_pointer_get_focus_surface (pointer))
                meta_topic (META_DEBUG_INPUT,
                            "waiting for surface to have pointer focus before "
                            "signaling done");
              timed_out = FALSE;
              guint timeout_id =
                g_timeout_add_seconds (1, set_true, &timed_out);
              while (!meta_wayland_pointer_get_focus_surface (pointer) &&
                     !timed_out)
                {
                  g_main_context_iteration (NULL, TRUE);
                  // Abort if the ui_controls client was destroyed.
                  if (!ui_controls->has_client)
                    {
                      meta_topic (META_DEBUG_INPUT,
                                  "aborting wait for pointer focus as "
                                  "ui_controls client was destroyed");
                      if (!timed_out)
                        g_clear_handle_id (&timeout_id, g_source_remove);
                      return;
                    }
                }
              if (timed_out)
                g_warning ("timed out waiting for surface to have pointer "
                              "focus before signaling done");
              else
                g_clear_handle_id (&timeout_id, g_source_remove);
            }
          g_queue_remove (ui_controls->requests, request);
          notify_request_done (request);
          break;
        }
    }
}

static gboolean
process_seen_events (gpointer user_data)
{
  MetaWaylandUiControls *ui_controls = user_data;
  g_autoptr (GArray) seen_events = NULL;
  guint i;

  ui_controls->seen_events_idle_id = 0;

  /* Swap the array out, as matching may spin the main loop, during which new
   * events may be observed. */
  seen_events = g_steal_pointer (&ui_controls->seen_events);
  ui_controls->seen_events =
    g_array_new (FALSE, FALSE, sizeof (MetaWaylandUiControlsSeenEvent));

  for (i = 0; i < seen_events->len; i++)
    {
      MetaWaylandUiControlsSeenEvent *seen =
        &g_array_index (seen_events, MetaWaylandUiControlsSeenEvent, i);

      on_input_event_processed (ui_controls, seen->type, seen->time_us);
    }

  return G_SOURCE_REMOVE;
}

static void
notify_request_done_during_grab (MetaWaylandUiControls *ui_controls,
                                 const ClutterEventType event_type)
{
  MetaWaylandUiControlsRequest *request;

  bool is_motion = event_type == CLUTTER_MOTION;
  GQueue *requests = is_motion
                         ? ui_controls->pointer_move_requests_during_grab
                         : ui_controls->pointer_release_requests_during_grab;
  guint queue_length = g_queue_get_length (requests);
  if (!is_motion && queue_length > 1)
    {
      // In case a test issues multiple release requests during window
      // grab, which is unlikely, release all of them on grab end as
      // best-effort.
      g_warning ("Multiple release events were received during window "
                    "grab, so releasing all of them on grab end. Note: "
                    "This likely indicates a test bug.");
    }
  while (!g_queue_is_empty (requests))
    {
      request = g_queue_pop_head (requests);
      notify_request_done (request);
      // Consider one motion event as done.
      if (is_motion)
        break;
    }
}

static void
schedule_process_seen_events (MetaWaylandUiControls *ui_controls)
{
  if (ui_controls->seen_events_idle_id)
    return;

  ui_controls->seen_events_idle_id =
    g_idle_add_full (G_PRIORITY_HIGH, process_seen_events, ui_controls, NULL);
}

/**
 * Called after each stage update. A window drag applies the move/resize for a
 * motion event from a META_LATER_BEFORE_REDRAW later, which runs during the
 * stage update that processes the (queued) motion event. So once the stage
 * update following the motion event has completed, the move/resize is done.
 **/
static void
on_stage_after_update (ClutterStage          *stage,
                       ClutterStageView      *view,
                       ClutterFrame          *frame,
                       MetaWaylandUiControls *ui_controls)
{
  guint pending_drag_motions = ui_controls->pending_drag_motions;

  meta_topic (META_DEBUG_INPUT,
              "%s pending_drag_motions=%u deferred_events=%u", __func__,
              pending_drag_motions, ui_controls->deferred_events->len);

  ui_controls->pending_drag_motions = 0;
  g_clear_signal_handler (&ui_controls->after_update_handler_id, stage);

  while (pending_drag_motions--)
    notify_request_done_during_grab (ui_controls, CLUTTER_MOTION);

  if (ui_controls->deferred_events->len > 0)
    {
      /* Matching may spin the main loop, so don't do it from within the stage
       * update. */
      g_array_append_vals (ui_controls->seen_events,
                           ui_controls->deferred_events->data,
                           ui_controls->deferred_events->len);
      g_array_set_size (ui_controls->deferred_events, 0);
      schedule_process_seen_events (ui_controls);
    }
}

static void
ensure_after_update_handler (MetaWaylandUiControls *ui_controls)
{
  if (ui_controls->after_update_handler_id)
    return;

  ui_controls->after_update_handler_id =
    g_signal_connect (ui_controls->stage, "after-update",
                      G_CALLBACK (on_stage_after_update), ui_controls);
}

/**
 * Observes every input event before mutter processes it. This filter is
 * registered before mutter's own event filter (see
 * meta_wayland_ui_controls_init()), so it sees events even if mutter consumes
 * them.
 **/
static gboolean
on_event_filter (const ClutterEvent *event,
                 ClutterActor       *event_actor,
                 gpointer            user_data)
{
  MetaWaylandUiControls *ui_controls = user_data;
  ClutterEventType event_type = clutter_event_type (event);
  MetaWaylandUiControlsSeenEvent seen;

  switch (event_type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      break;
    default:
      return CLUTTER_EVENT_PROPAGATE;
    }

  if (ui_controls->pending_input && event_type == CLUTTER_KEY_PRESS &&
      clutter_event_get_time_us (event) ==
        ui_controls->pending_input->modifier_press_time_us)
    schedule_continue_pending_input (ui_controls);

  if (event_type == CLUTTER_MOTION && ui_controls->window_drag &&
      !g_queue_is_empty (ui_controls->pointer_move_requests_during_grab))
    {
      ui_controls->pending_drag_motions++;
      ensure_after_update_handler (ui_controls);
    }

  if (g_queue_is_empty (ui_controls->requests))
    return CLUTTER_EVENT_PROPAGATE;

  seen.type = event_type;
  seen.time_us = clutter_event_get_time_us (event);

  if (clutter_stage_get_grab_actor (ui_controls->stage))
    {
      /* While the stage has a grab (e.g. a Wayland drag-and-drop or popup
       * grab), mutter does not handle the event from its event filter.
       * Instead, clutter queues the event and delivers it to the grab actor
       * at the start of the next stage update. */
      g_array_append_val (ui_controls->deferred_events, seen);
      ensure_after_update_handler (ui_controls);
      return CLUTTER_EVENT_PROPAGATE;
    }

  g_array_append_val (ui_controls->seen_events, seen);

  /* Mutter processes the event synchronously from the remaining event
   * filters, so a high priority idle runs once it has been handled. */
  schedule_process_seen_events (ui_controls);

  return CLUTTER_EVENT_PROPAGATE;
}

static void
on_grab_ended (MetaWaylandUiControls *ui_controls)
{
  meta_topic (META_DEBUG_INPUT, "%s", __func__);
  notify_request_done_during_grab (ui_controls, CLUTTER_BUTTON_RELEASE);
}

/**
 * Listens for stage grab state and subscribes to window drag signals if a
 * window drag is found during grab state.
 **/
static void
on_stage_is_grabbed_change (MetaWaylandUiControls *ui_controls)
{
  bool stage_has_grab =
    clutter_stage_get_grab_actor (ui_controls->stage) != NULL;
  if (stage_has_grab)
    {
      MetaContext *context =
        meta_wayland_compositor_get_context (ui_controls->compositor);
      MetaDisplay *display = meta_context_get_display (context);
      MetaCompositor *compositor = meta_display_get_compositor (display);

      ui_controls->window_drag = meta_compositor_get_current_window_drag (
        META_COMPOSITOR (compositor));
      meta_topic (META_DEBUG_INPUT, "%s window drag: %p", __func__,
                  ui_controls->window_drag);
      if (ui_controls->window_drag)
        {
          g_signal_connect_swapped (ui_controls->window_drag, "ended",
                                    G_CALLBACK (on_grab_ended), ui_controls);
        }
    }
  else
    {
      meta_topic (META_DEBUG_INPUT,
                  "%s stage doesn't have grab, clearing window drag", __func__);
      ui_controls->window_drag = NULL;
    }
}

/**
 * Stores the request until it is processed and `request_processed` is sent.
 **/
static void
store_request (MetaWaylandUiControls *ui_controls,
               struct wl_resource    *resource,
               uint32_t               request_id,
               ClutterEventType       type,
               uint64_t               time_us,
               bool                   wait_for_pointer_focus)
{
  MetaWaylandUiControlsRequest *request;
  request = g_new0 (MetaWaylandUiControlsRequest, 1);
  request->resource = resource;
  request->id = request_id;
  request->type = type;
  request->time_us = time_us;
  request->wait_for_pointer_focus = wait_for_pointer_focus;
  if (ui_controls->window_drag != NULL)
  // During window drag, motion and release events are not handled by the
  // wayland input handler. So store these requests separately from the
  // regular requests. Window drag events will signal their completion.
  // See on_stage_is_grabbed_change() and on_event_filter().
    {
      if (type == CLUTTER_MOTION)
        {
          g_queue_push_tail (ui_controls->pointer_move_requests_during_grab,
                             request);
          return;
        }
      if (type == CLUTTER_BUTTON_RELEASE)
        {
          g_queue_push_tail (ui_controls->pointer_release_requests_during_grab,
                             request);
          return;
        }
    }
  g_queue_push_tail (ui_controls->requests, request);
}

/**
 * Injects key events for the given modifiers. Returns the time of the last
 * injected event.
 **/
static uint64_t
notify_modifiers (MetaWaylandUiControls *ui_controls,
                  uint32_t               pressed_modifiers,
                  uint32_t               key_state)
{
  uint64_t time_us = 0;

  if (pressed_modifiers & ZCR_UI_CONTROLS_V2_MODIFIER_SHIFT)
    {
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_key (
        ui_controls->virtual_keyboard, time_us, KEY_LEFTSHIFT, key_state);
    }
  if (pressed_modifiers & ZCR_UI_CONTROLS_V2_MODIFIER_CONTROL)
    {
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_key (
        ui_controls->virtual_keyboard, time_us, KEY_LEFTCTRL, key_state);
    }
  if (pressed_modifiers & ZCR_UI_CONTROLS_V2_MODIFIER_ALT)
    {
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_key (
        ui_controls->virtual_keyboard, time_us, KEY_LEFTALT, key_state);
    }

  return time_us;
}

/**
 * Creates clutter virtual pointer and keyboard devices.
 **/
static void
create_virtual_input_devices (MetaWaylandUiControls *ui_controls)
{

  MetaWaylandCompositor *compositor = ui_controls->compositor;
  MetaContext *context =
    meta_wayland_compositor_get_context (compositor);
  MetaBackend *backend = meta_context_get_backend (context);
  ClutterBackend *clutter_backend =
    meta_backend_get_clutter_backend (backend);
  ClutterSeat *seat = clutter_backend_get_default_seat (clutter_backend);

  ui_controls->virtual_pointer =
    clutter_seat_create_virtual_device (seat, CLUTTER_POINTER_DEVICE);
  ui_controls->virtual_keyboard =
    clutter_seat_create_virtual_device (seat, CLUTTER_KEYBOARD_DEVICE);
}

/**
 * Injects the events for a key request, except for modifier key presses, which
 * must already have been injected.
 **/
static void
send_key_events_after_modifiers (MetaWaylandUiControls *ui_controls,
                                 struct wl_resource    *resource,
                                 uint32_t               key,
                                 uint32_t               key_state,
                                 uint32_t               pressed_modifiers,
                                 uint32_t               id)
{
  ClutterEventType event_type;
  uint64_t time_us;

  if (key_state & ZCR_UI_CONTROLS_V2_KEY_STATE_PRESS)
    {
      event_type = CLUTTER_KEY_PRESS;
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_key (ui_controls->virtual_keyboard,
                                               time_us, key,
                                               CLUTTER_KEY_STATE_PRESSED);
    }
  if (key_state & ZCR_UI_CONTROLS_V2_KEY_STATE_RELEASE)
    {
      event_type = CLUTTER_KEY_RELEASE;
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_key (ui_controls->virtual_keyboard,
                                               time_us, key,
                                               CLUTTER_KEY_STATE_RELEASED);
      notify_modifiers (ui_controls, pressed_modifiers,
                        CLUTTER_KEY_STATE_RELEASED);
    }

  store_request (ui_controls, resource, id, event_type, time_us,
                 false /*wait_for_pointer_focus=*/);
}

/**
 * Injects the modifier key presses for a key or mouse button request and
 * defers the remaining events until mutter has dispatched the modifier presses.
 *
 * The native backend updates the keymap's modifier state, which mutter uses to
 * send wl_keyboard.modifiers after a key event, from a high priority idle that
 * the input thread queues before the key event itself. If all of the request's
 * events were injected at once, the idles for both the modifier presses and
 * releases would run before the modifier presses are dispatched, so the client
 * would never be told that the modifiers are pressed.
 **/
static void
defer_until_modifiers_pressed (MetaWaylandUiControls *ui_controls,
                               struct wl_resource    *resource,
                               bool                   is_key,
                               uint32_t               key_or_button,
                               uint32_t               state,
                               uint32_t               pressed_modifiers,
                               uint32_t               id);

static void
ui_controls_send_key_events (struct wl_client   *client,
                             struct wl_resource *resource,
                             uint32_t            key,
                             uint32_t            key_state,
                             uint32_t            pressed_modifiers,
                             uint32_t            id)
{
  MetaWaylandUiControls *ui_controls = wl_resource_get_user_data (resource);

  meta_topic (META_DEBUG_INPUT, "%s id=%u key=%u state=%u has_modifiers=%d",
              __func__, id, key, key_state, pressed_modifiers != 0);

  if ((key_state & ZCR_UI_CONTROLS_V2_KEY_STATE_PRESS) &&
      pressed_modifiers != 0)
    {
      defer_until_modifiers_pressed (ui_controls, resource, true, key,
                                     key_state, pressed_modifiers, id);
      return;
    }

  send_key_events_after_modifiers (ui_controls, resource, key, key_state,
                                   pressed_modifiers, id);
}

static void
on_effects_completed (MetaWindowActor *window_actor,
                      gboolean        *done)
{
  meta_topic (META_DEBUG_INPUT, "window effects complete");
  *done = TRUE;
}

static void
wait_for_window_effects_completed (MetaWindowActor *window_actor)
{
  meta_topic (META_DEBUG_INPUT, "waiting for window effects to complete");
  gboolean done = FALSE;
  gulong handler_id;

  handler_id = g_signal_connect (window_actor, "effects-completed",
                                 G_CALLBACK (on_effects_completed), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_signal_handler_disconnect (window_actor, handler_id);
}

static void
ui_controls_send_mouse_move(struct wl_client   *client,
                            struct wl_resource *resource,
                            int32_t             x,
                            int32_t             y,
                            struct wl_resource *surface_resource,
                            uint32_t            id) {
  MetaWaylandSurface *surface;
  MetaWindow *window;
  MetaWindowActor *window_actor;
  MetaWaylandUiControls *ui_controls = wl_resource_get_user_data (resource);
  float abs_x, abs_y;
  bool effects_in_progress = false;
  uint64_t time_us = g_get_monotonic_time ();
  // Wait for pointer focus if mouse move was requested relative to a surface.
  const bool wait_for_pointer_focus = surface_resource != 0;

  if (surface_resource)
  // A surface was provided. Use coordinates relative to it.
    {
      surface = wl_resource_get_user_data (surface_resource);
      window = meta_wayland_surface_get_window (surface);
      window_actor = meta_window_actor_from_window (window);
      effects_in_progress = meta_window_actor_effect_in_progress (window_actor);
      if (effects_in_progress)
        wait_for_window_effects_completed (window_actor);
      meta_wayland_surface_get_absolute_coordinates (surface, x, y, &abs_x,
                                                     &abs_y);
      abs_x = roundf (abs_x);
      abs_y = roundf (abs_y);
    }
  else
  // No surface was provided. Use global coordinates.
    {
      abs_x = x;
      abs_y = y;
    }

  meta_topic (META_DEBUG_INPUT,
              "%s id=%u x=%d y=%d has_surface=%d, abs_x=%f, abs_y=%f", __func__,
              id, x, y, surface_resource != 0, abs_x, abs_y);

  clutter_virtual_input_device_notify_absolute_motion (
    ui_controls->virtual_pointer, time_us, abs_x, abs_y);
  store_request (ui_controls, resource, id, CLUTTER_MOTION, time_us,
                 wait_for_pointer_focus);
}

/**
 * Injects the events for a mouse button request, except for modifier key
 * presses, which must already have been injected.
 **/
static void
send_mouse_button_after_modifiers (MetaWaylandUiControls *ui_controls,
                                   struct wl_resource    *resource,
                                   uint32_t               button,
                                   uint32_t               button_state,
                                   uint32_t               pressed_modifiers,
                                   uint32_t               id)
{
  uint32_t clutter_button;
  ClutterEventType event_type;
  uint64_t time_us;

  switch (button)
    {
    case ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_LEFT:
      clutter_button = CLUTTER_BUTTON_PRIMARY;
      break;
    case ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_RIGHT:
      clutter_button = CLUTTER_BUTTON_SECONDARY;
      break;
    case ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_MIDDLE:
      clutter_button = CLUTTER_BUTTON_MIDDLE;
      break;
    default:
      g_assert_not_reached ();
    }

  if (button_state & ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_STATE_DOWN)
    {
      event_type = CLUTTER_BUTTON_PRESS;
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_button (ui_controls->virtual_pointer,
                                                  time_us, clutter_button,
                                                  CLUTTER_BUTTON_STATE_PRESSED);
    }

  if (button_state & ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_STATE_UP)
    {
      event_type = CLUTTER_BUTTON_RELEASE;
      time_us = g_get_monotonic_time ();
      clutter_virtual_input_device_notify_button (
        ui_controls->virtual_pointer, time_us, clutter_button,
        CLUTTER_BUTTON_STATE_RELEASED);
      notify_modifiers (ui_controls, pressed_modifiers,
                        CLUTTER_KEY_STATE_RELEASED);
    }

  store_request (ui_controls, resource, id, event_type, time_us,
                 false /*wait_for_pointer_focus=*/);
}

static void
ui_controls_send_mouse_button (struct wl_client   *client,
                               struct wl_resource *resource,
                               uint32_t            button,
                               uint32_t            button_state,
                               uint32_t            pressed_modifiers,
                               uint32_t            id)
{
  MetaWaylandUiControls *ui_controls = wl_resource_get_user_data (resource);

  meta_topic (META_DEBUG_INPUT, "%s id=%u button=%u state=%u has_modifiers=%d",
              __func__, id, button, button_state, pressed_modifiers != 0);

  if ((button_state & ZCR_UI_CONTROLS_V2_MOUSE_BUTTON_STATE_DOWN) &&
      pressed_modifiers != 0)
    {
      defer_until_modifiers_pressed (ui_controls, resource, false, button,
                                     button_state, pressed_modifiers, id);
      return;
    }

  send_mouse_button_after_modifiers (ui_controls, resource, button,
                                     button_state, pressed_modifiers, id);
}

static void
clear_pending_input (MetaWaylandUiControls *ui_controls)
{
  g_clear_handle_id (&ui_controls->pending_input_idle_id, g_source_remove);
  g_clear_handle_id (&ui_controls->pending_input_timeout_id, g_source_remove);
  g_clear_pointer (&ui_controls->pending_input, g_free);
}

static void
continue_pending_input (MetaWaylandUiControls *ui_controls)
{
  MetaWaylandUiControlsPendingInput *pending =
    g_steal_pointer (&ui_controls->pending_input);

  clear_pending_input (ui_controls);
  if (!pending)
    return;

  meta_topic (META_DEBUG_INPUT, "%s id=%u", __func__, pending->id);

  if (pending->is_key)
    {
      send_key_events_after_modifiers (ui_controls, pending->resource,
                                       pending->key_or_button, pending->state,
                                       pending->pressed_modifiers,
                                       pending->id);
    }
  else
    {
      send_mouse_button_after_modifiers (ui_controls, pending->resource,
                                         pending->key_or_button,
                                         pending->state,
                                         pending->pressed_modifiers,
                                         pending->id);
    }
  g_free (pending);
}

static gboolean
on_pending_input_idle (gpointer user_data)
{
  MetaWaylandUiControls *ui_controls = user_data;

  ui_controls->pending_input_idle_id = 0;
  continue_pending_input (ui_controls);

  return G_SOURCE_REMOVE;
}

static gboolean
on_pending_input_timeout (gpointer user_data)
{
  MetaWaylandUiControls *ui_controls = user_data;

  ui_controls->pending_input_timeout_id = 0;
  g_warning ("timed out waiting for modifier key presses to be dispatched");
  continue_pending_input (ui_controls);

  return G_SOURCE_REMOVE;
}

/**
 * Called from the event filter when the last modifier key press is about to be
 * handled. Continue once mutter has finished handling it.
 **/
static void
schedule_continue_pending_input (MetaWaylandUiControls *ui_controls)
{
  if (ui_controls->pending_input_idle_id)
    return;

  ui_controls->pending_input_idle_id =
    g_idle_add_full (G_PRIORITY_HIGH, on_pending_input_idle, ui_controls,
                     NULL);
}

static void
defer_until_modifiers_pressed (MetaWaylandUiControls *ui_controls,
                               struct wl_resource    *resource,
                               bool                   is_key,
                               uint32_t               key_or_button,
                               uint32_t               state,
                               uint32_t               pressed_modifiers,
                               uint32_t               id)
{
  MetaWaylandUiControlsPendingInput *pending;

  /* Clients wait for each request to be processed before sending the next one,
   * so there shouldn't be another pending request. Finish it if there is. */
  if (ui_controls->pending_input)
    continue_pending_input (ui_controls);

  pending = g_new0 (MetaWaylandUiControlsPendingInput, 1);
  pending->resource = resource;
  pending->is_key = is_key;
  pending->key_or_button = key_or_button;
  pending->state = state;
  pending->pressed_modifiers = pressed_modifiers;
  pending->id = id;
  pending->modifier_press_time_us =
    notify_modifiers (ui_controls, pressed_modifiers,
                      CLUTTER_KEY_STATE_PRESSED);
  ui_controls->pending_input = pending;

  /* In case the modifier presses are never dispatched, e.g. because a modifier
   * was already pressed. */
  ui_controls->pending_input_timeout_id =
    g_timeout_add_seconds (1, on_pending_input_timeout, ui_controls);
}

static const struct zcr_ui_controls_v2_interface meta_ui_controls_interface = {
  ui_controls_send_key_events,
  ui_controls_send_mouse_move,
  ui_controls_send_mouse_button,
};

static void
destroy_ui_controls (struct wl_resource *resource)
{
  MetaWaylandUiControls *ui_controls = wl_resource_get_user_data (resource);

  meta_topic (META_DEBUG_INPUT, "%s", __func__);

  ui_controls->has_client = false;

  clear_pending_input (ui_controls);

  // Ensure any pressed keys and buttons are released when a client resource is
  // destroyed. Destroying a virtual input device releases everything that is
  // still pressed on it, so replace the devices with new ones.
  g_clear_object (&ui_controls->virtual_pointer);
  g_clear_object (&ui_controls->virtual_keyboard);
  create_virtual_input_devices (ui_controls);

  // Clear all queues.
  g_queue_clear_full (ui_controls->requests, g_free);
  g_queue_clear_full (ui_controls->pointer_move_requests_during_grab, g_free);
  g_queue_clear_full (ui_controls->pointer_release_requests_during_grab,
                      g_free);
  g_array_set_size (ui_controls->seen_events, 0);
  g_array_set_size (ui_controls->deferred_events, 0);
  ui_controls->pending_drag_motions = 0;
}

static void
bind_ui_controls (struct wl_client *client,
                  void             *data,
                  uint32_t          version,
                  uint32_t          id)
{
  MetaWaylandUiControls *ui_controls = data;
  struct wl_resource *resource;

  meta_topic (META_DEBUG_INPUT, "%s id=%u requested_version=%u", __func__, id,
              version);

  ui_controls->has_client = true;

  resource = wl_resource_create (client, &zcr_ui_controls_v2_interface,
                                 META_UI_CONTROLS_V2_VERSION, id);
  wl_resource_set_implementation (resource, &meta_ui_controls_interface,
                                  ui_controls, &destroy_ui_controls);
}

static void
meta_wayland_ui_controls_free (gpointer  data,
                               GObject  *where_the_compositor_was)
{
  MetaWaylandUiControls *ui_controls = data;

  g_clear_handle_id (&ui_controls->event_filter_id,
                     clutter_event_remove_filter);
  g_clear_handle_id (&ui_controls->seen_events_idle_id, g_source_remove);
  clear_pending_input (ui_controls);
  g_clear_signal_handler (&ui_controls->after_update_handler_id,
                          ui_controls->stage);
  g_signal_handlers_disconnect_by_func (ui_controls->stage,
                                        on_stage_is_grabbed_change,
                                        ui_controls);
  g_clear_pointer (&ui_controls->seen_events, g_array_unref);
  g_clear_pointer (&ui_controls->deferred_events, g_array_unref);
  g_clear_pointer (&ui_controls->requests, g_queue_free);
  g_clear_pointer (&ui_controls->pointer_move_requests_during_grab,
                   g_queue_free);
  g_clear_pointer (&ui_controls->pointer_release_requests_during_grab,
                   g_queue_free);
  g_free (ui_controls);
}

static void
meta_wayland_ui_controls_init (MetaWaylandCompositor *compositor)
{
  MetaWaylandUiControls *ui_controls;
  struct wl_display *wayland_display;
  MetaContext *context;
  MetaBackend *backend;

  ui_controls = g_new0 (MetaWaylandUiControls, 1);
  ui_controls->compositor = compositor;
  create_virtual_input_devices (ui_controls);

  wayland_display = meta_wayland_compositor_get_wayland_display (compositor);
  ui_controls->global = wl_global_create (wayland_display,
                                          &zcr_ui_controls_v2_interface,
                                          META_UI_CONTROLS_V2_VERSION,
                                          ui_controls, bind_ui_controls);
  if (!ui_controls->global)
    g_error ("Could not create ui controls global");

  ui_controls->requests = g_queue_new ();
  ui_controls->pointer_move_requests_during_grab = g_queue_new ();
  ui_controls->pointer_release_requests_during_grab = g_queue_new ();
  ui_controls->seen_events =
    g_array_new (FALSE, FALSE, sizeof (MetaWaylandUiControlsSeenEvent));
  ui_controls->deferred_events =
    g_array_new (FALSE, FALSE, sizeof (MetaWaylandUiControlsSeenEvent));

  /* This runs before MetaDisplay is created, so this filter is registered
   * before (and hence runs before) mutter's own event filter. */
  ui_controls->event_filter_id =
    clutter_event_add_filter (NULL, on_event_filter, NULL, ui_controls);

  context = meta_wayland_compositor_get_context (compositor);
  backend = meta_context_get_backend (context);
  ui_controls->stage = CLUTTER_STAGE (meta_backend_get_stage (backend));
  g_signal_connect_swapped (
    ui_controls->stage, "notify::is-grabbed",
    G_CALLBACK (on_stage_is_grabbed_change), ui_controls);

  g_object_weak_ref (G_OBJECT (compositor), meta_wayland_ui_controls_free,
                     ui_controls);
}

/**
 * Link-time wrapper (-Wl,--wrap=meta_wayland_compositor_new) that sets up
 * ui_controls right after the Wayland compositor is created, which is where
 * upstream mutter initializes its own Wayland globals.
 **/
MetaWaylandCompositor *
__wrap_meta_wayland_compositor_new (MetaContext *context)
{
  MetaWaylandCompositor *compositor;

  compositor = __real_meta_wayland_compositor_new (context);
  meta_wayland_ui_controls_init (compositor);

  return compositor;
}
