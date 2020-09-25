// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a subset of wayland's connection.c. It's necessary to
// provide implementations of variadic functions which generate_stubs.py cannot
// generate forwarding functions for. Fortunately, libwayland-client exposes
// non-variadic equivalents for these functions that we can forward to.

#include <wayland-client.h>
#include <wayland-server.h>

#include <cstdarg>
#include <cstdint>

#define WL_CLOSURE_MAX_ARGS 20

extern "C" {

struct argument_details {
  char type;
  int nullable;
};

const char* get_next_argument(const char* signature,
                              struct argument_details* details) {
  details->nullable = 0;
  for (; *signature; ++signature) {
    switch (*signature) {
      case 'i':
      case 'u':
      case 'f':
      case 's':
      case 'o':
      case 'n':
      case 'a':
      case 'h':
        details->type = *signature;
        return signature + 1;
      case '?':
        details->nullable = 1;
    }
  }
  details->type = '\0';
  return signature;
}

void wl_argument_from_va_list(const char* signature,
                              union wl_argument* args,
                              int count,
                              va_list ap) {
  int i;
  const char* sig_iter;
  struct argument_details arg;

  sig_iter = signature;
  for (i = 0; i < count; i++) {
    sig_iter = get_next_argument(sig_iter, &arg);

    switch (arg.type) {
      case 'i':
        args[i].i = va_arg(ap, int32_t);
        break;
      case 'u':
        args[i].u = va_arg(ap, uint32_t);
        break;
      case 'f':
        args[i].f = va_arg(ap, wl_fixed_t);
        break;
      case 's':
        args[i].s = va_arg(ap, const char*);
        break;
      case 'o':
        args[i].o = va_arg(ap, struct wl_object*);
        break;
      case 'n':
        args[i].o = va_arg(ap, struct wl_object*);
        break;
      case 'a':
        args[i].a = va_arg(ap, struct wl_array*);
        break;
      case 'h':
        args[i].h = va_arg(ap, int32_t);
        break;
      case '\0':
        return;
    }
  }
}

void wl_proxy_marshal(struct wl_proxy* proxy, uint32_t opcode, ...) {
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;

  va_start(ap, opcode);
  wl_argument_from_va_list(
      reinterpret_cast<wl_object*>(proxy)->interface->methods[opcode].signature,
      args, WL_CLOSURE_MAX_ARGS, ap);
  va_end(ap);

  wl_proxy_marshal_array_constructor(proxy, opcode, args, nullptr);
}

struct wl_proxy* wl_proxy_marshal_constructor(
    struct wl_proxy* proxy,
    uint32_t opcode,
    const struct wl_interface* interface,
    ...) {
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;

  va_start(ap, interface);
  wl_argument_from_va_list(
      reinterpret_cast<wl_object*>(proxy)->interface->methods[opcode].signature,
      args, WL_CLOSURE_MAX_ARGS, ap);
  va_end(ap);

  return wl_proxy_marshal_array_constructor(proxy, opcode, args, interface);
}

struct wl_proxy* wl_proxy_marshal_constructor_versioned(
    struct wl_proxy* proxy,
    uint32_t opcode,
    const struct wl_interface* interface,
    uint32_t version,
    ...) {
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;

  va_start(ap, version);
  wl_argument_from_va_list(
      reinterpret_cast<wl_object*>(proxy)->interface->methods[opcode].signature,
      args, WL_CLOSURE_MAX_ARGS, ap);
  va_end(ap);

  return wl_proxy_marshal_array_constructor_versioned(proxy, opcode, args,
                                                      interface, version);
}
}
