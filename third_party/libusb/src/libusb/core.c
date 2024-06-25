/* -*- Mode: C; indent-tabs-mode:t ; c-basic-offset:8 -*- */
/*
 * Core functions for libusbx
 * Copyright © 2012-2013 Nathan Hjelm <hjelmn@cs.unm.edu>
 * Copyright © 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "libusbi.h"
#include "hotplug.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
const struct usbi_os_backend * const usbi_backend = &linux_usbfs_backend;
#elif defined(OS_DARWIN)
const struct usbi_os_backend * const usbi_backend = &darwin_backend;
#elif defined(OS_OPENBSD)
const struct usbi_os_backend * const usbi_backend = &openbsd_backend;
#elif defined(OS_NETBSD)
const struct usbi_os_backend * const usbi_backend = &netbsd_backend;
#elif defined(OS_WINDOWS)
const struct usbi_os_backend * const usbi_backend = &windows_backend;
#elif defined(OS_WINCE)
const struct usbi_os_backend * const usbi_backend = &wince_backend;
#else
#error "Unsupported OS"
#endif

struct libusb_context *usbi_default_context = NULL;
static const struct libusb_version libusb_version_internal =
	{ LIBUSB_MAJOR, LIBUSB_MINOR, LIBUSB_MICRO, LIBUSB_NANO,
	  LIBUSB_RC, "http://libusbx.org" };
static int default_context_refcnt = 0;
static usbi_mutex_static_t default_context_lock = USBI_MUTEX_INITIALIZER;
static struct timeval timestamp_origin = { 0, 0 };

usbi_mutex_static_t active_contexts_lock = USBI_MUTEX_INITIALIZER;
struct list_head active_contexts_list;

/**
 * \mainpage libusbx-1.0 API Reference
 *
 * \section intro Introduction
 *
 * libusbx is an open source library that allows you to communicate with USB
 * devices from userspace. For more info, see the
 * <a href="http://libusbx.org">libusbx homepage</a>.
 *
 * This documentation is aimed at application developers wishing to
 * communicate with USB peripherals from their own software. After reviewing
 * this documentation, feedback and questions can be sent to the
 * <a href="http://mailing-list.libusbx.org">libusbx-devel mailing list</a>.
 *
 * This documentation assumes knowledge of how to operate USB devices from
 * a software standpoint (descriptors, configurations, interfaces, endpoints,
 * control/bulk/interrupt/isochronous transfers, etc). Full information
 * can be found in the <a href="http://www.usb.org/developers/docs/">USB 3.0
 * Specification</a> which is available for free download. You can probably
 * find less verbose introductions by searching the web.
 *
 * \section features Library features
 *
 * - All transfer types supported (control/bulk/interrupt/isochronous)
 * - 2 transfer interfaces:
 *    -# Synchronous (simple)
 *    -# Asynchronous (more complicated, but more powerful)
 * - Thread safe (although the asynchronous interface means that you
 *   usually won't need to thread)
 * - Lightweight with lean API
 * - Compatible with libusb-0.1 through the libusb-compat-0.1 translation layer
 * - Hotplug support (on some platforms). See \ref hotplug.
 *
 * \section gettingstarted Getting Started
 *
 * To begin reading the API documentation, start with the Modules page which
 * links to the different categories of libusbx's functionality.
 *
 * One decision you will have to make is whether to use the synchronous
 * or the asynchronous data transfer interface. The \ref io documentation
 * provides some insight into this topic.
 *
 * Some example programs can be found in the libusbx source distribution under
 * the "examples" subdirectory. The libusbx homepage includes a list of
 * real-life project examples which use libusbx.
 *
 * \section errorhandling Error handling
 *
 * libusbx functions typically return 0 on success or a negative error code
 * on failure. These negative error codes relate to LIBUSB_ERROR constants
 * which are listed on the \ref misc "miscellaneous" documentation page.
 *
 * \section msglog Debug message logging
 *
 * libusbx uses stderr for all logging. By default, logging is set to NONE,
 * which means that no output will be produced. However, unless the library
 * has been compiled with logging disabled, then any application calls to
 * libusb_set_debug(), or the setting of the environmental variable
 * LIBUSB_DEBUG outside of the application, can result in logging being
 * produced. Your application should therefore not close stderr, but instead
 * direct it to the null device if its output is undesireable.
 *
 * The libusb_set_debug() function can be used to enable logging of certain
 * messages. Under standard configuration, libusbx doesn't really log much
 * so you are advised to use this function to enable all error/warning/
 * informational messages. It will help debug problems with your software.
 *
 * The logged messages are unstructured. There is no one-to-one correspondence
 * between messages being logged and success or failure return codes from
 * libusbx functions. There is no format to the messages, so you should not
 * try to capture or parse them. They are not and will not be localized.
 * These messages are not intended to being passed to your application user;
 * instead, you should interpret the error codes returned from libusbx functions
 * and provide appropriate notification to the user. The messages are simply
 * there to aid you as a programmer, and if you're confused because you're
 * getting a strange error code from a libusbx function, enabling message
 * logging may give you a suitable explanation.
 *
 * The LIBUSB_DEBUG environment variable can be used to enable message logging
 * at run-time. This environment variable should be set to a log level number,
 * which is interpreted the same as the libusb_set_debug() parameter. When this
 * environment variable is set, the message logging verbosity level is fixed
 * and libusb_set_debug() effectively does nothing.
 *
 * libusbx can be compiled without any logging functions, useful for embedded
 * systems. In this case, libusb_set_debug() and the LIBUSB_DEBUG environment
 * variable have no effects.
 *
 * libusbx can also be compiled with verbose debugging messages always. When
 * the library is compiled in this way, all messages of all verbosities are
 * always logged. libusb_set_debug() and the LIBUSB_DEBUG environment variable
 * have no effects.
 *
 * \section remarks Other remarks
 *
 * libusbx does have imperfections. The \ref caveats "caveats" page attempts
 * to document these.
 */

/**
 * \page caveats Caveats
 *
 * \section devresets Device resets
 *
 * The libusb_reset_device() function allows you to reset a device. If your
 * program has to call such a function, it should obviously be aware that
 * the reset will cause device state to change (e.g. register values may be
 * reset).
 *
 * The problem is that any other program could reset the device your program
 * is working with, at any time. libusbx does not offer a mechanism to inform
 * you when this has happened, so if someone else resets your device it will
 * not be clear to your own program why the device state has changed.
 *
 * Ultimately, this is a limitation of writing drivers in userspace.
 * Separation from the USB stack in the underlying kernel makes it difficult
 * for the operating system to deliver such notifications to your program.
 * The Linux kernel USB stack allows such reset notifications to be delivered
 * to in-kernel USB drivers, but it is not clear how such notifications could
 * be delivered to second-class drivers that live in userspace.
 *
 * \section blockonly Blocking-only functionality
 *
 * The functionality listed below is only available through synchronous,
 * blocking functions. There are no asynchronous/non-blocking alternatives,
 * and no clear ways of implementing these.
 *
 * - Configuration activation (libusb_set_configuration())
 * - Interface/alternate setting activation (libusb_set_interface_alt_setting())
 * - Releasing of interfaces (libusb_release_interface())
 * - Clearing of halt/stall condition (libusb_clear_halt())
 * - Device resets (libusb_reset_device())
 *
 * \section configsel Configuration selection and handling
 *
 * When libusbx presents a device handle to an application, there is a chance
 * that the corresponding device may be in unconfigured state. For devices
 * with multiple configurations, there is also a chance that the configuration
 * currently selected is not the one that the application wants to use.
 *
 * The obvious solution is to add a call to libusb_set_configuration() early
 * on during your device initialization routines, but there are caveats to
 * be aware of:
 * -# If the device is already in the desired configuration, calling
 *    libusb_set_configuration() using the same configuration value will cause
 *    a lightweight device reset. This may not be desirable behaviour.
 * -# libusbx will be unable to change configuration if the device is in
 *    another configuration and other programs or drivers have claimed
 *    interfaces under that configuration.
 * -# In the case where the desired configuration is already active, libusbx
 *    may not even be able to perform a lightweight device reset. For example,
 *    take my USB keyboard with fingerprint reader: I'm interested in driving
 *    the fingerprint reader interface through libusbx, but the kernel's
 *    USB-HID driver will almost always have claimed the keyboard interface.
 *    Because the kernel has claimed an interface, it is not even possible to
 *    perform the lightweight device reset, so libusb_set_configuration() will
 *    fail. (Luckily the device in question only has a single configuration.)
 *
 * One solution to some of the above problems is to consider the currently
 * active configuration. If the configuration we want is already active, then
 * we don't have to select any configuration:
\code
cfg = libusb_get_configuration(dev);
if (cfg != desired)
	libusb_set_configuration(dev, desired);
\endcode
 *
 * This is probably suitable for most scenarios, but is inherently racy:
 * another application or driver may change the selected configuration
 * <em>after</em> the libusb_get_configuration() call.
 *
 * Even in cases where libusb_set_configuration() succeeds, consider that other
 * applications or drivers may change configuration after your application
 * calls libusb_set_configuration().
 *
 * One possible way to lock your device into a specific configuration is as
 * follows:
 * -# Set the desired configuration (or use the logic above to realise that
 *    it is already in the desired configuration)
 * -# Claim the interface that you wish to use
 * -# Check that the currently active configuration is the one that you want
 *    to use.
 *
 * The above method works because once an interface is claimed, no application
 * or driver is able to select another configuration.
 *
 * \section earlycomp Early transfer completion
 *
 * NOTE: This section is currently Linux-centric. I am not sure if any of these
 * considerations apply to Darwin or other platforms.
 *
 * When a transfer completes early (i.e. when less data is received/sent in
 * any one packet than the transfer buffer allows for) then libusbx is designed
 * to terminate the transfer immediately, not transferring or receiving any
 * more data unless other transfers have been queued by the user.
 *
 * On legacy platforms, libusbx is unable to do this in all situations. After
 * the incomplete packet occurs, "surplus" data may be transferred. For recent
 * versions of libusbx, this information is kept (the data length of the
 * transfer is updated) and, for device-to-host transfers, any surplus data was
 * added to the buffer. Still, this is not a nice solution because it loses the
 * information about the end of the short packet, and the user probably wanted
 * that surplus data to arrive in the next logical transfer.
 *
 *
 * \section zlp Zero length packets
 *
 * - libusbx is able to send a packet of zero length to an endpoint simply by
 * submitting a transfer of zero length.
 * - The \ref libusb_transfer_flags::LIBUSB_TRANSFER_ADD_ZERO_PACKET
 * "LIBUSB_TRANSFER_ADD_ZERO_PACKET" flag is currently only supported on Linux.
 */

