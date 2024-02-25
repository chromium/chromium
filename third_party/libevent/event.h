/*
 * Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT_H_
#define _EVENT_H_

/** @mainpage

  @section intro Introduction

  libevent is an event notification library for developing scalable network
  servers.  The libevent API provides a mechanism to execute a callback
  function when a specific event occurs on a file descriptor or after a
  timeout has been reached. Furthermore, libevent also support callbacks due
  to signals or regular timeouts.

  libevent is meant to replace the event loop found in event driven network
  servers. An application just needs to call event_dispatch() and then add or
  remove events dynamically without having to change the event loop.

  Currently, libevent supports /dev/poll, kqueue(2), select(2), poll(2) and
  epoll(4). It also has experimental support for real-time signals. The
  internal event mechanism is completely independent of the exposed event API,
  and a simple update of libevent can provide new functionality without having
  to redesign the applications. As a result, Libevent allows for portable
  application development and provides the most scalable event notification
  mechanism available on an operating system. Libevent can also be used for
  multi-threaded aplications; see Steven Grimm's explanation. Libevent should
  compile on Linux, *BSD, Mac OS X, Solaris and Windows.

  @section usage Standard usage

  Every program that uses libevent must include the <event.h> header, and pass
  the -levent flag to the linker.  Before using any of the functions in the
  library, you must call event_init() or event_base_new() to perform one-time
  initialization of the libevent library.

  @section event Event notification

  For each file descriptor that you wish to monitor, you must declare an event
  structure and call event_set() to initialize the members of the structure.
  To enable notification, you add the structure to the list of monitored
  events by calling event_add().  The event structure must remain allocated as
  long as it is active, so it should be allocated on the heap. Finally, you
  call event_dispatch() to loop and dispatch events.

  @section bufferevent I/O Buffers

  libevent provides an abstraction on top of the regular event callbacks. This
  abstraction is called a buffered event. A buffered event provides input and
  output buffers that get filled and drained automatically. The user of a
  buffered event no longer deals directly with the I/O, but instead is reading
  from input and writing to output buffers.

  Once initialized via bufferevent_new(), the bufferevent structure can be
  used repeatedly with bufferevent_enable() and bufferevent_disable().
  Instead of reading and writing directly to a socket, you would call
  bufferevent_read() and bufferevent_write().

  When read enabled the bufferevent will try to read from the file descriptor
  and call the read callback. The write callback is executed whenever the
  output buffer is drained below the write low watermark, which is 0 by
  default.

  @section timers Timers

  libevent can also be used to create timers that invoke a callback after a
  certain amount of time has expired. The evtimer_set() function prepares an
  event struct to be used as a timer. To activate the timer, call
  evtimer_add(). Timers can be deactivated by calling evtimer_del().

  @section timeouts Timeouts

  In addition to simple timers, libevent can assign timeout events to file
  descriptors that are triggered whenever a certain amount of time has passed
  with no activity on a file descriptor.  The timeout_set() function
  initializes an event struct for use as a timeout. Once initialized, the
  event must be activated by using timeout_add().  To cancel the timeout, call
  timeout_del().

  @section evdns Asynchronous DNS resolution

  libevent provides an asynchronous DNS resolver that should be used instead
  of the standard DNS resolver functions.  These functions can be imported by
  including the <evdns.h> header in your program. Before using any of the
  resolver functions, you must call evdns_init() to initialize the library. To
  convert a hostname to an IP address, you call the evdns_resolve_ipv4()
  function.  To perform a reverse lookup, you would call the
  evdns_resolve_reverse() function.  All of these functions use callbacks to
  avoid blocking while the lookup is performed.

  @section evhttp Event-driven HTTP servers

  libevent provides a very simple event-driven HTTP server that can be
  embedded in your program and used to service HTTP requests.

  To use this capability, you need to include the <evhttp.h> header in your
  program.  You create the server by calling evhttp_new(). Add addresses and
  ports to listen on with evhttp_bind_socket(). You then register one or more
  callbacks to handle incoming requests.  Each URI can be assigned a callback
  via the evhttp_set_cb() function.  A generic callback function can also be
  registered via evhttp_set_gencb(); this callback will be invoked if no other
  callbacks have been registered for a given URI.

  @section evrpc A framework for RPC servers and clients
 
  libevents provides a framework for creating RPC servers and clients.  It
  takes care of marshaling and unmarshaling all data structures.

  @section api API Reference

  To browse the complete documentation of the libevent API, click on any of
  the following links.

  event.h
  The primary libevent header

  evdns.h
  Asynchronous DNS resolution

  evhttp.h
  An embedded libevent-based HTTP server

  evrpc.h
  A framework for creating RPC servers and clients

 */

