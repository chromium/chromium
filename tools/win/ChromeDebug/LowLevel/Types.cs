// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ChromeDebug.LowLevel {
  // Defines structures, enumerations, and types used by Win32 API calls.  In some cases, the API
  // calls support (and even document) many more values than what are listed here.  Should
  // additional values be required, they can be added to the respective types.
  public static class LowLevelTypes {

    #region Constants and Enums
    // Represents the image format of a DLL or executable.
    public enum ImageFormat {
      NATIVE,
      MANAGED,
      UNKNOWN
    }

    // Flags used for opening a file handle (e.g. in a call to CreateFile), that determine the
    // requested permission level.
    [Flags]
    public enum FileAccessFlags : uint {
      GENERIC_WRITE = 0x40000000,
      GENERIC_READ = 0x80000000
    }

    // Value used for CreateFile to determine how to behave in the presence (or absence) of a
    // file with the requested name.  Used only for CreateFile.
    public enum FileCreationDisposition : uint {
      CREATE_NEW = 1,
      CREATE_ALWAYS = 2,
      OPEN_EXISTING = 3,
      OPEN_ALWAYS = 4,
      TRUNCATE_EXISTING = 5
    }

    // Flags that determine what level of sharing this application requests on the target file.
    // Used only for CreateFile.
    [Flags]
    public enum FileShareFlags : uint {
      EXCLUSIVE_ACCESS = 0x0,
      SHARE_READ = 0x1,
      SHARE_WRITE = 0x2,
      SHARE_DELETE = 0x4
    }

    // Flags that control caching and other behavior of the underlying file object.  Used only for
    // CreateFile.
    [Flags]
    public enum FileFlagsAndAttributes : uint {
      NORMAL = 0x80,
      OPEN_REPARSE_POINT = 0x200000,
      SEQUENTIAL_SCAN = 0x8000000,
      RANDOM_ACCESS = 0x10000000,
      NO_BUFFERING = 0x20000000,
      OVERLAPPED = 0x40000000
    }

    // The target architecture of a given executable image.  The various values correspond to the
    // magic numbers defined by the PE Executable Image File Format.
    // http://www.microsoft.com/whdc/system/platform/firmware/PECOFF.mspx
    public enum MachineType : ushort {
      UNKNOWN = 0x0,
      X64 = 0x8664,
      X86 = 0x14c,
      IA64 = 0x200
    }

    // A flag indicating the format of the path string that Windows returns from a call to
    // QueryFullProcessImageName().
    public enum ProcessQueryImageNameMode : uint {
      WIN32_FORMAT = 0,
      NATIVE_SYSTEM_FORMAT = 1
    }

    // Flags indicating the level of permission requested when opening a handle to an external
    // process.  Used by OpenProcess().
    [Flags]
    public enum ProcessAccessFlags : uint {
      NONE = 0x0,
      ALL = 0x001F0FFF,
      VM_OPERATION = 0x00000008,
      VM_READ = 0x00000010,
      QUERY_INFORMATION = 0x00000400,
      QUERY_LIMITED_INFORMATION = 0x00001000
    }

    // Defines return value codes used by various Win32 System APIs.
    public enum NTSTATUS : int {
      SUCCESS = 0,
    }

    // Determines the amount of information requested (and hence the type of structure returned)
    // by a call to NtQueryInformationProcess.
    public enum PROCESSINFOCLASS : int {
      PROCESS_BASIC_INFORMATION = 0
    };

    [Flags]
    public enum SHGFI : uint {
      Icon = 0x000000100,
      DisplayName = 0x000000200,
      TypeName = 0x000000400,
      Attributes = 0x000000800,
      IconLocation = 0x000001000,
      ExeType = 0x000002000,
      SysIconIndex = 0x000004000,
      LinkOverlay = 0x000008000,
      Selected = 0x000010000,
      Attr_Specified = 0x000020000,
      LargeIcon = 0x000000000,
      SmallIcon = 0x000000001,
      OpenIcon = 0x000000002,
      ShellIconSize = 0x000000004,
      PIDL = 0x000000008,
      UseFileAttributes = 0x000000010,
      AddOverlays = 0x000000020,
      OverlayIndex = 0x000000040,
    }   
    #endregion

    #region Structures
    // In general, for all structures below which contains a pointer (represented here by IntPtr),
    // the pointers refer to memory in the address space of the process from which the original
    // structure was read.  While this seems obvious, it means we cannot provide an elegant
    // interface to the various fields in the structure due to the de-reference requiring a
    // handle to the target process.  Instead, that functionality needs to be provided at a
    // higher level.
    //
    // Additionally, since we usually explicitly define the fields that we're interested in along
    // with their respective offsets, we frequently specify the exact size of the native structure.

    // Win32 UNICODE_STRING structure.
    [StructLayout(LayoutKind.Sequential)]
    public struct UNICODE_STRING {
      // The length in bytes of the string pointed to by buffer, not including the null-terminator.
      private ushort length;
      // The total allocated size in memory pointed to by buffer.
      private ushort maximumLength;
      // A pointer to the buffer containing the string data.
      private IntPtr buffer;

      public ushort Length { get { return length; } }
      public ushort MaximumLength { get { return maximumLength; } }
      public IntPtr Buffer { get { return buffer; } }
    }

    // Win32 RTL_USER_PROCESS_PARAMETERS structure.
    [StructLayout(LayoutKind.Explicit, Size = 72)]
    public struct RTL_USER_PROCESS_PARAMETERS {
      [FieldOffset(56)]
      private UNICODE_STRING imagePathName;
      [FieldOffset(64)]
      private UNICODE_STRING commandLine;

      public UNICODE_STRING ImagePathName { get { return imagePathName; } }
      public UNICODE_STRING CommandLine { get { return commandLine; } }
    };

    // Win32 PEB structure.  Represents the process environment block of a process.
    [StructLayout(LayoutKind.Explicit, Size = 472)]
    public struct PEB {
      [FieldOffset(2), MarshalAs(UnmanagedType.U1)]
      private bool isBeingDebugged;
      [FieldOffset(12)]
      private IntPtr ldr;
      [FieldOffset(16)]
      private IntPtr processParameters;
      [FieldOffset(468)]
      private uint sessionId;

      public bool IsBeingDebugged { get { return isBeingDebugged; } }
      public IntPtr Ldr { get { return ldr; } }
      public IntPtr ProcessParameters { get { return processParameters; } }
      public uint SessionId { get { return sessionId; } }
    };

    // Win32 PROCESS_BASIC_INFORMATION.  Contains a pointer to the PEB, and various other
    // information about a process.
    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct PROCESS_BASIC_INFORMATION {
      [FieldOffset(4)]
      private IntPtr pebBaseAddress;
      [FieldOffset(16)]
      private UIntPtr uniqueProcessId;

      public IntPtr PebBaseAddress { get { return pebBaseAddress; } }
      public UIntPtr UniqueProcessId { get { return uniqueProcessId; } }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct SHFILEINFO {
      // C# doesn't support overriding the default constructor of value types, so we need to use
      // a dummy constructor.
      public SHFILEINFO(bool dummy) {
        hIcon = IntPtr.Zero;
        iIcon = 0;
        dwAttributes = 0;
        szDisplayName = "";
        szTypeName = "";
      }
      public IntPtr hIcon;
      public int iIcon;
      public uint dwAttributes;
      [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
      public string szDisplayName;
      [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
      public string szTypeName;
    };
    #endregion
  }
}
