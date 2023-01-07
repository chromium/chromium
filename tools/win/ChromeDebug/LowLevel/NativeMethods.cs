// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using Microsoft.Win32.SafeHandles;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ChromeDebug.LowLevel {
  public static class NativeMethods {
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ReadProcessMemory(IntPtr hProcess,
                                                IntPtr lpBaseAddress,
                                                IntPtr lpBuffer,
                                                int dwSize,
                                                out int lpNumberOfBytesRead);

    [DllImport("ntdll.dll", SetLastError = true)]
    public static extern LowLevelTypes.NTSTATUS NtQueryInformationProcess(
        IntPtr hProcess,
        LowLevelTypes.PROCESSINFOCLASS pic,
        ref LowLevelTypes.PROCESS_BASIC_INFORMATION pbi,
        int cb,
        out int pSize);

    [DllImport("shell32.dll", SetLastError = true)]
    public static extern IntPtr CommandLineToArgvW(
        [MarshalAs(UnmanagedType.LPWStr)] string lpCmdLine,
        out int pNumArgs);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr LocalFree(IntPtr hMem);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        LowLevelTypes.ProcessAccessFlags dwDesiredAccess,
        [MarshalAs(UnmanagedType.Bool)] bool bInheritHandle,
        int dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.StdCall,
        CharSet = CharSet.Unicode)]
    public static extern uint QueryFullProcessImageName(
        IntPtr hProcess,
        [MarshalAs(UnmanagedType.U4)] LowLevelTypes.ProcessQueryImageNameMode flags,
        [Out] StringBuilder lpImageName, ref int size);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern SafeFileHandle CreateFile(string lpFileName,
                                                   LowLevelTypes.FileAccessFlags dwDesiredAccess,
                                                   LowLevelTypes.FileShareFlags dwShareMode,
                                                   IntPtr lpSecurityAttributes,
                                                   LowLevelTypes.FileCreationDisposition dwDisp,
                                                   LowLevelTypes.FileFlagsAndAttributes dwFlags,
                                                   IntPtr hTemplateFile);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SHGetFileInfo(string pszPath,
                                              uint dwFileAttributes,
                                              ref LowLevelTypes.SHFILEINFO psfi,
                                              uint cbFileInfo,
                                              uint uFlags);
  }
}