/** @file event.h

  A library for writing event-driven network servers

 */

#ifdef __cplusplus
extern "C" {
#endif

#include "event-config.h"
#ifdef _EVENT_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef _EVENT_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef _EVENT_HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdarg.h>

/* For int types. */
#include "evutil.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
typedef unsigned char u_char;
typedef unsigned short u_short;
#endif

#define EVLIST_TIMEOUT	0x01
#define EVLIST_INSERTED	0x02
#define EVLIST_SIGNAL	0x04
#define EVLIST_ACTIVE	0x08
#define EVLIST_INTERNAL	0x10
#define EVLIST_INIT	0x80

/* EVLIST_X_ Private space: 0x1000-0xf000 */
#define EVLIST_ALL	(0xf000 | 0x9f)

#define EV_TIMEOUT	0x01
#define EV_READ		0x02
#define EV_WRITE	0x04
#define EV_SIGNAL	0x08
#define EV_PERSIST	0x10	/* Persistant event */

/* Fix so that ppl dont have to run with <sys/queue.h> */
#ifndef TAILQ_ENTRY
#define _EVENT_DEFINED_TQENTRY
#define TAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;	/* next element */			\
	struct type **tqe_prev;	/* address of previous next element */	\
}
#endif /* !TAILQ_ENTRY */

struct event_base;
#ifndef EVENT_NO_STRUCT
struct event {
	TAILQ_ENTRY (event) ev_next;
	TAILQ_ENTRY (event) ev_active_next;
	TAILQ_ENTRY (event) ev_signal_next;
	unsigned int min_heap_idx;	/* for managing timeouts */

	struct event_base *ev_base;

	int ev_fd;
	short ev_events;
	short ev_ncalls;
	short *ev_pncalls;	/* Allows deletes in callback */

	struct timeval ev_timeout;

	int ev_pri;		/* smaller numbers are higher priority */

	void (*ev_callback)(int, short, void *arg);
	void *ev_arg;

	int ev_res;		/* result passed to event callback */
	int ev_flags;
};
#else
struct event;
#endif

#define EVENT_SIGNAL(ev)	(int)(ev)->ev_fd
#define EVENT_FD(ev)		(int)(ev)->ev_fd

/*
 * Key-Value pairs.  Can be used for HTTP headers but also for
 * query argument parsing.
 */
struct evkeyval {
	TAILQ_ENTRY(evkeyval) next;

	char *key;
	char *value;
};

#ifdef _EVENT_DEFINED_TQENTRY
#undef TAILQ_ENTRY
struct event_list;
struct evkeyvalq;
#undef _EVENT_DEFINED_TQENTRY
#else
TAILQ_HEAD (event_list, event);
TAILQ_HEAD (evkeyvalq, evkeyval);
#endif /* _EVENT_DEFINED_TQENTRY */

/**
  Initialize the event API.

  Use event_base_new() to initialize a new event base, but does not set
  the current_base global.   If using only event_base_new(), each event
  added must have an event base set with event_base_set()

  @see event_base_set(), event_base_free(), event_init()
 */
struct event_base *event_base_new(void);

/**
  Initialize the event API.

  The event API needs to be initialized with event_init() before it can be
  used.  Sets the current_base global representing the default base for
  events that have no base associated with them.

  @see event_base_set(), event_base_new()
 */
struct event_base *event_init(void);

