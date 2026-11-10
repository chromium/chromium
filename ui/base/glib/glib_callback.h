// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_GLIB_CALLBACK_H_
#define UI_BASE_GLIB_GLIB_CALLBACK_H_

#include <glib.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/stack_allocated.h"

// Helpers for passing a `base::OnceCallback` to GLib/GTK C functions of the
// form:
//
//   gtk_do_something(arg1, arg2, callback, user_data, destroy);
//
// where `callback` is a C function pointer that receives `user_data` as one of
// its parameters, and `destroy` is a `GDestroyNotify` that GLib/GTK calls to
// free `user_data` once it no longer needs it.
//
// If the C callback has the signature
//
//   ret (*)(cb_arg1, cb_arg2, gpointer user_data, cb_arg3)
//
// then the `base::OnceCallback` must have the same signature with the
// `gpointer user_data` parameter removed, i.e. `ret(cb_arg1, cb_arg2,
// cb_arg3)`. `user_data` may appear at any position in the C callback's
// parameter list.
//
// The simplest form, when `callback`, `user_data` and `destroy` are the last
// three parameters of the GLib/GTK function, is:
//
//   ui::CallGlibWithCallback(&gtk_do_something, base::BindOnce(...), arg1,
//                            arg2);
//
// Otherwise, `ui::WrapGlibOnceCallback()` returns the triple directly:
//
//   auto [callback, user_data, destroy] =
//       ui::WrapGlibOnceCallback(base::BindOnce(...));
//   gtk_do_something(arg1, arg2, callback, user_data, destroy);
//
// The `base::OnceCallback` is run at most once, and is destroyed when GLib/GTK
// calls `destroy`, whether or not the callback was run.

namespace ui {

namespace internal {

// Returns the last index `I` in `CArgsTuple` where `CArgsTuple[I]` is
// `gpointer` and the remaining parameters match `ArgsTuple`, or
// `std::tuple_size_v<CArgsTuple>` if no such index exists.
template <typename ArgsTuple, typename CArgsTuple>
consteval size_t FindGlibUserDataIndex() {
  constexpr size_t kNumArgs = std::tuple_size_v<ArgsTuple>;
  constexpr size_t kNumCArgs = std::tuple_size_v<CArgsTuple>;
  if constexpr (kNumCArgs != kNumArgs + 1) {
    return kNumCArgs;
  } else {
    size_t match = kNumCArgs;
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      auto check = [&]<size_t I>() {
        if constexpr (std::is_same_v<std::tuple_element_t<I, CArgsTuple>,
                                     gpointer>) {
          constexpr bool kRestMatch = [&]<size_t... Js>(
                                          std::index_sequence<Js...>) {
            return (
                std::is_same_v<
                    std::tuple_element_t<Js, ArgsTuple>,
                    std::tuple_element_t<(Js < I ? Js : Js + 1), CArgsTuple>> &&
                ...);
          }(std::make_index_sequence<kNumArgs>{});
          if constexpr (kRestMatch) {
            match = I;
          }
        }
      };
      (check.template operator()<Is>(), ...);
    }(std::make_index_sequence<kNumCArgs>{});
    return match;
  }
}

template <typename Ret, typename... Args>
class GlibOnceCallbackThunk {
 public:
  using Callback = base::OnceCallback<Ret(Args...)>;

  template <typename... CArgs>
  using CFunctionPtr = Ret (*)(CArgs...);

  template <typename... CArgs>
    requires(
        FindGlibUserDataIndex<std::tuple<Args...>, std::tuple<CArgs...>>() <
        sizeof...(CArgs))
  // Deliberately implicit so callers can pass `callback` directly to GLib/GTK
  // C APIs regardless of where `gpointer user_data` appears in the C callback
  // parameter list (e.g. `GtkPrintJobCompleteFunc` places it second).
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator CFunctionPtr<CArgs...>() const {
    return &Invoke<CArgs...>;
  }

 private:
  template <typename... CArgs>
  static Ret Invoke(CArgs... c_args) {
    constexpr size_t kUserDataIdx =
        FindGlibUserDataIndex<std::tuple<Args...>, std::tuple<CArgs...>>();
    auto args_tuple = std::forward_as_tuple(std::forward<CArgs>(c_args)...);
    gpointer user_data = std::get<kUserDataIdx>(args_tuple);
    auto* callback = static_cast<Callback*>(user_data);
    return [&]<size_t... Js>(std::index_sequence<Js...>) -> Ret {
      return std::move(*callback).Run(std::forward<Args>(
          std::get<(Js < kUserDataIdx ? Js : Js + 1)>(args_tuple))...);
    }(std::make_index_sequence<sizeof...(Args)>{});
  }
};

}  // namespace internal

template <typename Ret, typename... Args>
struct [[nodiscard]] GlibOnceCallback {
  internal::GlibOnceCallbackThunk<Ret, Args...> callback;
  gpointer user_data = nullptr;
  GDestroyNotify destroy = nullptr;

  STACK_ALLOCATED();
};

// Wraps a `base::OnceCallback` into a `(callback, user_data, destroy)` triple
// for GLib/GTK C APIs that take a `GDestroyNotify` alongside `user_data` (such
// as `gtk_print_job_send`). Ownership of `user_data` is transferred to the C
// API, which must invoke `destroy(user_data)` to free the callback whether or
// not `callback` was called.
template <typename Ret, typename... Args>
GlibOnceCallback<Ret, Args...> WrapGlibOnceCallback(
    base::OnceCallback<Ret(Args...)> callback) {
  CHECK(callback);
  using Callback = base::OnceCallback<Ret(Args...)>;
  return {
      .callback = {},
      .user_data = new Callback(std::move(callback)),
      .destroy = [](gpointer data) { delete static_cast<Callback*>(data); },
  };
}

// Calls `func(leading_args..., callback, user_data, destroy)`, where the last
// three arguments come from `WrapGlibOnceCallback(std::move(callback))`.
// `func` must take `(callback, user_data, destroy)` as its last three
// parameters, e.g. `gtk_print_job_send()`.
template <typename FuncRet,
          typename... FuncArgs,
          typename Ret,
          typename... Args,
          typename... LeadingArgs>
FuncRet CallGlibWithCallback(FuncRet (*func)(FuncArgs...),
                             base::OnceCallback<Ret(Args...)> callback,
                             LeadingArgs&&... leading_args) {
  static_assert(sizeof...(FuncArgs) == sizeof...(LeadingArgs) + 3,
                "`func` must take (callback, user_data, destroy) after the "
                "leading arguments");
  auto wrapped = WrapGlibOnceCallback(std::move(callback));
  return func(std::forward<LeadingArgs>(leading_args)..., wrapped.callback,
              wrapped.user_data, wrapped.destroy);
}

}  // namespace ui

#endif  // UI_BASE_GLIB_GLIB_CALLBACK_H_