/**
 * \page contexts Contexts
 *
 * It is possible that libusbx may be used simultaneously from two independent
 * libraries linked into the same executable. For example, if your application
 * has a plugin-like system which allows the user to dynamically load a range
 * of modules into your program, it is feasible that two independently
 * developed modules may both use libusbx.
 *
 * libusbx is written to allow for these multiple user scenarios. The two
 * "instances" of libusbx will not interfere: libusb_set_debug() calls
 * from one user will not affect the same settings for other users, other
 * users can continue using libusbx after one of them calls libusb_exit(), etc.
 *
 * This is made possible through libusbx's <em>context</em> concept. When you
 * call libusb_init(), you are (optionally) given a context. You can then pass
 * this context pointer back into future libusbx functions.
 *
 * In order to keep things simple for more simplistic applications, it is
 * legal to pass NULL to all functions requiring a context pointer (as long as
 * you're sure no other code will attempt to use libusbx from the same process).
 * When you pass NULL, the default context will be used. The default context
 * is created the first time a process calls libusb_init() when no other
 * context is alive. Contexts are destroyed during libusb_exit().
 *
 * The default context is reference-counted and can be shared. That means that
 * if libusb_init(NULL) is called twice within the same process, the two
 * users end up sharing the same context. The deinitialization and freeing of
 * the default context will only happen when the last user calls libusb_exit().
 * In other words, the default context is created and initialized when its
 * reference count goes from 0 to 1, and is deinitialized and destroyed when
 * its reference count goes from 1 to 0.
 *
 * You may be wondering why only a subset of libusbx functions require a
 * context pointer in their function definition. Internally, libusbx stores
 * context pointers in other objects (e.g. libusb_device instances) and hence
 * can infer the context from those objects.
 */

/**
 * @defgroup lib Library initialization/deinitialization
 * This page details how to initialize and deinitialize libusbx. Initialization
 * must be performed before using any libusbx functionality, and similarly you
 * must not call any libusbx functions after deinitialization.
 */

/**
 * @defgroup dev Device handling and enumeration
 * The functionality documented below is designed to help with the following
 * operations:
 * - Enumerating the USB devices currently attached to the system
 * - Choosing a device to operate from your software
 * - Opening and closing the chosen device
 *
 * \section nutshell In a nutshell...
 *
 * The description below really makes things sound more complicated than they
 * actually are. The following sequence of function calls will be suitable
 * for almost all scenarios and does not require you to have such a deep
 * understanding of the resource management issues:
 * \code
// discover devices
libusb_device **list;
libusb_device *found = NULL;
ssize_t cnt = libusb_get_device_list(NULL, &list);
ssize_t i = 0;
int err = 0;
if (cnt < 0)
	error();

for (i = 0; i < cnt; i++) {
	libusb_device *device = list[i];
	if (is_interesting(device)) {
		found = device;
		break;
	}
}

if (found) {
	libusb_device_handle *handle;

	err = libusb_open(found, &handle);
	if (err)
		error();
	// etc
}

libusb_free_device_list(list, 1);
\endcode
 *
 * The two important points:
 * - You asked libusb_free_device_list() to unreference the devices (2nd
 *   parameter)
 * - You opened the device before freeing the list and unreferencing the
 *   devices
 *
 * If you ended up with a handle, you can now proceed to perform I/O on the
 * device.
 *
 * \section devshandles Devices and device handles
 * libusbx has a concept of a USB device, represented by the
 * \ref libusb_device opaque type. A device represents a USB device that
 * is currently or was previously connected to the system. Using a reference
 * to a device, you can determine certain information about the device (e.g.
 * you can read the descriptor data).
 *
 * The libusb_get_device_list() function can be used to obtain a list of
 * devices currently connected to the system. This is known as device
 * discovery.
 *
 * Just because you have a reference to a device does not mean it is
 * necessarily usable. The device may have been unplugged, you may not have
 * permission to operate such device, or another program or driver may be
 * using the device.
 *
 * When you've found a device that you'd like to operate, you must ask
 * libusbx to open the device using the libusb_open() function. Assuming
 * success, libusbx then returns you a <em>device handle</em>
 * (a \ref libusb_device_handle pointer). All "real" I/O operations then
 * operate on the handle rather than the original device pointer.
 *
 * \section devref Device discovery and reference counting
 *
 * Device discovery (i.e. calling libusb_get_device_list()) returns a
 * freshly-allocated list of devices. The list itself must be freed when
 * you are done with it. libusbx also needs to know when it is OK to free
 * the contents of the list - the devices themselves.
 *
 * To handle these issues, libusbx provides you with two separate items:
 * - A function to free the list itself
 * - A reference counting system for the devices inside
 *
 * New devices presented by the libusb_get_device_list() function all have a
 * reference count of 1. You can increase and decrease reference count using
 * libusb_ref_device() and libusb_unref_device(). A device is destroyed when
 * its reference count reaches 0.
 *
 * With the above information in mind, the process of opening a device can
 * be viewed as follows:
 * -# Discover devices using libusb_get_device_list().
 * -# Choose the device that you want to operate, and call libusb_open().
 * -# Unref all devices in the discovered device list.
 * -# Free the discovered device list.
 *
 * The order is important - you must not unreference the device before
 * attempting to open it, because unreferencing it may destroy the device.
 *
 * For convenience, the libusb_free_device_list() function includes a
 * parameter to optionally unreference all the devices in the list before
 * freeing the list itself. This combines steps 3 and 4 above.
 *
 * As an implementation detail, libusb_open() actually adds a reference to
 * the device in question. This is because the device remains available
 * through the handle via libusb_get_device(). The reference is deleted during
 * libusb_close().
 */

/** @defgroup misc Miscellaneous */

/* we traverse usbfs without knowing how many devices we are going to find.
 * so we create this discovered_devs model which is similar to a linked-list
 * which grows when required. it can be freed once discovery has completed,
 * eliminating the need for a list node in the libusb_device structure
 * itself. */
#define DISCOVERED_DEVICES_SIZE_STEP 8

static struct discovered_devs *discovered_devs_alloc(void)
{
	struct discovered_devs *ret =
		malloc(sizeof(*ret) + (sizeof(void *) * DISCOVERED_DEVICES_SIZE_STEP));

	if (ret) {
		ret->len = 0;
		ret->capacity = DISCOVERED_DEVICES_SIZE_STEP;
	}
	return ret;
}

/* append a device to the discovered devices collection. may realloc itself,
 * returning new discdevs. returns NULL on realloc failure. */
struct discovered_devs *discovered_devs_append(
	struct discovered_devs *discdevs, struct libusb_device *dev)
{
	size_t len = discdevs->len;
	size_t capacity;

	/* if there is space, just append the device */
	if (len < discdevs->capacity) {
		discdevs->devices[len] = libusb_ref_device(dev);
		discdevs->len++;
		return discdevs;
	}

	/* exceeded capacity, need to grow */
	usbi_dbg("need to increase capacity");
	capacity = discdevs->capacity + DISCOVERED_DEVICES_SIZE_STEP;
	discdevs = usbi_reallocf(discdevs,
		sizeof(*discdevs) + (sizeof(void *) * capacity));
	if (discdevs) {
		discdevs->capacity = capacity;
		discdevs->devices[len] = libusb_ref_device(dev);
		discdevs->len++;
	}

	return discdevs;
}

static void discovered_devs_free(struct discovered_devs *discdevs)
{
	size_t i;

	for (i = 0; i < discdevs->len; i++)
		libusb_unref_device(discdevs->devices[i]);

	free(discdevs);
}

/* Allocate a new device with a specific session ID. The returned device has
 * a reference count of 1. */
struct libusb_device *usbi_alloc_device(struct libusb_context *ctx,
	unsigned long session_id)
{
	size_t priv_size = usbi_backend->device_priv_size;
	struct libusb_device *dev = calloc(1, sizeof(*dev) + priv_size);
	int r;

	if (!dev)
		return NULL;

	r = usbi_mutex_init(&dev->lock, NULL);
	if (r) {
		free(dev);
		return NULL;
	}

	dev->ctx = ctx;
	dev->refcnt = 1;
	dev->session_data = session_id;
	dev->speed = LIBUSB_SPEED_UNKNOWN;

	if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		usbi_connect_device (dev);
	}

	return dev;
}

void usbi_connect_device(struct libusb_device *dev)
{
	libusb_hotplug_message message;
	ssize_t ret;

	memset(&message, 0, sizeof(message));
	message.event = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED;
	message.device = dev;
	dev->attached = 1;

	usbi_mutex_lock(&dev->ctx->usb_devs_lock);
	list_add(&dev->list, &dev->ctx->usb_devs);
	usbi_mutex_unlock(&dev->ctx->usb_devs_lock);

	/* Signal that an event has occurred for this device if we support hotplug AND
	 * the hotplug pipe is ready. This prevents an event from getting raised during
	 * initial enumeration. */
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) && dev->ctx->hotplug_pipe[1] > 0) {
		ret = usbi_write(dev->ctx->hotplug_pipe[1], &message, sizeof(message));
		if (sizeof (message) != ret) {
			usbi_err(DEVICE_CTX(dev), "error writing hotplug message");
		}
	}
}