/**
  Reinitialized the event base after a fork

  Some event mechanisms do not survive across fork.   The event base needs
  to be reinitialized with the event_reinit() function.

  @param base the event base that needs to be re-initialized
  @return 0 if successful, or -1 if some events could not be re-added.
  @see event_base_new(), event_init()
*/
int event_reinit(struct event_base *base);

/**
  Loop to process events.

  In order to process events, an application needs to call
  event_dispatch().  This function only returns on error, and should
  replace the event core of the application program.

  @see event_base_dispatch()
 */
int event_dispatch(void);


/**
  Threadsafe event dispatching loop.

  @param eb the event_base structure returned by event_init()
  @see event_init(), event_dispatch()
 */
int event_base_dispatch(struct event_base *);


/**
 Get the kernel event notification mechanism used by libevent.
 
 @param eb the event_base structure returned by event_base_new()
 @return a string identifying the kernel event mechanism (kqueue, epoll, etc.)
 */
const char *event_base_get_method(struct event_base *);
        
        
/**
  Deallocate all memory associated with an event_base, and free the base.

  Note that this function will not close any fds or free any memory passed
  to event_set as the argument to callback.

  @param eb an event_base to be freed
 */
void event_base_free(struct event_base *);


#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG   1
#define _EVENT_LOG_WARN  2
#define _EVENT_LOG_ERR   3
typedef void (*event_log_cb)(int severity, const char *msg);
/**
  Redirect libevent's log messages.

  @param cb a function taking two arguments: an integer severity between
     _EVENT_LOG_DEBUG and _EVENT_LOG_ERR, and a string.  If cb is NULL,
	 then the default log is used.
  */
void event_set_log_callback(event_log_cb cb);

/**
  Associate a different event base with an event.

  @param eb the event base
  @param ev the event
 */
int event_base_set(struct event_base *, struct event *);

/**
 event_loop() flags
 */
/*@{*/
#define EVLOOP_ONCE	0x01	/**< Block at most once. */
#define EVLOOP_NONBLOCK	0x02	/**< Do not block. */
/*@}*/

/**
  Handle events.

  This is a more flexible version of event_dispatch().

  @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
  @return 0 if successful, -1 if an error occurred, or 1 if no events were
    registered.
  @see event_loopexit(), event_base_loop()
*/
int event_loop(int);

/**
  Handle events (threadsafe version).

  This is a more flexible version of event_base_dispatch().

  @param eb the event_base structure returned by event_init()
  @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
  @return 0 if successful, -1 if an error occurred, or 1 if no events were
    registered.
  @see event_loopexit(), event_base_loop()
  */
int event_base_loop(struct event_base *, int);

/**
  Exit the event loop after the specified time.

  The next event_loop() iteration after the given timer expires will
  complete normally (handling all queued events) then exit without
  blocking for events again.

  Subsequent invocations of event_loop() will proceed normally.

  @param tv the amount of time after which the loop should terminate.
  @return 0 if successful, or -1 if an error occurred
  @see event_loop(), event_base_loop(), event_base_loopexit()
  */
int event_loopexit(const struct timeval *);


/**
  Exit the event loop after the specified time (threadsafe variant).

  The next event_base_loop() iteration after the given timer expires will
  complete normally (handling all queued events) then exit without
  blocking for events again.

  Subsequent invocations of event_base_loop() will proceed normally.

  @param eb the event_base structure returned by event_init()
  @param tv the amount of time after which the loop should terminate.
  @return 0 if successful, or -1 if an error occurred
  @see event_loopexit()
 */
int event_base_loopexit(struct event_base *, const struct timeval *);

/**
  Abort the active event_loop() immediately.

  event_loop() will abort the loop after the next event is completed;
  event_loopbreak() is typically invoked from this event's callback.
  This behavior is analogous to the "break;" statement.

  Subsequent invocations of event_loop() will proceed normally.

  @return 0 if successful, or -1 if an error occurred
  @see event_base_loopbreak(), event_loopexit()
 */
int event_loopbreak(void);

