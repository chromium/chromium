// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Guids.cs
// MUST match guids.h
using System;

namespace ChromeDebug {
  static class GuidList {
    public const string guidChromeDebugPkgString = "7de8bbab-82c7-4871-b82c-4d5d44a3979d";
    public const string guidChromeDebugCmdSetString = "6608d840-ce6c-45ab-b856-eb0a0b471ff1";

    public static readonly Guid guidChromeDebugCmdSet = new Guid(guidChromeDebugCmdSetString);
  };
}