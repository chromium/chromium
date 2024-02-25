// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/xproto_internal.h"

#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/glx.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/render.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/sync.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xtest.h"

namespace x11 {

namespace {

template <typename T>
std::unique_ptr<Error> MakeError(RawError error_) {
  ReadBuffer buf(error_);
  auto error = std::make_unique<T>();
  ReadError(error.get(), &buf);
  return error;
}

}  // namespace

void Connection::InitErrorParsers() {
  uint8_t first_errors[256];
  memset(first_errors, 0, sizeof(first_errors));

  auto add_parser = [&](uint8_t error_code, uint8_t first_error,
                        ErrorParser parser) {
    if (!error_parsers_[error_code] || first_error > first_errors[error_code]) {
      first_errors[error_code] = error_code;
      error_parsers_[error_code] = parser;
    }
  };

  if (glx().present()) {
    uint8_t first_error = glx().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Glx::BadContextError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<Glx::BadContextStateError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<Glx::BadDrawableError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<Glx::BadPixmapError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 4;
      auto parse = MakeError<Glx::BadContextTagError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 5;
      auto parse = MakeError<Glx::BadCurrentWindowError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 6;
      auto parse = MakeError<Glx::BadRenderRequestError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 7;
      auto parse = MakeError<Glx::BadLargeRequestError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 8;
      auto parse = MakeError<Glx::UnsupportedPrivateRequestError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 9;
      auto parse = MakeError<Glx::BadFBConfigError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 10;
      auto parse = MakeError<Glx::BadPbufferError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 11;
      auto parse = MakeError<Glx::BadCurrentDrawableError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 12;
      auto parse = MakeError<Glx::BadWindowError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 13;
      auto parse = MakeError<Glx::GLXBadProfileARBError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (randr().present()) {
    uint8_t first_error = randr().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<RandR::BadOutputError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<RandR::BadCrtcError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<RandR::BadModeError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<RandR::BadProviderError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (render().present()) {
    uint8_t first_error = render().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Render::PictFormatError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<Render::PictureError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<Render::PictOpError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<Render::GlyphSetError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 4;
      auto parse = MakeError<Render::GlyphError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (shm().present()) {
    uint8_t first_error = shm().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Shm::BadSegError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<ValueError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<WindowError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 4;
      auto parse = MakeError<PixmapError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 5;
      auto parse = MakeError<AtomError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 6;
      auto parse = MakeError<CursorError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 7;
      auto parse = MakeError<FontError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 9;
      auto parse = MakeError<DrawableError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 12;
      auto parse = MakeError<ColormapError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 13;
      auto parse = MakeError<GContextError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 14;
      auto parse = MakeError<IDChoiceError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (sync().present()) {
    uint8_t first_error = sync().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Sync::CounterError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<Sync::AlarmError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (xfixes().present()) {
    uint8_t first_error = xfixes().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<XFixes::BadRegionError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (xinput().present()) {
    uint8_t first_error = xinput().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Input::DeviceError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<Input::EventError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<Input::ModeError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<Input::DeviceBusyError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 4;
      auto parse = MakeError<Input::ClassError>;
      add_parser(error_code, first_error, parse);
    }
  }

  if (xkb().present()) {
    uint8_t first_error = xkb().first_error();
    {
      auto error_code = first_error + 0;
      auto parse = MakeError<Xkb::KeyboardError>;
      add_parser(error_code, first_error, parse);
    }
  }

  {
    uint8_t first_error = 0;
    {
      auto error_code = first_error + 1;
      auto parse = MakeError<RequestError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 2;
      auto parse = MakeError<ValueError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 3;
      auto parse = MakeError<WindowError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 4;
      auto parse = MakeError<PixmapError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 5;
      auto parse = MakeError<AtomError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 6;
      auto parse = MakeError<CursorError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 7;
      auto parse = MakeError<FontError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 8;
      auto parse = MakeError<MatchError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 9;
      auto parse = MakeError<DrawableError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 10;
      auto parse = MakeError<AccessError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 11;
      auto parse = MakeError<AllocError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 12;
      auto parse = MakeError<ColormapError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 13;
      auto parse = MakeError<GContextError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 14;
      auto parse = MakeError<IDChoiceError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 15;
      auto parse = MakeError<NameError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 16;
      auto parse = MakeError<LengthError>;
      add_parser(error_code, first_error, parse);
    }
    {
      auto error_code = first_error + 17;
      auto parse = MakeError<ImplementationError>;
      add_parser(error_code, first_error, parse);
    }
  }

}

}  // namespace x11