/**
  Abort the active event_base_loop() immediately.

  event_base_loop() will abort the loop after the next event is completed;
  event_base_loopbreak() is typically invoked from this event's callback.
  This behavior is analogous to the "break;" statement.

  Subsequent invocations of event_loop() will proceed normally.

  @param eb the event_base structure returned by event_init()
  @return 0 if successful, or -1 if an error occurred
  @see event_base_loopexit
 */
int event_base_loopbreak(struct event_base *);


/**
  Add a timer event.

  @param ev the event struct
  @param tv timeval struct
 */
#define evtimer_add(ev, tv)		event_add(ev, tv)


/**
  Define a timer event.

  @param ev event struct to be modified
  @param cb callback function
  @param arg argument that will be passed to the callback function
 */
#define evtimer_set(ev, cb, arg)	event_set(ev, -1, 0, cb, arg)


/**
 * Delete a timer event.
 *
 * @param ev the event struct to be disabled
 */
#define evtimer_del(ev)			event_del(ev)
#define evtimer_pending(ev, tv)		event_pending(ev, EV_TIMEOUT, tv)
#define evtimer_initialized(ev)		((ev)->ev_flags & EVLIST_INIT)

/**
 * Add a timeout event.
 *
 * @param ev the event struct to be disabled
 * @param tv the timeout value, in seconds
 */
#define timeout_add(ev, tv)		event_add(ev, tv)


/**
 * Define a timeout event.
 *
 * @param ev the event struct to be defined
 * @param cb the callback to be invoked when the timeout expires
 * @param arg the argument to be passed to the callback
 */
#define timeout_set(ev, cb, arg)	event_set(ev, -1, 0, cb, arg)


/**
 * Disable a timeout event.
 *
 * @param ev the timeout event to be disabled
 */
#define timeout_del(ev)			event_del(ev)

#define timeout_pending(ev, tv)		event_pending(ev, EV_TIMEOUT, tv)
#define timeout_initialized(ev)		((ev)->ev_flags & EVLIST_INIT)

#define signal_add(ev, tv)		event_add(ev, tv)
#define signal_set(ev, x, cb, arg)	\
	event_set(ev, x, EV_SIGNAL|EV_PERSIST, cb, arg)
#define signal_del(ev)			event_del(ev)
#define signal_pending(ev, tv)		event_pending(ev, EV_SIGNAL, tv)
#define signal_initialized(ev)		((ev)->ev_flags & EVLIST_INIT)

/**
  Prepare an event structure to be added.

  The function event_set() prepares the event structure ev to be used in
  future calls to event_add() and event_del().  The event will be prepared to
  call the function specified by the fn argument with an int argument
  indicating the file descriptor, a short argument indicating the type of
  event, and a void * argument given in the arg argument.  The fd indicates
  the file descriptor that should be monitored for events.  The events can be
  either EV_READ, EV_WRITE, or both.  Indicating that an application can read
  or write from the file descriptor respectively without blocking.

  The function fn will be called with the file descriptor that triggered the
  event and the type of event which will be either EV_TIMEOUT, EV_SIGNAL,
  EV_READ, or EV_WRITE.  The additional flag EV_PERSIST makes an event_add()
  persistent until event_del() has been called.

  @param ev an event struct to be modified
  @param fd the file descriptor to be monitored
  @param event desired events to monitor; can be EV_READ and/or EV_WRITE
  @param fn callback function to be invoked when the event occurs
  @param arg an argument to be passed to the callback function

  @see event_add(), event_del(), event_once()

 */
void event_set(struct event *, int, short, void (*)(int, short, void *), void *);

/**
  Schedule a one-time event to occur.

  The function event_once() is similar to event_set().  However, it schedules
  a callback to be called exactly once and does not require the caller to
  prepare an event structure.

  @param fd a file descriptor to monitor
  @param events event(s) to monitor; can be any of EV_TIMEOUT | EV_READ |
         EV_WRITE
  @param callback callback function to be invoked when the event occurs
  @param arg an argument to be passed to the callback function
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_set()

 */
int event_once(int, short, void (*)(int, short, void *), void *,
    const struct timeval *);


