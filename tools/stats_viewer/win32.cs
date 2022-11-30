// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace StatsViewer {
  /// <summary>
  /// Win32 API constants, structs, and wrappers for access via C#.
  /// </summary>
  class Win32 {
    #region Constants
    public enum MapAccess {
      FILE_MAP_COPY = 0x0001,
      FILE_MAP_WRITE = 0x0002,
      FILE_MAP_READ = 0x0004,
      FILE_MAP_ALL_ACCESS = 0x001f,
    }

    public const int GENERIC_READ = unchecked((int)0x80000000);
    public const int GENERIC_WRITE = unchecked((int)0x40000000);
    public const int OPEN_ALWAYS = 4;
    public static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);
    #endregion

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr CreateFile ( 
       String lpFileName, int dwDesiredAccess, int dwShareMode,
       IntPtr lpSecurityAttributes, int dwCreationDisposition,
       int dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32", SetLastError=true)]
    public static extern IntPtr MapViewOfFile (
       IntPtr hFileMappingObject, int dwDesiredAccess, int dwFileOffsetHigh,
       int dwFileOffsetLow, int dwNumBytesToMap);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr OpenFileMapping (
       int dwDesiredAccess, bool bInheritHandle, String lpName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool UnmapViewOfFile (IntPtr lpBaseAddress);

    [DllImport("kernel32", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);
  }
}
