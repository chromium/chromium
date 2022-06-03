The weston-test protocol for input emulation.
=============

The weston-test protocol is a protocol that is used to extend the functionality of
a Wayland compositor and provide its clients the ability to send input events
through it.

This protocol is used only in tests and is not meant to be used in production.

Usage of the protocol.
=============

The client uses the protocol like this:
1) Bind to the weston-tests.

       struct weston_test* weston_test =
           wl_registry_bind(registry, name, interface, version);

2) Attach a buffer to a surface and wait until a frame callback is sent. This
ensures a Wayland compositor has processed the request, and the surface has
the size equals to the size of the buffer.

       wl_surface_attach(wlsurface, wl_buffer, x, y);
       wl_callback* frame_callback = wl_surface_frame(wlsurface);
       static const struct wl_callback_listener kFrameCallbackListener = {
           &FrameCallbackHandler};
       wl_callback_add_listener(frame_callback, &kFrameCallbackListener, NULL);
       wl_surface_commit(wlsurface);

3) Once the frame callback comes, activate a surface the events should go to.
Otherwise, it is unknown what surface will receive the events as the surface
may miss a keyboard focus or another surface may be located on top of the
stack of existing surfaces.

       weston_test_activate_surface(weston_test, wl_surface);

4) Then move the pointer to a surface that should receive events using surface local coordinates.
Please note that if keyboard events are sent, it is not required to move the pointer.

       weston_test_move_pointer(weston_test, wlsurface, tv_sec_hi, tv_sec_lo, surface_x, surface_y);

5) Send input events.

       weston_test_send_button(weston_test, tv_sec_hi, tv_sec_lo, changed_button, press_state);

       weston_test_send_key(weston_test, tv_sec_hi, tv_sec_lo, evdev_key_code, press_state);
