See [Ozone
Overview](https://source.chromium.org/chromium/chromium/src/+/main:docs/ozone_overview.md)
for high-level summary.

Wayland is a window server protocol primarily being developed for Linux desktop.
See [home page](https://wayland.freedesktop.org/) and [core protocol
API](https://wayland.freedesktop.org/docs/html/). The API lives
[here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/wayland/)
and the default Weston implementation can be found
[here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/weston/).

For those less familiar, the primary purpose of a window server protocol is to
provide a mechanism by which clients [e.g. web browser] can submit pixel
buffers. The various pixel buffers from different clients are composited, and
ultimately displayed on a screen.

The core protocol is intentionally minimalist. It supports basic event handling,
message passing and transactional buffer submissions. Wayland supports
extensions, which allow for extensive customization of the protocol.

The canonical reference implementation of a Wayland server is Weston. Chrome has
a custom implementation used on chromeOS called
[exo](https://source.chromium.org/chromium/chromium/src/+/main:components/exo/).

This directory contains Chrome's implementation of the Wayland client. The gpu/
subdirectory contains code typically run in the GPU process, and the host/
subdirectory contains code typically run in the browser process. A typical
high-level control flow for displaying content looks something like this:

* The browser process is the sole Wayland client, and is responsible for all
communication with the Wayland server. It is responsible for configuring window
settings, and routes input.
* The GPU process is responsible for actually drawing content. First, it
allocates or reuses a buffer. See subclasses of WaylandSurfaceGpu for types of
buffers.
* The GPU process then registers the buffer with the Wayland server. This is
done via IPC to the browser process and results in the creation of a wl_buffer
object in the browser process. See CreateShmBasedBuffer() and
CreateDmabufBasedBuffer().
* The GPU process draws into the buffer.
* The GPU process commits the buffer for presentation. This is done via IPC to
the browser process. See CommitBuffer().
* The browser process eventually returns with OnSubmission() and
OnPresentation(), which mark the buffer as ready for reuse.