void usbi_disconnect_device(struct libusb_device *dev)
{
	libusb_hotplug_message message;
	struct libusb_context *ctx = dev->ctx;
	ssize_t ret;

	memset(&message, 0, sizeof(message));
	message.event = LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
	message.device = dev;
	usbi_mutex_lock(&dev->lock);
	dev->attached = 0;
	usbi_mutex_unlock(&dev->lock);

	usbi_mutex_lock(&ctx->usb_devs_lock);
	list_del(&dev->list);
	usbi_mutex_unlock(&ctx->usb_devs_lock);

	/* Signal that an event has occurred for this device if we support hotplug AND
	 * the hotplug pipe is ready. This prevents an event from getting raised during
	 * initial enumeration. libusb_handle_events will take care of dereferencing the
	 * device. */
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) && dev->ctx->hotplug_pipe[1] > 0) {
		ret = usbi_write(dev->ctx->hotplug_pipe[1], &message, sizeof(message));
		if (sizeof(message) != ret) {
			usbi_err(DEVICE_CTX(dev), "error writing hotplug message");
		}
	}
}

/* Perform some final sanity checks on a newly discovered device. If this
 * function fails (negative return code), the device should not be added
 * to the discovered device list. */
int usbi_sanitize_device(struct libusb_device *dev)
{
	int r;
	uint8_t num_configurations;

	r = usbi_device_cache_descriptor(dev);
	if (r < 0)
		return r;

	num_configurations = dev->device_descriptor.bNumConfigurations;
	if (num_configurations > USB_MAXCONFIG) {
		usbi_err(DEVICE_CTX(dev), "too many configurations");
		return LIBUSB_ERROR_IO;
	} else if (0 == num_configurations)
		usbi_dbg("zero configurations, maybe an unauthorized device");

	dev->num_configurations = num_configurations;
	return 0;
}

/* Examine libusbx's internal list of known devices, looking for one with
 * a specific session ID. Returns the matching device if it was found, and
 * NULL otherwise. */
struct libusb_device *usbi_get_device_by_session_id(struct libusb_context *ctx,
	unsigned long session_id)
{
	struct libusb_device *dev;
	struct libusb_device *ret = NULL;

	usbi_mutex_lock(&ctx->usb_devs_lock);
	list_for_each_entry(dev, &ctx->usb_devs, list, struct libusb_device)
		if (dev->session_data == session_id) {
			ret = dev;
			break;
		}
	usbi_mutex_unlock(&ctx->usb_devs_lock);

	return ret;
}

/** @ingroup dev
 * Returns a list of USB devices currently attached to the system. This is
 * your entry point into finding a USB device to operate.
 *
 * You are expected to unreference all the devices when you are done with
 * them, and then free the list with libusb_free_device_list(). Note that
 * libusb_free_device_list() can unref all the devices for you. Be careful
 * not to unreference a device you are about to open until after you have
 * opened it.
 *
 * This return value of this function indicates the number of devices in
 * the resultant list. The list is actually one element larger, as it is
 * NULL-terminated.
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param list output location for a list of devices. Must be later freed with
 * libusb_free_device_list().
 * \returns the number of devices in the outputted list, or any
 * \ref libusb_error according to errors encountered by the backend.
 */
ssize_t API_EXPORTED libusb_get_device_list(libusb_context *ctx,
	libusb_device ***list)
{
	struct discovered_devs *discdevs = discovered_devs_alloc();
	struct libusb_device **ret;
	int r = 0;
	ssize_t i, len;
	USBI_GET_CONTEXT(ctx);
	usbi_dbg("");

	if (!discdevs)
		return LIBUSB_ERROR_NO_MEM;

	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		/* backend provides hotplug support */
		struct libusb_device *dev;

		if (usbi_backend->hotplug_poll)
			usbi_backend->hotplug_poll();

		usbi_mutex_lock(&ctx->usb_devs_lock);
		list_for_each_entry(dev, &ctx->usb_devs, list, struct libusb_device) {
			discdevs = discovered_devs_append(discdevs, dev);

			if (!discdevs) {
				r = LIBUSB_ERROR_NO_MEM;
				break;
			}
		}
		usbi_mutex_unlock(&ctx->usb_devs_lock);
	} else {
		/* backend does not provide hotplug support */
		r = usbi_backend->get_device_list(ctx, &discdevs);
	}

	if (r < 0) {
		len = r;
		goto out;
	}

	/* convert discovered_devs into a list */
	len = discdevs->len;
	ret = calloc(len + 1, sizeof(struct libusb_device *));
	if (!ret) {
		len = LIBUSB_ERROR_NO_MEM;
		goto out;
	}

	ret[len] = NULL;
	for (i = 0; i < len; i++) {
		struct libusb_device *dev = discdevs->devices[i];
		ret[i] = libusb_ref_device(dev);
	}
	*list = ret;

out:
	discovered_devs_free(discdevs);
	return len;
}

/** \ingroup dev
 * Frees a list of devices previously discovered using
 * libusb_get_device_list(). If the unref_devices parameter is set, the
 * reference count of each device in the list is decremented by 1.
 * \param list the list to free
 * \param unref_devices whether to unref the devices in the list
 */
void API_EXPORTED libusb_free_device_list(libusb_device **list,
	int unref_devices)
{
	if (!list)
		return;

	if (unref_devices) {
		int i = 0;
		struct libusb_device *dev;

		while ((dev = list[i++]) != NULL)
			libusb_unref_device(dev);
	}
	free(list);
}

/** \ingroup dev
 * Get the number of the bus that a device is connected to.
 * \param dev a device
 * \returns the bus number
 */
uint8_t API_EXPORTED libusb_get_bus_number(libusb_device *dev)
{
	return dev->bus_number;
}

/** \ingroup dev
 * Get the number of the port that a device is connected to.
 * Unless the OS does something funky, or you are hot-plugging USB extension cards,
 * the port number returned by this call is usually guaranteed to be uniquely tied
 * to a physical port, meaning that different devices plugged on the same physical
 * port should return the same port number.
 *
 * But outside of this, there is no guarantee that the port number returned by this
 * call will remain the same, or even match the order in which ports have been
 * numbered by the HUB/HCD manufacturer.
 *
 * \param dev a device
 * \returns the port number (0 if not available)
 */
uint8_t API_EXPORTED libusb_get_port_number(libusb_device *dev)
{
	return dev->port_number;
}

/** \ingroup dev
 * Get the list of all port numbers from root for the specified device
 *
 * Since version 1.0.16, \ref LIBUSBX_API_VERSION >= 0x01000102
 * \param dev a device
 * \param port_numbers the array that should contain the port numbers
 * \param port_numbers_len the maximum length of the array. As per the USB 3.0
 * specs, the current maximum limit for the depth is 7.
 * \returns the number of elements filled
 * \returns LIBUSB_ERROR_OVERFLOW if the array is too small
 */
int API_EXPORTED libusb_get_port_numbers(libusb_device *dev,
	uint8_t* port_numbers, int port_numbers_len)
{
	int i = port_numbers_len;

	while(dev) {
		// HCDs can be listed as devices and would have port #0
		// TODO: see how the other backends want to implement HCDs as parents
		if (dev->port_number == 0)
			break;
		i--;
		if (i < 0) {
			usbi_warn(DEVICE_CTX(dev),
				"port numbers array too small");
			return LIBUSB_ERROR_OVERFLOW;
		}
		port_numbers[i] = dev->port_number;
		dev = dev->parent_dev;
	}
	memmove(port_numbers, &port_numbers[i], port_numbers_len - i);
	return port_numbers_len - i;
}

/** \ingroup dev
 * Deprecated please use libusb_get_port_numbers instead.
 */
int API_EXPORTED libusb_get_port_path(libusb_context *ctx, libusb_device *dev,
	uint8_t* port_numbers, uint8_t port_numbers_len)
{
	UNUSED(ctx);

	return libusb_get_port_numbers(dev, port_numbers, port_numbers_len);
}

/** \ingroup dev
 * Get the the parent from the specified device.
 * \param dev a device
 * \returns the device parent or NULL if not available
 * You should issue a \ref libusb_get_device_list() before calling this
 * function and make sure that you only access the parent before issuing
 * \ref libusb_free_device_list(). The reason is that libusbx currently does
 * not maintain a permanent list of device instances, and therefore can
 * only guarantee that parents are fully instantiated within a 
 * libusb_get_device_list() - libusb_free_device_list() block.
 */
DEFAULT_VISIBILITY
libusb_device * LIBUSB_CALL libusb_get_parent(libusb_device *dev)
{
	return dev->parent_dev;
}

/** \ingroup dev
 * Get the address of the device on the bus it is connected to.
 * \param dev a device
 * \returns the device address
 */
uint8_t API_EXPORTED libusb_get_device_address(libusb_device *dev)
{
	return dev->device_address;
}

/** \ingroup dev
 * Get the negotiated connection speed for a device.
 * \param dev a device
 * \returns a \ref libusb_speed code, where LIBUSB_SPEED_UNKNOWN means that
 * the OS doesn't know or doesn't support returning the negotiated speed.
 */
int API_EXPORTED libusb_get_device_speed(libusb_device *dev)
{
	return dev->speed;
}