/**
  Schedule a one-time event (threadsafe variant)

  The function event_base_once() is similar to event_set().  However, it
  schedules a callback to be called exactly once and does not require the
  caller to prepare an event structure.

  @param base an event_base returned by event_init()
  @param fd a file descriptor to monitor
  @param events event(s) to monitor; can be any of EV_TIMEOUT | EV_READ |
         EV_WRITE
  @param callback callback function to be invoked when the event occurs
  @param arg an argument to be passed to the callback function
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_once()
 */
int event_base_once(struct event_base *base, int fd, short events,
    void (*callback)(int, short, void *), void *arg,
    const struct timeval *timeout);


/**
  Add an event to the set of monitored events.

  The function event_add() schedules the execution of the ev event when the
  event specified in event_set() occurs or in at least the time specified in
  the tv.  If tv is NULL, no timeout occurs and the function will only be
  called if a matching event occurs on the file descriptor.  The event in the
  ev argument must be already initialized by event_set() and may not be used
  in calls to event_set() until it has timed out or been removed with
  event_del().  If the event in the ev argument already has a scheduled
  timeout, the old timeout will be replaced by the new one.

  @param ev an event struct initialized via event_set()
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_del(), event_set()
  */
int event_add(struct event *ev, const struct timeval *timeout);


/**
  Remove an event from the set of monitored events.

  The function event_del() will cancel the event in the argument ev.  If the
  event has already executed or has never been added the call will have no
  effect.

  @param ev an event struct to be removed from the working set
  @return 0 if successful, or -1 if an error occurred
  @see event_add()
 */
int event_del(struct event *);

void event_active(struct event *, int, short);


/**
  Checks if a specific event is pending or scheduled.

  @param ev an event struct previously passed to event_add()
  @param event the requested event type; any of EV_TIMEOUT|EV_READ|
         EV_WRITE|EV_SIGNAL
  @param tv an alternate timeout (FIXME - is this true?)

  @return 1 if the event is pending, or 0 if the event has not occurred

 */
int event_pending(struct event *ev, short event, struct timeval *tv);


/**
  Test if an event structure has been initialized.

  The event_initialized() macro can be used to check if an event has been
  initialized.

  @param ev an event structure to be tested
  @return 1 if the structure has been initialized, or 0 if it has not been
          initialized
 */
#ifdef WIN32
#define event_initialized(ev)		((ev)->ev_flags & EVLIST_INIT && (ev)->ev_fd != (int)INVALID_HANDLE_VALUE)
#else
#define event_initialized(ev)		((ev)->ev_flags & EVLIST_INIT)
#endif


/**
  Get the libevent version number.

  @return a string containing the version number of libevent
 */
const char *event_get_version(void);


/**
  Get the kernel event notification mechanism used by libevent.

  @return a string identifying the kernel event mechanism (kqueue, epoll, etc.)
 */
const char *event_get_method(void);


/**
  Set the number of different event priorities.

  By default libevent schedules all active events with the same priority.
  However, some time it is desirable to process some events with a higher
  priority than others.  For that reason, libevent supports strict priority
  queues.  Active events with a lower priority are always processed before
  events with a higher priority.

  The number of different priorities can be set initially with the
  event_priority_init() function.  This function should be called before the
  first call to event_dispatch().  The event_priority_set() function can be
  used to assign a priority to an event.  By default, libevent assigns the
  middle priority to all events unless their priority is explicitly set.

  @param npriorities the maximum number of priorities
  @return 0 if successful, or -1 if an error occurred
  @see event_base_priority_init(), event_priority_set()

 */
int	event_priority_init(int);


/**
  Set the number of different event priorities (threadsafe variant).

  See the description of event_priority_init() for more information.

  @param eb the event_base structure returned by event_init()
  @param npriorities the maximum number of priorities
  @return 0 if successful, or -1 if an error occurred
  @see event_priority_init(), event_priority_set()
 */
int	event_base_priority_init(struct event_base *, int);


/**
  Assign a priority to an event.

  @param ev an event struct
  @param priority the new priority to be assigned
  @return 0 if successful, or -1 if an error occurred
  @see event_priority_init()
  */
int	event_priority_set(struct event *, int);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_H_ */
