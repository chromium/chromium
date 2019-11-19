@REM Copyright 2017 The Chromium Authors. All rights reserved.
@REM Use of this source code is governed by a BSD-style license that can be
@REM found in the LICENSE file.

@REM midl.exe output is arch-specific, remember to run this for all supported
@REM target_cpu, currently x86, x64 and arm64.
ninja -C out\gn ^
    gen/browser_switcher/ie_bho/ie_bho_idl.h ^
    gen/google_update/google_update_idl.h ^
    gen/remoting/host/win/chromoting_lib.h ^
    gen/third_party/iaccessible2/ia2_api_all.h ^
    gen/third_party/isimpledom/ISimpleDOMDocument.h ^
    gen/third_party/isimpledom/ISimpleDOMNode.h ^
    gen/third_party/isimpledom/ISimpleDOMText.h