static const struct libusb_endpoint_descriptor *find_endpoint(
	struct libusb_config_descriptor *config, unsigned char endpoint)
{
	int iface_idx;
	for (iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) {
		const struct libusb_interface *iface = &config->interface[iface_idx];
		int altsetting_idx;

		for (altsetting_idx = 0; altsetting_idx < iface->num_altsetting;
				altsetting_idx++) {
			const struct libusb_interface_descriptor *altsetting
				= &iface->altsetting[altsetting_idx];
			int ep_idx;

			for (ep_idx = 0; ep_idx < altsetting->bNumEndpoints; ep_idx++) {
				const struct libusb_endpoint_descriptor *ep =
					&altsetting->endpoint[ep_idx];
				if (ep->bEndpointAddress == endpoint)
					return ep;
			}
		}
	}
	return NULL;
}

/** \ingroup dev
 * Convenience function to retrieve the wMaxPacketSize value for a particular
 * endpoint in the active device configuration.
 *
 * This function was originally intended to be of assistance when setting up
 * isochronous transfers, but a design mistake resulted in this function
 * instead. It simply returns the wMaxPacketSize value without considering
 * its contents. If you're dealing with isochronous transfers, you probably
 * want libusb_get_max_iso_packet_size() instead.
 *
 * \param dev a device
 * \param endpoint address of the endpoint in question
 * \returns the wMaxPacketSize value
 * \returns LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
 * \returns LIBUSB_ERROR_OTHER on other failure
 */
int API_EXPORTED libusb_get_max_packet_size(libusb_device *dev,
	unsigned char endpoint)
{
	struct libusb_config_descriptor *config;
	const struct libusb_endpoint_descriptor *ep;
	int r;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		usbi_err(DEVICE_CTX(dev),
			"could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	ep = find_endpoint(config, endpoint);
	if (!ep)
		return LIBUSB_ERROR_NOT_FOUND;

	r = ep->wMaxPacketSize;
	libusb_free_config_descriptor(config);
	return r;
}

/** \ingroup dev
 * Calculate the maximum packet size which a specific endpoint is capable is
 * sending or receiving in the duration of 1 microframe
 *
 * Only the active configuration is examined. The calculation is based on the
 * wMaxPacketSize field in the endpoint descriptor as described in section
 * 9.6.6 in the USB 2.0 specifications.
 *
 * If acting on an isochronous or interrupt endpoint, this function will
 * multiply the value found in bits 0:10 by the number of transactions per
 * microframe (determined by bits 11:12). Otherwise, this function just
 * returns the numeric value found in bits 0:10.
 *
 * This function is useful for setting up isochronous transfers, for example
 * you might pass the return value from this function to
 * libusb_set_iso_packet_lengths() in order to set the length field of every
 * isochronous packet in a transfer.
 *
 * Since v1.0.3.
 *
 * \param dev a device
 * \param endpoint address of the endpoint in question
 * \returns the maximum packet size which can be sent/received on this endpoint
 * \returns LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
 * \returns LIBUSB_ERROR_OTHER on other failure
 */
int API_EXPORTED libusb_get_max_iso_packet_size(libusb_device *dev,
	unsigned char endpoint)
{
	struct libusb_config_descriptor *config;
	const struct libusb_endpoint_descriptor *ep;
	enum libusb_transfer_type ep_type;
	uint16_t val;
	int r;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		usbi_err(DEVICE_CTX(dev),
			"could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	ep = find_endpoint(config, endpoint);
	if (!ep)
		return LIBUSB_ERROR_NOT_FOUND;

	val = ep->wMaxPacketSize;
	ep_type = (enum libusb_transfer_type) (ep->bmAttributes & 0x3);
	libusb_free_config_descriptor(config);

	r = val & 0x07ff;
	if (ep_type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS
			|| ep_type == LIBUSB_TRANSFER_TYPE_INTERRUPT)
		r *= (1 + ((val >> 11) & 3));
	return r;
}

/** \ingroup dev
 * Increment the reference count of a device.
 * \param dev the device to reference
 * \returns the same device
 */
DEFAULT_VISIBILITY
libusb_device * LIBUSB_CALL libusb_ref_device(libusb_device *dev)
{
	usbi_mutex_lock(&dev->lock);
	dev->refcnt++;
	usbi_mutex_unlock(&dev->lock);
	return dev;
}

/** \ingroup dev
 * Decrement the reference count of a device. If the decrement operation
 * causes the reference count to reach zero, the device shall be destroyed.
 * \param dev the device to unreference
 */
void API_EXPORTED libusb_unref_device(libusb_device *dev)
{
	int refcnt;

	if (!dev)
		return;

	usbi_mutex_lock(&dev->lock);
	refcnt = --dev->refcnt;
	usbi_mutex_unlock(&dev->lock);

	if (refcnt == 0) {
		usbi_dbg("destroy device %d.%d", dev->bus_number, dev->device_address);

		libusb_unref_device(dev->parent_dev);

		if (usbi_backend->destroy_device)
			usbi_backend->destroy_device(dev);

		if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
			/* backend does not support hotplug */
			usbi_disconnect_device(dev);
		}

		usbi_mutex_destroy(&dev->lock);
		free(dev);
	}
}

/*
 * Interrupt the iteration of the event handling thread, so that it picks
 * up the new fd.
 */
void usbi_fd_notification(struct libusb_context *ctx)
{
	unsigned char dummy = 1;
	ssize_t r;

	if (ctx == NULL)
		return;

	/* record that we are messing with poll fds */
	usbi_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	usbi_mutex_unlock(&ctx->pollfd_modify_lock);

	/* write some data on control pipe to interrupt event handlers */
	r = usbi_write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	if (r <= 0) {
		usbi_warn(ctx, "internal signalling write failed");
		usbi_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		usbi_mutex_unlock(&ctx->pollfd_modify_lock);
		return;
	}

	/* take event handling lock */
	libusb_lock_events(ctx);

	/* read the dummy data */
	r = usbi_read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	if (r <= 0)
		usbi_warn(ctx, "internal signalling read failed");

	/* we're done with modifying poll fds */
	usbi_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	usbi_mutex_unlock(&ctx->pollfd_modify_lock);

	/* Release event handling lock and wake up event waiters */
	libusb_unlock_events(ctx);
}

/** \ingroup dev
 * Open a device and obtain a device handle. A handle allows you to perform
 * I/O on the device in question.
 *
 * Internally, this function adds a reference to the device and makes it
 * available to you through libusb_get_device(). This reference is removed
 * during libusb_close().
 *
 * This is a non-blocking function; no requests are sent over the bus.
 *
 * \param dev the device to open
 * \param handle output location for the returned device handle pointer. Only
 * populated when the return code is 0.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NO_MEM on memory allocation failure
 * \returns LIBUSB_ERROR_ACCESS if the user has insufficient permissions
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_open(libusb_device *dev,
	libusb_device_handle **handle)
{
	struct libusb_context *ctx = DEVICE_CTX(dev);
	struct libusb_device_handle *_handle;
	size_t priv_size = usbi_backend->device_handle_priv_size;
	int r;
	usbi_dbg("open %d.%d", dev->bus_number, dev->device_address);

	if (!dev->attached) {
		return LIBUSB_ERROR_NO_DEVICE;
	}

	_handle = malloc(sizeof(*_handle) + priv_size);
	if (!_handle)
		return LIBUSB_ERROR_NO_MEM;

	r = usbi_mutex_init(&_handle->lock, NULL);
	if (r) {
		free(_handle);
		return LIBUSB_ERROR_OTHER;
	}

	_handle->dev = libusb_ref_device(dev);
	_handle->auto_detach_kernel_driver = 0;
	_handle->claimed_interfaces = 0;
	memset(&_handle->os_priv, 0, priv_size);

	r = usbi_backend->open(_handle);
	if (r < 0) {
		usbi_dbg("open %d.%d returns %d", dev->bus_number, dev->device_address, r);
		libusb_unref_device(dev);
		usbi_mutex_destroy(&_handle->lock);
		free(_handle);
		return r;
	}

	usbi_mutex_lock(&ctx->open_devs_lock);
	list_add(&_handle->list, &ctx->open_devs);
	usbi_mutex_unlock(&ctx->open_devs_lock);
	*handle = _handle;

	/* At this point, we want to interrupt any existing event handlers so
	 * that they realise the addition of the new device's poll fd. One
	 * example when this is desirable is if the user is running a separate
	 * dedicated libusbx events handling thread, which is running with a long
	 * or infinite timeout. We want to interrupt that iteration of the loop,
	 * so that it picks up the new fd, and then continues. */
	usbi_fd_notification(ctx);

	return 0;
}

