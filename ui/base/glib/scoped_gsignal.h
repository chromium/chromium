// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_SCOPED_GSIGNAL_H_
#define UI_BASE_GLIB_SCOPED_GSIGNAL_H_

#include <glib-object.h>
#include <glib.h>

#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "ui/base/glib/scoped_gobject.h"

// ScopedGSignal manages the lifecycle of a GLib signal connection.
// It disconnects the signal when this object is destroyed or goes out of scope.
// This class should be used on a single sequence.
class COMPONENT_EXPORT(UI_BASE) ScopedGSignal {
 public:
  // Constructs and connects a GLib signal with specified attributes.
  // Parameters:
  // - instance: The GLib object instance emitting the signal.
  // - detailed_signal: Signal name to connect.
  // - handler: Callback function to invoke when the signal is emitted.
  // - connect_flags: Optional flags to influence signal connection behavior.
  // If signal connection fails, this object will be left unconnected.
  // `Connected()` can be used to check for signal connection success.
  template <typename Sender, typename Ret, typename... Args>
  ScopedGSignal(Sender* instance,
                const gchar* detailed_signal,
                base::RepeatingCallback<Ret(Sender*, Args...)> handler,
                GConnectFlags connect_flags = static_cast<GConnectFlags>(0))
      : impl_(std::make_unique<SignalImpl<Sender, Ret, Args...>>(
            instance,
            detailed_signal,
            std::move(handler),
            connect_flags)) {
    if (!Connected()) {
      // No need to keep `impl_` around.
      impl_.reset();
    }
  }

  // Overload accepting a ScopedGObject.
  template <typename Sender, typename Ret, typename... Args>
  ScopedGSignal(ScopedGObject<Sender> instance,
                const gchar* detailed_signal,
                base::RepeatingCallback<Ret(Sender*, Args...)> handler,
                GConnectFlags connect_flags = static_cast<GConnectFlags>(0))
      : ScopedGSignal(instance.get(),
                      detailed_signal,
                      std::move(handler),
                      connect_flags) {}

  // Overload accepting a raw_ptr.
  template <typename Sender, typename Ret, typename... Args>
  ScopedGSignal(raw_ptr<Sender> instance,
                const gchar* detailed_signal,
                base::RepeatingCallback<Ret(Sender*, Args...)> handler,
                GConnectFlags connect_flags = static_cast<GConnectFlags>(0))
      : ScopedGSignal(instance.get(),
                      detailed_signal,
                      std::move(handler),
                      connect_flags) {}

  // Constructs an unconnected ScopedGSignal.
  ScopedGSignal();

  ScopedGSignal(ScopedGSignal&&) noexcept;
  ScopedGSignal& operator=(ScopedGSignal&&) noexcept;

  ~ScopedGSignal();

  [[nodiscard]] bool Connected() const;

  void Reset();

 private:
  // The implementation uses the PIMPL idiom for the following reasons:
  // 1. GLib binds a user data pointer that gets passed to the callback.
  //    This means the implementation class can't be movable.  To support
  //    moves, keep a pointer to the implementation.
  // 2. Type erasure: the derived class depends on the callback type and
  //    sender type, so a virtual destructor is required.
  class SignalBase {
   public:
    SignalBase(SignalBase&&) = delete;
    SignalBase& operator=(SignalBase&&) = delete;

    virtual ~SignalBase();

    [[nodiscard]] bool Connected() const { return signal_id_; }

   protected:
    SignalBase();

    [[nodiscard]] gulong signal_id() const { return signal_id_; }
    void set_signal_id(gulong signal_id) { signal_id_ = signal_id; }

   private:
    gulong signal_id_ = 0;
  };

  template <typename Sender, typename Ret, typename... Args>
  class SignalImpl final : public SignalBase {
   public:
    using Handler = base::RepeatingCallback<Ret(Sender*, Args...)>;

    SignalImpl(Sender* instance,
               const gchar* detailed_signal,
               Handler handler,
               GConnectFlags connect_flags) {
      CHECK(instance);
      CHECK(detailed_signal);
      CHECK(handler);

      const bool swapped = connect_flags & G_CONNECT_SWAPPED;
      const bool after = connect_flags & G_CONNECT_AFTER;

      auto* new_closure = swapped ? g_cclosure_new_swap : g_cclosure_new;
      if (!(gclosure_ = new_closure(G_CALLBACK(OnSignalEmittedThunk), this,
                                    OnDisconnectedThunk))) {
        LOG(ERROR) << "Failed to create GClosure";
        return;
      }

      set_signal_id(g_signal_connect_closure(instance, detailed_signal,
                                             gclosure_, after));
      if (!signal_id()) {
        LOG(ERROR) << "Failed to connect to " << detailed_signal;
        // Prevent OnDisconnectedThunk from running.
        g_closure_remove_finalize_notifier(gclosure_, this,
                                           OnDisconnectedThunk);
        // Remove the floating reference to free `gclosure_`.  Note that this
        // should not be called if the signal connected since it will take
        // ownership of `gclosure_`.
        g_closure_unref(gclosure_.ExtractAsDangling());
        return;
      }

      sender_ = instance;
      handler_ = std::move(handler);
    }

    ~SignalImpl() override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      if (!Connected()) {
        return;
      }
      // If the finalize notifier is not removed, it will get called some time
      // after this object has been destroyed.  Remove the finalize notifier to
      // prevent this.
      g_closure_remove_finalize_notifier(gclosure_.ExtractAsDangling(), this,
                                         OnDisconnectedThunk);
      g_signal_handler_disconnect(sender_, signal_id());
      // `OnDisconnected()` must be explicitly called since the finalize
      // notifier was removed.
      OnDisconnected();
    }

   private:
    static Ret OnSignalEmittedThunk(Sender* sender,
                                    Args... args,
                                    gpointer self) {
      return reinterpret_cast<SignalImpl*>(self)->OnSignalEmitted(sender,
                                                                  args...);
    }

    static void OnDisconnectedThunk(gpointer self, GClosure* closure) {
      reinterpret_cast<SignalImpl*>(self)->OnDisconnected();
    }

    Ret OnSignalEmitted(Sender* sender, Args... args) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return handler_.Run(sender, args...);
    }

    void OnDisconnected() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      CHECK(Connected());
      set_signal_id(0);
      sender_ = nullptr;
      gclosure_ = nullptr;
      handler_.Reset();
    }

    raw_ptr<Sender> sender_ = nullptr;
    raw_ptr<GClosure> gclosure_ = nullptr;
    Handler handler_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  std::unique_ptr<SignalBase> impl_;
};

#endif  // UI_BASE_GLIB_SCOPED_GSIGNAL_H_
