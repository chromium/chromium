// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

PlatformClipboard::DataMap MakeTextData(const std::string& text) {
  PlatformClipboard::DataMap data_map;
  data_map[std::string("text/plain")] =
      base::MakeRefCounted<base::RefCountedBytes>(
          std::vector<uint8_t>(text.begin(), text.end()));
  return data_map;
}

}  // namespace

// Verifies the X11-specific behavior that a clipboard write bumps the sequence
// number synchronously, and that the asynchronous XFixes echo of that same
// write does not bump it a second time (while a genuine foreign change still
// does).
class X11ClipboardOzoneTest : public testing::Test {
 public:
  X11ClipboardOzoneTest() : connection_(*x11::Connection::Get()) {}

  X11ClipboardOzoneTest(const X11ClipboardOzoneTest&) = delete;
  X11ClipboardOzoneTest& operator=(const X11ClipboardOzoneTest&) = delete;

  ~X11ClipboardOzoneTest() override = default;

 protected:
  void SetUp() override {
    event_source_ = std::make_unique<X11EventSource>(x11::Connection::Get());
    clipboard_ = std::make_unique<X11ClipboardOzone>();
    clipboard_->SetClipboardDataChangedCallback(
        base::BindRepeating(&X11ClipboardOzoneTest::OnClipboardDataChanged,
                            base::Unretained(this)));
  }

  void TearDown() override {
    clipboard_.reset();
    event_source_.reset();
  }

  void OnClipboardDataChanged(ClipboardBuffer buffer) {
    ++change_count_;
    last_buffer_ = buffer;
  }

  // Fabricates and dispatches an XFixes SetSelectionOwner SelectionNotify event
  // for `selection` reporting `owner` as the new owner. This is the same event
  // the X server emits when a selection's owner changes; dispatching it
  // directly makes the owner-filtering deterministic without depending on the
  // timing of the real server echo.
  void DispatchSelectionNotify(x11::Atom selection, x11::Window owner) {
    x11::XFixes::SelectionNotifyEvent notify{
        .subtype = x11::XFixes::SelectionEvent::SetSelectionOwner,
        .window = ui::GetX11RootWindow(),
        .owner = owner,
        .selection = selection,
    };
    connection_->DispatchEvent(x11::Event(/*send_event=*/false, notify));
  }

  raw_ref<x11::Connection> connection_;
  std::unique_ptr<X11EventSource> event_source_;
  std::unique_ptr<X11ClipboardOzone> clipboard_;
  int change_count_ = 0;
  ClipboardBuffer last_buffer_ = ClipboardBuffer::kCopyPaste;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Taking ownership of the clipboard must bump the
// sequence number synchronously (i.e. before the message loop is pumped and
// before the X server round-trip completes).
TEST_F(X11ClipboardOzoneTest, OfferClipboardDataBumpsSequenceSynchronously) {
  clipboard_->OfferClipboardData(ClipboardBuffer::kCopyPaste,
                                 MakeTextData("hello"));

  EXPECT_EQ(1, change_count_);
  EXPECT_EQ(ClipboardBuffer::kCopyPaste, last_buffer_);
}

// The XFixes echo of our own write (its new owner is our own selection-owner
// window) must be filtered out, so it does not bump the sequence number a
// second time.
TEST_F(X11ClipboardOzoneTest, SelfEchoDoesNotBumpSequenceAgain) {
  if (!connection_->xfixes().present()) {
    GTEST_SKIP()
        << "XFixes isn't available so SelectionChangeObserver won't subscribe "
           "to selection events in this environment.";
  }

  const x11::Atom clipboard_atom = x11::GetAtom("CLIPBOARD");
  clipboard_->OfferClipboardData(ClipboardBuffer::kCopyPaste,
                                 MakeTextData("hello"));

  // The write bumps the sequence number exactly once, synchronously.
  ASSERT_EQ(1, change_count_);

  const x11::Window our_owner = clipboard_->GetSelectionOwnerWindowForTesting();
  ASSERT_NE(x11::Window::None, our_owner);

  // The XFixes echo of that write reports our own window as the new owner, so
  // it must be filtered out. The count therefore stays at 1: bumped by the
  // write, not by the echo.
  DispatchSelectionNotify(clipboard_atom, our_owner);
  EXPECT_EQ(1, change_count_);
}

// A genuine foreign clipboard change (a different owner window) must bump the
// sequence number. This confirms the owner comparison does not swallow real
// changes.
TEST_F(X11ClipboardOzoneTest, ForeignSelectionChangeBumpsSequence) {
  if (!connection_->xfixes().present()) {
    GTEST_SKIP()
        << "XFixes isn't available so SelectionChangeObserver won't subscribe "
           "to selection events in this environment.";
  }

  const x11::Atom clipboard_atom = x11::GetAtom("CLIPBOARD");
  clipboard_->OfferClipboardData(ClipboardBuffer::kCopyPaste,
                                 MakeTextData("hello"));

  // The write bumps the sequence number exactly once, synchronously.
  ASSERT_EQ(1, change_count_);

  // A window that is not our selection-owner window models another client
  // taking ownership of the clipboard.
  const x11::Window foreign_owner =
      connection_->CreateDummyWindow("Foreign clipboard owner");
  ASSERT_NE(clipboard_->GetSelectionOwnerWindowForTesting(), foreign_owner);

  // A selection change owned by a different window is a genuine foreign change,
  // so it must bump the sequence number a second time (to 2).
  DispatchSelectionNotify(clipboard_atom, foreign_owner);
  EXPECT_EQ(2, change_count_);
  EXPECT_EQ(ClipboardBuffer::kCopyPaste, last_buffer_);
}

}  // namespace ui