/** \ingroup dev
 * Open a device and obtain a device handle. A handle allows you to perform
 * I/O on the device in question.
 *
 * Instead of opening the device itself this function accepts an open file
 * descriptor that it will take ownership of.
 *
 * Internally, this function adds a reference to the device and makes it
 * available to you through libusb_get_device(). This reference is removed
 * during libusb_close().
 *
 * This is a non-blocking function; no requests are sent over the bus.
 *
 * \param dev the device to open
 * \param fd open file handle to the device
 * \param handle output location for the returned device handle pointer. Only
 * populated when the return code is 0.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NO_MEM on memory allocation failure
 * \returns LIBUSB_ERROR_ACCESS if the user has insufficient permissions
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_open_fd(libusb_device *dev,
  int fd,
  libusb_device_handle **handle)
{
  struct libusb_context *ctx = DEVICE_CTX(dev);
  struct libusb_device_handle *_handle;
  size_t priv_size = usbi_backend->device_handle_priv_size;
  int r;
  usbi_dbg("open %d.%d", dev->bus_number, dev->device_address);

  if (!dev->attached) {
    return LIBUSB_ERROR_NO_DEVICE;
  }

  _handle = malloc(sizeof(*_handle) + priv_size);
  if (!_handle)
    return LIBUSB_ERROR_NO_MEM;

  r = usbi_mutex_init(&_handle->lock, NULL);
  if (r) {
    free(_handle);
    return LIBUSB_ERROR_OTHER;
  }

  _handle->dev = libusb_ref_device(dev);
  _handle->auto_detach_kernel_driver = 0;
  _handle->claimed_interfaces = 0;
  memset(&_handle->os_priv, 0, priv_size);

  r = usbi_backend->open_fd(_handle, fd);
  if (r < 0) {
    usbi_dbg("open %d.%d returns %d", dev->bus_number, dev->device_address, r);
    libusb_unref_device(dev);
    usbi_mutex_destroy(&_handle->lock);
    free(_handle);
    return r;
  }

  usbi_mutex_lock(&ctx->open_devs_lock);
  list_add(&_handle->list, &ctx->open_devs);
  usbi_mutex_unlock(&ctx->open_devs_lock);
  *handle = _handle;

  /* At this point, we want to interrupt any existing event handlers so
   * that they realise the addition of the new device's poll fd. One
   * example when this is desirable is if the user is running a separate
   * dedicated libusbx events handling thread, which is running with a long
   * or infinite timeout. We want to interrupt that iteration of the loop,
   * so that it picks up the new fd, and then continues. */
  usbi_fd_notification(ctx);

  return 0;
}

/** \ingroup dev
 * Convenience function for finding a device with a particular
 * <tt>idVendor</tt>/<tt>idProduct</tt> combination. This function is intended
 * for those scenarios where you are using libusbx to knock up a quick test
 * application - it allows you to avoid calling libusb_get_device_list() and
 * worrying about traversing/freeing the list.
 *
 * This function has limitations and is hence not intended for use in real
 * applications: if multiple devices have the same IDs it will only
 * give you the first one, etc.
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param vendor_id the idVendor value to search for
 * \param product_id the idProduct value to search for
 * \returns a handle for the first found device, or NULL on error or if the
 * device could not be found. */
DEFAULT_VISIBILITY
libusb_device_handle * LIBUSB_CALL libusb_open_device_with_vid_pid(
	libusb_context *ctx, uint16_t vendor_id, uint16_t product_id)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device *dev;
	struct libusb_device_handle *handle = NULL;
	size_t i = 0;
	int r;

	if (libusb_get_device_list(ctx, &devs) < 0)
		return NULL;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
			goto out;
		if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
			found = dev;
			break;
		}
	}

	if (found) {
		r = libusb_open(found, &handle);
		if (r < 0)
			handle = NULL;
	}

out:
	libusb_free_device_list(devs, 1);
	return handle;
}

static void do_close(struct libusb_context *ctx,
	struct libusb_device_handle *dev_handle)
{
	struct usbi_transfer *itransfer;
	struct usbi_transfer *tmp;

	libusb_lock_events(ctx);

	/* remove any transfers in flight that are for this device */
	usbi_mutex_lock(&ctx->flying_transfers_lock);

	/* safe iteration because transfers may be being deleted */
	list_for_each_entry_safe(itransfer, tmp, &ctx->flying_transfers, list, struct usbi_transfer) {
		struct libusb_transfer *transfer =
			USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

		if (transfer->dev_handle != dev_handle)
			continue;

		if (!(itransfer->flags & USBI_TRANSFER_DEVICE_DISAPPEARED)) {
			usbi_err(ctx, "Device handle closed while transfer was still being processed, but the device is still connected as far as we know");

			if (itransfer->flags & USBI_TRANSFER_CANCELLING)
				usbi_warn(ctx, "A cancellation for an in-flight transfer hasn't completed but closing the device handle");
			else
				usbi_err(ctx, "A cancellation hasn't even been scheduled on the transfer for which the device is closing");
		}

		/* remove from the list of in-flight transfers and make sure
		 * we don't accidentally use the device handle in the future
		 * (or that such accesses will be easily caught and identified as a crash)
		 */
		usbi_mutex_lock(&itransfer->lock);
		list_del(&itransfer->list);
		transfer->dev_handle = NULL;
		usbi_mutex_unlock(&itransfer->lock);

		/* it is up to the user to free up the actual transfer struct.  this is
		 * just making sure that we don't attempt to process the transfer after
		 * the device handle is invalid
		 */
		usbi_dbg("Removed transfer %p from the in-flight list because device handle %p closed",
			 transfer, dev_handle);
	}
	usbi_mutex_unlock(&ctx->flying_transfers_lock);

	libusb_unlock_events(ctx);

	usbi_mutex_lock(&ctx->open_devs_lock);
	list_del(&dev_handle->list);
	usbi_mutex_unlock(&ctx->open_devs_lock);

	usbi_backend->close(dev_handle);
	libusb_unref_device(dev_handle->dev);
	usbi_mutex_destroy(&dev_handle->lock);
	free(dev_handle);
}

/** \ingroup dev
 * Close a device handle. Should be called on all open handles before your
 * application exits.
 *
 * Internally, this function destroys the reference that was added by
 * libusb_open() on the given device.
 *
 * This is a non-blocking function; no requests are sent over the bus.
 *
 * \param dev_handle the handle to close
 */
void API_EXPORTED libusb_close(libusb_device_handle *dev_handle)
{
	struct libusb_context *ctx;
	unsigned char dummy = 1;
	ssize_t r;

	if (!dev_handle)
		return;
	usbi_dbg("");

	ctx = HANDLE_CTX(dev_handle);

	/* Similarly to libusb_open(), we want to interrupt all event handlers
	 * at this point. More importantly, we want to perform the actual close of
	 * the device while holding the event handling lock (preventing any other
	 * thread from doing event handling) because we will be removing a file
	 * descriptor from the polling loop. */

	/* record that we are messing with poll fds */
	usbi_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	usbi_mutex_unlock(&ctx->pollfd_modify_lock);

	/* write some data on control pipe to interrupt event handlers */
	r = usbi_write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	if (r <= 0) {
		usbi_warn(ctx, "internal signalling write failed, closing anyway");
		do_close(ctx, dev_handle);
		usbi_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		usbi_mutex_unlock(&ctx->pollfd_modify_lock);
		return;
	}

	/* take event handling lock */
	libusb_lock_events(ctx);

	/* read the dummy data */
	r = usbi_read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	if (r <= 0)
		usbi_warn(ctx, "internal signalling read failed, closing anyway");

	/* Close the device */
	do_close(ctx, dev_handle);

	/* we're done with modifying poll fds */
	usbi_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	usbi_mutex_unlock(&ctx->pollfd_modify_lock);

	/* Release event handling lock and wake up event waiters */
	libusb_unlock_events(ctx);
}

/** \ingroup dev
 * Get the underlying device for a handle. This function does not modify
 * the reference count of the returned device, so do not feel compelled to
 * unreference it when you are done.
 * \param dev_handle a device handle
 * \returns the underlying device
 */
DEFAULT_VISIBILITY
libusb_device * LIBUSB_CALL libusb_get_device(libusb_device_handle *dev_handle)
{
	return dev_handle->dev;
}

/** \ingroup dev
 * Determine the bConfigurationValue of the currently active configuration.
 *
 * You could formulate your own control request to obtain this information,
 * but this function has the advantage that it may be able to retrieve the
 * information from operating system caches (no I/O involved).
 *
 * If the OS does not cache this information, then this function will block
 * while a control transfer is submitted to retrieve the information.
 *
 * This function will return a value of 0 in the <tt>config</tt> output
 * parameter if the device is in unconfigured state.
 *
 * \param dev a device handle
 * \param config output location for the bConfigurationValue of the active
 * configuration (only valid for return code 0)
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_get_configuration(libusb_device_handle *dev,
	int *config)
{
	int r = LIBUSB_ERROR_NOT_SUPPORTED;

	usbi_dbg("");
	if (usbi_backend->get_configuration)
		r = usbi_backend->get_configuration(dev, config);

	if (r == LIBUSB_ERROR_NOT_SUPPORTED) {
		uint8_t tmp = 0;
		usbi_dbg("falling back to control message");
		r = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN,
			LIBUSB_REQUEST_GET_CONFIGURATION, 0, 0, &tmp, 1, 1000);
		if (r == 0) {
			usbi_err(HANDLE_CTX(dev), "zero bytes returned in ctrl transfer?");
			r = LIBUSB_ERROR_IO;
		} else if (r == 1) {
			r = 0;
			*config = tmp;
		} else {
			usbi_dbg("control failed, error %d", r);
		}
	}

	if (r == 0)
		usbi_dbg("active config %d", *config);

	return r;
}

/** \ingroup dev
 * Set the active configuration for a device.
 *
 * The operating system may or may not have already set an active
 * configuration on the device. It is up to your application to ensure the
 * correct configuration is selected before you attempt to claim interfaces
 * and perform other operations.
 *
 * If you call this function on a device already configured with the selected
 * configuration, then this function will act as a lightweight device reset:
 * it will issue a SET_CONFIGURATION request using the current configuration,
 * causing most USB-related device state to be reset (altsetting reset to zero,
 * endpoint halts cleared, toggles reset).
 *
 * You cannot change/reset configuration if your application has claimed
 * interfaces. It is advised to set the desired configuration before claiming
 * interfaces.
 *
 * Alternatively you can call libusb_release_interface() first. Note if you
 * do things this way you must ensure that auto_detach_kernel_driver for
 * <tt>dev</tt> is 0, otherwise the kernel driver will be re-attached when you
 * release the interface(s).
 *
 * You cannot change/reset configuration if other applications or drivers have
 * claimed interfaces.
 *
 * A configuration value of -1 will put the device in unconfigured state.
 * The USB specifications state that a configuration value of 0 does this,
 * however buggy devices exist which actually have a configuration 0.
 *
 * You should always use this function rather than formulating your own
 * SET_CONFIGURATION control request. This is because the underlying operating
 * system needs to know when such changes happen.
 *
 * This is a blocking function.
 *
 * \param dev a device handle
 * \param configuration the bConfigurationValue of the configuration you
 * wish to activate, or -1 if you wish to put the device in unconfigured state
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the requested configuration does not exist
 * \returns LIBUSB_ERROR_BUSY if interfaces are currently claimed
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 * \see libusb_set_auto_detach_kernel_driver()
 */
int API_EXPORTED libusb_set_configuration(libusb_device_handle *dev,
	int configuration)
{
	usbi_dbg("configuration %d", configuration);
	return usbi_backend->set_configuration(dev, configuration);
}

/** \ingroup dev
 * Claim an interface on a given device handle. You must claim the interface
 * you wish to use before you can perform I/O on any of its endpoints.
 *
 * It is legal to attempt to claim an already-claimed interface, in which
 * case libusbx just returns 0 without doing anything.
 *
 * If auto_detach_kernel_driver is set to 1 for <tt>dev</tt>, the kernel driver
 * will be detached if necessary, on failure the detach error is returned.
 *
 * Claiming of interfaces is a purely logical operation; it does not cause
 * any requests to be sent over the bus. Interface claiming is used to
 * instruct the underlying operating system that your application wishes
 * to take ownership of the interface.
 *
 * This is a non-blocking function.
 *
 * \param dev a device handle
 * \param interface_number the <tt>bInterfaceNumber</tt> of the interface you
 * wish to claim
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the requested interface does not exist
 * \returns LIBUSB_ERROR_BUSY if another program or driver has claimed the
 * interface
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns a LIBUSB_ERROR code on other failure
 * \see libusb_set_auto_detach_kernel_driver()
 */
int API_EXPORTED libusb_claim_interface(libusb_device_handle *dev,
	int interface_number)
{
	int r = 0;

	usbi_dbg("interface %d", interface_number);
	if (interface_number >= USB_MAXINTERFACES)
		return LIBUSB_ERROR_INVALID_PARAM;

	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	usbi_mutex_lock(&dev->lock);
	if (dev->claimed_interfaces & (1U << interface_number))
		goto out;

	r = usbi_backend->claim_interface(dev, interface_number);
	if (r == 0)
		dev->claimed_interfaces |= 1U << interface_number;

out:
	usbi_mutex_unlock(&dev->lock);
	return r;
}

/** \ingroup dev
 * Release an interface previously claimed with libusb_claim_interface(). You
 * should release all claimed interfaces before closing a device handle.
 *
 * This is a blocking function. A SET_INTERFACE control request will be sent
 * to the device, resetting interface state to the first alternate setting.
 *
 * If auto_detach_kernel_driver is set to 1 for <tt>dev</tt>, the kernel
 * driver will be re-attached after releasing the interface.
 *
 * \param dev a device handle
 * \param interface_number the <tt>bInterfaceNumber</tt> of the
 * previously-claimed interface
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the interface was not claimed
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 * \see libusb_set_auto_detach_kernel_driver()
 */
int API_EXPORTED libusb_release_interface(libusb_device_handle *dev,
	int interface_number)
{
	int r;

	usbi_dbg("interface %d", interface_number);
	if (interface_number >= USB_MAXINTERFACES)
		return LIBUSB_ERROR_INVALID_PARAM;

	usbi_mutex_lock(&dev->lock);
	if (!(dev->claimed_interfaces & (1U << interface_number))) {
		r = LIBUSB_ERROR_NOT_FOUND;
		goto out;
	}

	r = usbi_backend->release_interface(dev, interface_number);
	if (r == 0)
		dev->claimed_interfaces &= ~(1U << interface_number);

out:
	usbi_mutex_unlock(&dev->lock);
	return r;
}

/** \ingroup dev
 * Activate an alternate setting for an interface. The interface must have
 * been previously claimed with libusb_claim_interface().
 *
 * You should always use this function rather than formulating your own
 * SET_INTERFACE control request. This is because the underlying operating
 * system needs to know when such changes happen.
 *
 * This is a blocking function.
 *
 * \param dev a device handle
 * \param interface_number the <tt>bInterfaceNumber</tt> of the
 * previously-claimed interface
 * \param alternate_setting the <tt>bAlternateSetting</tt> of the alternate
 * setting to activate
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the interface was not claimed, or the
 * requested alternate setting does not exist
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_set_interface_alt_setting(libusb_device_handle *dev,
	int interface_number, int alternate_setting)
{
	usbi_dbg("interface %d altsetting %d",
		interface_number, alternate_setting);
	if (interface_number >= USB_MAXINTERFACES)
		return LIBUSB_ERROR_INVALID_PARAM;

	usbi_mutex_lock(&dev->lock);
	if (!dev->dev->attached) {
		usbi_mutex_unlock(&dev->lock);
		return LIBUSB_ERROR_NO_DEVICE;
	}

	if (!(dev->claimed_interfaces & (1U << interface_number))) {
		usbi_mutex_unlock(&dev->lock);
		return LIBUSB_ERROR_NOT_FOUND;
	}
	usbi_mutex_unlock(&dev->lock);

	return usbi_backend->set_interface_altsetting(dev, interface_number,
		alternate_setting);
}

/** \ingroup dev
 * Clear the halt/stall condition for an endpoint. Endpoints with halt status
 * are unable to receive or transmit data until the halt condition is stalled.
 *
 * You should cancel all pending transfers before attempting to clear the halt
 * condition.
 *
 * This is a blocking function.
 *
 * \param dev a device handle
 * \param endpoint the endpoint to clear halt status
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_clear_halt(libusb_device_handle *dev,
	unsigned char endpoint)
{
	usbi_dbg("endpoint %x", endpoint);
	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	return usbi_backend->clear_halt(dev, endpoint);
}

/** \ingroup dev
 * Perform a USB port reset to reinitialize a device. The system will attempt
 * to restore the previous configuration and alternate settings after the
 * reset has completed.
 *
 * If the reset fails, the descriptors change, or the previous state cannot be
 * restored, the device will appear to be disconnected and reconnected. This
 * means that the device handle is no longer valid (you should close it) and
 * rediscover the device. A return code of LIBUSB_ERROR_NOT_FOUND indicates
 * when this is the case.
 *
 * This is a blocking function which usually incurs a noticeable delay.
 *
 * \param dev a handle of the device to reset
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if re-enumeration is required, or if the
 * device has been disconnected
 * \returns another LIBUSB_ERROR code on other failure
 */
int API_EXPORTED libusb_reset_device(libusb_device_handle *dev)
{
	usbi_dbg("");
	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	return usbi_backend->reset_device(dev);
}

/** \ingroup dev
 * Determine if a kernel driver is active on an interface. If a kernel driver
 * is active, you cannot claim the interface, and libusbx will be unable to
 * perform I/O.
 *
 * This functionality is not available on Windows.
 *
 * \param dev a device handle
 * \param interface_number the interface to check
 * \returns 0 if no kernel driver is active
 * \returns 1 if a kernel driver is active
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality
 * is not available
 * \returns another LIBUSB_ERROR code on other failure
 * \see libusb_detach_kernel_driver()
 */
int API_EXPORTED libusb_kernel_driver_active(libusb_device_handle *dev,
	int interface_number)
{
	usbi_dbg("interface %d", interface_number);

	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	if (usbi_backend->kernel_driver_active)
		return usbi_backend->kernel_driver_active(dev, interface_number);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

/** \ingroup dev
 * Detach a kernel driver from an interface. If successful, you will then be
 * able to claim the interface and perform I/O.
 *
 * This functionality is not available on Darwin or Windows.
 *
 * Note that libusbx itself also talks to the device through a special kernel
 * driver, if this driver is already attached to the device, this call will
 * not detach it and return LIBUSB_ERROR_NOT_FOUND.
 *
 * \param dev a device handle
 * \param interface_number the interface to detach the driver from
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if no kernel driver was active
 * \returns LIBUSB_ERROR_INVALID_PARAM if the interface does not exist
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality
 * is not available
 * \returns another LIBUSB_ERROR code on other failure
 * \see libusb_kernel_driver_active()
 */
int API_EXPORTED libusb_detach_kernel_driver(libusb_device_handle *dev,
	int interface_number)
{
	usbi_dbg("interface %d", interface_number);

	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	if (usbi_backend->detach_kernel_driver)
		return usbi_backend->detach_kernel_driver(dev, interface_number);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

/** \ingroup dev
 * Re-attach an interface's kernel driver, which was previously detached
 * using libusb_detach_kernel_driver(). This call is only effective on
 * Linux and returns LIBUSB_ERROR_NOT_SUPPORTED on all other platforms.
 *
 * This functionality is not available on Darwin or Windows.
 *
 * \param dev a device handle
 * \param interface_number the interface to attach the driver from
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if no kernel driver was active
 * \returns LIBUSB_ERROR_INVALID_PARAM if the interface does not exist
 * \returns LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
 * \returns LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality
 * is not available
 * \returns LIBUSB_ERROR_BUSY if the driver cannot be attached because the
 * interface is claimed by a program or driver
 * \returns another LIBUSB_ERROR code on other failure
 * \see libusb_kernel_driver_active()
 */
int API_EXPORTED libusb_attach_kernel_driver(libusb_device_handle *dev,
	int interface_number)
{
	usbi_dbg("interface %d", interface_number);

	if (!dev->dev->attached)
		return LIBUSB_ERROR_NO_DEVICE;

	if (usbi_backend->attach_kernel_driver)
		return usbi_backend->attach_kernel_driver(dev, interface_number);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

/** \ingroup dev
 * Enable/disable libusbx's automatic kernel driver detachment. When this is
 * enabled libusbx will automatically detach the kernel driver on an interface
 * when claiming the interface, and attach it when releasing the interface.
 *
 * Automatic kernel driver detachment is disabled on newly opened device
 * handles by default.
 *
 * On platforms which do not have LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER
 * this function will return LIBUSB_ERROR_NOT_SUPPORTED, and libusbx will
 * continue as if this function was never called.
 *
 * \param dev a device handle
 * \param enable whether to enable or disable auto kernel driver detachment
 *
 * \returns LIBUSB_SUCCESS on success
 * \returns LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality
 * is not available
 * \see libusb_claim_interface()
 * \see libusb_release_interface()
 * \see libusb_set_configuration()
 */
int API_EXPORTED libusb_set_auto_detach_kernel_driver(
	libusb_device_handle *dev, int enable)
{
	if (!(usbi_backend->caps & USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER))
		return LIBUSB_ERROR_NOT_SUPPORTED;

	dev->auto_detach_kernel_driver = enable;
	return LIBUSB_SUCCESS;
}

/** \ingroup lib
 * Set log message verbosity.
 *
 * The default level is LIBUSB_LOG_LEVEL_NONE, which means no messages are ever
 * printed. If you choose to increase the message verbosity level, ensure
 * that your application does not close the stdout/stderr file descriptors.
 *
 * You are advised to use level LIBUSB_LOG_LEVEL_WARNING. libusbx is conservative
 * with its message logging and most of the time, will only log messages that
 * explain error conditions and other oddities. This will help you debug
 * your software.
 *
 * If the LIBUSB_DEBUG environment variable was set when libusbx was
 * initialized, this function does nothing: the message verbosity is fixed
 * to the value in the environment variable.
 *
 * If libusbx was compiled without any message logging, this function does
 * nothing: you'll never get any messages.
 *
 * If libusbx was compiled with verbose debug message logging, this function
 * does nothing: you'll always get messages from all levels.
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param level debug level to set
 */
void API_EXPORTED libusb_set_debug(libusb_context *ctx, int level)
{
	USBI_GET_CONTEXT(ctx);
	if (!ctx->debug_fixed)
		ctx->debug = level;
}

/** \ingroup lib
 * Initialize libusb. This function must be called before calling any other
 * libusbx function.
 *
 * If you do not provide an output location for a context pointer, a default
 * context will be created. If there was already a default context, it will
 * be reused (and nothing will be initialized/reinitialized).
 *
 * \param context Optional output location for context pointer.
 * Only valid on return code 0.
 * \returns 0 on success, or a LIBUSB_ERROR code on failure
 * \see contexts
 */
int API_EXPORTED libusb_init(libusb_context **context)
{
	struct libusb_device *dev, *next;
	char *dbg = getenv("LIBUSB_DEBUG");
	struct libusb_context *ctx;
	static int first_init = 1;
	int r = 0;

	usbi_mutex_static_lock(&default_context_lock);

	if (!timestamp_origin.tv_sec) {
		usbi_gettimeofday(&timestamp_origin, NULL);
	}

	if (!context && usbi_default_context) {
		usbi_dbg("reusing default context");
		default_context_refcnt++;
		usbi_mutex_static_unlock(&default_context_lock);
		return 0;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		r = LIBUSB_ERROR_NO_MEM;
		goto err_unlock;
	}

#ifdef ENABLE_DEBUG_LOGGING
	ctx->debug = LIBUSB_LOG_LEVEL_DEBUG;
#endif

	if (dbg) {
		ctx->debug = atoi(dbg);
		if (ctx->debug)
			ctx->debug_fixed = 1;
	}

	/* default context should be initialized before calling usbi_dbg */
	if (!usbi_default_context) {
		usbi_default_context = ctx;
		default_context_refcnt++;
		usbi_dbg("created default context");
	}

	usbi_dbg("libusbx v%d.%d.%d.%d", libusb_version_internal.major, libusb_version_internal.minor,
		libusb_version_internal.micro, libusb_version_internal.nano);

	usbi_mutex_init(&ctx->usb_devs_lock, NULL);
	usbi_mutex_init(&ctx->open_devs_lock, NULL);
	usbi_mutex_init(&ctx->hotplug_cbs_lock, NULL);
	list_init(&ctx->usb_devs);
	list_init(&ctx->open_devs);
	list_init(&ctx->hotplug_cbs);

	usbi_mutex_static_lock(&active_contexts_lock);
	if (first_init) {
		first_init = 0;
		list_init (&active_contexts_list);
	}
	list_add (&ctx->list, &active_contexts_list);
	usbi_mutex_static_unlock(&active_contexts_lock);

	if (usbi_backend->init) {
		r = usbi_backend->init(ctx);
		if (r)
			goto err_free_ctx;
	}

	r = usbi_io_init(ctx);
	if (r < 0)
		goto err_backend_exit;

	usbi_mutex_static_unlock(&default_context_lock);

	if (context)
		*context = ctx;

	return 0;

err_backend_exit:
	if (usbi_backend->exit)
		usbi_backend->exit();
err_free_ctx:
	if (ctx == usbi_default_context)
		usbi_default_context = NULL;

	usbi_mutex_static_lock(&active_contexts_lock);
	list_del (&ctx->list);
	usbi_mutex_static_unlock(&active_contexts_lock);

	usbi_mutex_lock(&ctx->usb_devs_lock);
	list_for_each_entry_safe(dev, next, &ctx->usb_devs, list, struct libusb_device) {
		list_del(&dev->list);
		libusb_unref_device(dev);
	}
	usbi_mutex_unlock(&ctx->usb_devs_lock);

	usbi_mutex_destroy(&ctx->open_devs_lock);
	usbi_mutex_destroy(&ctx->usb_devs_lock);
	usbi_mutex_destroy(&ctx->hotplug_cbs_lock);

	free(ctx);
err_unlock:
	usbi_mutex_static_unlock(&default_context_lock);
	return r;
}

/** \ingroup lib
 * Deinitialize libusb. Should be called after closing all open devices and
 * before your application terminates.
 * \param ctx the context to deinitialize, or NULL for the default context
 */
void API_EXPORTED libusb_exit(struct libusb_context *ctx)
{
	struct libusb_device *dev, *next;

	usbi_dbg("");
	USBI_GET_CONTEXT(ctx);

	/* if working with default context, only actually do the deinitialization
	 * if we're the last user */
	usbi_mutex_static_lock(&default_context_lock);
	if (ctx == usbi_default_context) {
		if (--default_context_refcnt > 0) {
			usbi_dbg("not destroying default context");
			usbi_mutex_static_unlock(&default_context_lock);
			return;
		}
		usbi_dbg("destroying default context");
		usbi_default_context = NULL;
	}
	usbi_mutex_static_unlock(&default_context_lock);

	usbi_mutex_static_lock(&active_contexts_lock);
	list_del (&ctx->list);
	usbi_mutex_static_unlock(&active_contexts_lock);

	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		usbi_hotplug_deregister_all(ctx);
		usbi_mutex_lock(&ctx->usb_devs_lock);
		list_for_each_entry_safe(dev, next, &ctx->usb_devs, list, struct libusb_device) {
			list_del(&dev->list);
			libusb_unref_device(dev);
		}
		usbi_mutex_unlock(&ctx->usb_devs_lock);
	}

	/* a few sanity checks. don't bother with locking because unless
	 * there is an application bug, nobody will be accessing these. */
	if (!list_empty(&ctx->usb_devs))
		usbi_warn(ctx, "some libusb_devices were leaked");
	if (!list_empty(&ctx->open_devs))
		usbi_warn(ctx, "application left some devices open");

	usbi_io_exit(ctx);
	if (usbi_backend->exit)
		usbi_backend->exit();

	usbi_mutex_destroy(&ctx->open_devs_lock);
	usbi_mutex_destroy(&ctx->usb_devs_lock);
	usbi_mutex_destroy(&ctx->hotplug_cbs_lock);
	free(ctx);
}

/** \ingroup misc
 * Check at runtime if the loaded library has a given capability.
 * This call should be performed after \ref libusb_init(), to ensure the
 * backend has updated its capability set.
 *
 * \param capability the \ref libusb_capability to check for
 * \returns nonzero if the running library has the capability, 0 otherwise
 */
int API_EXPORTED libusb_has_capability(uint32_t capability)
{
	switch (capability) {
	case LIBUSB_CAP_HAS_CAPABILITY:
		return 1;
	case LIBUSB_CAP_HAS_HOTPLUG:
		return !(usbi_backend->get_device_list);
	case LIBUSB_CAP_HAS_HID_ACCESS:
		return (usbi_backend->caps & USBI_CAP_HAS_HID_ACCESS);
	case LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER:
		return (usbi_backend->caps & USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER);
	}
	return 0;
}

/* this is defined in libusbi.h if needed */
#ifdef LIBUSB_GETTIMEOFDAY_WIN32
/*
 * gettimeofday
 * Implementation according to:
 * The Open Group Base Specifications Issue 6
 * IEEE Std 1003.1, 2004 Edition
 */

/*
 *  THIS SOFTWARE IS NOT COPYRIGHTED
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAIMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  Contributed by:
 *  Danny Smith <dannysmith@users.sourceforge.net>
 */

/* Offset between 1/1/1601 and 1/1/1970 in 100 nanosec units */
#define _W32_FT_OFFSET (116444736000000000)

int usbi_gettimeofday(struct timeval *tp, void *tzp)
{
	union {
		unsigned __int64 ns100; /* Time since 1 Jan 1601, in 100ns units */
		FILETIME ft;
	} _now;
	UNUSED(tzp);

	if(tp) {
#if defined(OS_WINCE)
		SYSTEMTIME st;
		GetSystemTime(&st);
		SystemTimeToFileTime(&st, &_now.ft);
#else
		GetSystemTimeAsFileTime (&_now.ft);
#endif
		tp->tv_usec=(long)((_now.ns100 / 10) % 1000000 );
		tp->tv_sec= (long)((_now.ns100 - _W32_FT_OFFSET) / 10000000);
	}
	/* Always return 0 as per Open Group Base Specifications Issue 6.
	   Do not set errno on error.  */
	return 0;
}
#endif

static void usbi_log_str(struct libusb_context *ctx,
	enum libusb_log_level level, const char * str)
{
#if defined(USE_SYSTEM_LOGGING_FACILITY)
#if defined(OS_WINDOWS) || defined(OS_WINCE)
	/* Windows CE only supports the Unicode version of OutputDebugString. */
	WCHAR wbuf[USBI_MAX_LOG_LEN];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wbuf, sizeof(wbuf));
	OutputDebugStringW(wbuf);
#elif defined(__ANDROID__)
	int priority = ANDROID_LOG_UNKNOWN;
	switch (level) {
	case LIBUSB_LOG_LEVEL_INFO: priority = ANDROID_LOG_INFO; break;
	case LIBUSB_LOG_LEVEL_WARNING: priority = ANDROID_LOG_WARN; break;
	case LIBUSB_LOG_LEVEL_ERROR: priority = ANDROID_LOG_ERROR; break;
	case LIBUSB_LOG_LEVEL_DEBUG: priority = ANDROID_LOG_DEBUG; break;
	}
	__android_log_write(priority, "libusb", str);
#elif defined(HAVE_SYSLOG_FUNC)
	int syslog_level = LOG_INFO;
	switch (level) {
	case LIBUSB_LOG_LEVEL_INFO: syslog_level = LOG_INFO; break;
	case LIBUSB_LOG_LEVEL_WARNING: syslog_level = LOG_WARNING; break;
	case LIBUSB_LOG_LEVEL_ERROR: syslog_level = LOG_ERR; break;
	case LIBUSB_LOG_LEVEL_DEBUG: syslog_level = LOG_DEBUG; break;
	}
	syslog(syslog_level, "%s", str);
#else /* All of gcc, Clang, XCode seem to use #warning */
#warning System logging is not supported on this platform. Logging to stderr will be used instead.
	fputs(str, stderr);
#endif
#else
	fputs(str, stderr);
#endif /* USE_SYSTEM_LOGGING_FACILITY */
	UNUSED(ctx);
	UNUSED(level);
}

void usbi_log_v(struct libusb_context *ctx, enum libusb_log_level level,
	const char *function, const char *format, va_list args)
{
	const char *prefix = "";
	char buf[USBI_MAX_LOG_LEN];
	struct timeval now;
	int global_debug, header_len, text_len;
	static int has_debug_header_been_displayed = 0;

#ifdef ENABLE_DEBUG_LOGGING
	global_debug = 1;
	UNUSED(ctx);
#else
	USBI_GET_CONTEXT(ctx);
	if (ctx == NULL)
		return;
	global_debug = (ctx->debug == LIBUSB_LOG_LEVEL_DEBUG);
	if (!ctx->debug)
		return;
	if (level == LIBUSB_LOG_LEVEL_WARNING && ctx->debug < LIBUSB_LOG_LEVEL_WARNING)
		return;
	if (level == LIBUSB_LOG_LEVEL_INFO && ctx->debug < LIBUSB_LOG_LEVEL_INFO)
		return;
	if (level == LIBUSB_LOG_LEVEL_DEBUG && ctx->debug < LIBUSB_LOG_LEVEL_DEBUG)
		return;
#endif

	usbi_gettimeofday(&now, NULL);
	if ((global_debug) && (!has_debug_header_been_displayed)) {
		has_debug_header_been_displayed = 1;
		usbi_log_str(ctx, LIBUSB_LOG_LEVEL_DEBUG, "[timestamp] [threadID] facility level [function call] <message>\n");
		usbi_log_str(ctx, LIBUSB_LOG_LEVEL_DEBUG, "--------------------------------------------------------------------------------\n");
	}
	if (now.tv_usec < timestamp_origin.tv_usec) {
		now.tv_sec--;
		now.tv_usec += 1000000;
	}
	now.tv_sec -= timestamp_origin.tv_sec;
	now.tv_usec -= timestamp_origin.tv_usec;

	switch (level) {
	case LIBUSB_LOG_LEVEL_INFO:
		prefix = "info";
		break;
	case LIBUSB_LOG_LEVEL_WARNING:
		prefix = "warning";
		break;
	case LIBUSB_LOG_LEVEL_ERROR:
		prefix = "error";
		break;
	case LIBUSB_LOG_LEVEL_DEBUG:
		prefix = "debug";
		break;
	case LIBUSB_LOG_LEVEL_NONE:
		return;
	default:
		prefix = "unknown";
		break;
	}

	if (global_debug) {
		header_len = snprintf(buf, sizeof(buf),
			"[%2d.%06d] [%08x] libusbx: %s [%s] ",
			(int)now.tv_sec, (int)now.tv_usec, usbi_get_tid(), prefix, function);
	} else {
		header_len = snprintf(buf, sizeof(buf),
			"libusbx: %s [%s] ", prefix, function);
	}

	if (header_len < 0 || header_len >= sizeof(buf)) {
		/* Somehow snprintf failed to write to the buffer,
		 * remove the header so something useful is output. */
		header_len = 0;
	}
	/* Make sure buffer is NUL terminated */
	buf[header_len] = '\0';
	text_len = vsnprintf(buf + header_len, sizeof(buf) - header_len,
		format, args);
	if (text_len < 0 || text_len + header_len >= sizeof(buf)) {
		/* Truncated log output. On some platforms a -1 return value means
		 * that the output was truncated. */
		text_len = sizeof(buf) - header_len;
	}
	if (header_len + text_len + sizeof(USBI_LOG_LINE_END) >= sizeof(buf)) {
		/* Need to truncate the text slightly to fit on the terminator. */
		text_len -= (header_len + text_len + sizeof(USBI_LOG_LINE_END)) - sizeof(buf);
	}
	strcpy(buf + header_len + text_len, USBI_LOG_LINE_END);

	usbi_log_str(ctx, level, buf);
}

void usbi_log(struct libusb_context *ctx, enum libusb_log_level level,
	const char *function, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	usbi_log_v(ctx, level, function, format, args);
	va_end (args);
}

/** \ingroup misc
 * Returns a constant NULL-terminated string with the ASCII name of a libusbx
 * error or transfer status code. The caller must not free() the returned
 * string.
 *
 * \param error_code The \ref libusb_error or libusb_transfer_status code to
 * return the name of.
 * \returns The error name, or the string **UNKNOWN** if the value of
 * error_code is not a known error / status code.
 */
DEFAULT_VISIBILITY const char * LIBUSB_CALL libusb_error_name(int error_code)
{
	switch (error_code) {
	case LIBUSB_ERROR_IO:
		return "LIBUSB_ERROR_IO";
	case LIBUSB_ERROR_INVALID_PARAM:
		return "LIBUSB_ERROR_INVALID_PARAM";
	case LIBUSB_ERROR_ACCESS:
		return "LIBUSB_ERROR_ACCESS";
	case LIBUSB_ERROR_NO_DEVICE:
		return "LIBUSB_ERROR_NO_DEVICE";
	case LIBUSB_ERROR_NOT_FOUND:
		return "LIBUSB_ERROR_NOT_FOUND";
	case LIBUSB_ERROR_BUSY:
		return "LIBUSB_ERROR_BUSY";
	case LIBUSB_ERROR_TIMEOUT:
		return "LIBUSB_ERROR_TIMEOUT";
	case LIBUSB_ERROR_OVERFLOW:
		return "LIBUSB_ERROR_OVERFLOW";
	case LIBUSB_ERROR_PIPE:
		return "LIBUSB_ERROR_PIPE";
	case LIBUSB_ERROR_INTERRUPTED:
		return "LIBUSB_ERROR_INTERRUPTED";
	case LIBUSB_ERROR_NO_MEM:
		return "LIBUSB_ERROR_NO_MEM";
	case LIBUSB_ERROR_NOT_SUPPORTED:
		return "LIBUSB_ERROR_NOT_SUPPORTED";
	case LIBUSB_ERROR_OTHER:
		return "LIBUSB_ERROR_OTHER";

	case LIBUSB_TRANSFER_ERROR:
		return "LIBUSB_TRANSFER_ERROR";
	case LIBUSB_TRANSFER_TIMED_OUT:
		return "LIBUSB_TRANSFER_TIMED_OUT";
	case LIBUSB_TRANSFER_CANCELLED:
		return "LIBUSB_TRANSFER_CANCELLED";
	case LIBUSB_TRANSFER_STALL:
		return "LIBUSB_TRANSFER_STALL";
	case LIBUSB_TRANSFER_NO_DEVICE:
		return "LIBUSB_TRANSFER_NO_DEVICE";
	case LIBUSB_TRANSFER_OVERFLOW:
		return "LIBUSB_TRANSFER_OVERFLOW";

	case 0:
		return "LIBUSB_SUCCESS / LIBUSB_TRANSFER_COMPLETED";
	default:
		return "**UNKNOWN**";
	}
}

/** \ingroup misc
 * Returns a pointer to const struct libusb_version with the version
 * (major, minor, micro, nano and rc) of the running library.
 */
DEFAULT_VISIBILITY
const struct libusb_version * LIBUSB_CALL libusb_get_version(void)
{
	return &libusb_version_internal;
}
