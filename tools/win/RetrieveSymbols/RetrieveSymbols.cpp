// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Symbol downloading demonstration code.
// For more information see ReadMe.txt and this blog post:
// https://randomascii.wordpress.com/2013/03/09/symbols-the-microsoft-way/

#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <string>

// Link with the dbghelp import library
#pragma comment(lib, "dbghelp.lib")

// Uncomment this line to test with known-good parameters.
//#define TESTING

int main(int argc, char* argv[])
{
   // Tell dbghelp to print diagnostics to the debugger output.
   SymSetOptions(SYMOPT_DEBUG);

   // Initialize dbghelp
   const HANDLE fakeProcess = (HANDLE)1;
   BOOL result = SymInitialize(fakeProcess, NULL, FALSE);

#ifdef TESTING
   // Set a search path and cache directory. If this isn't set
   // then _NT_SYMBOL_PATH will be used instead.
   // Force setting it here to make sure that the test succeeds.
   SymSetSearchPath(fakeProcess,
              "SRV*c:\\symbolstest*http://msdl.microsoft.com/download/symbols");

   // Valid PDB data to test the code.
   std::string gTextArg = "072FF0EB54D24DFAAE9D13885486EE09";
   const char* ageText = "2";
   const char* fileName = "kernel32.pdb";

   // Valid PE data to test the code
   fileName = "crypt32.dll";
   const char* dateStampText = "4802A0D7";
   const char* sizeText = "95000";
   //fileName = "chrome_child.dll";
   //const char* dateStampText = "5420D824";
   //const char* sizeText = "20a6000";
#else
   if (argc < 4)
   {
       printf("Error: insufficient arguments.\n");
       printf("Usage: %s guid age pdbname\n", argv[0]);
       printf("Usage: %s dateStamp size pename\n", argv[0]);
       printf("Example: %s 6720c31f4ac24f3ab0243e0641a4412f 1 "
              "chrome_child.dll.pdb\n", argv[0]);
       printf("Example: %s 4802A0D7 95000 crypt32.dll\n", argv[0]);
       return 0;
   }

   std::string gTextArg = argv[1];
   const char* dateStampText = argv[1];
   const char* ageText = argv[2];
   const char* sizeText = argv[2];
   const char* fileName = argv[3];
#endif

   // Parse the GUID and age from the text
   GUID g = {};
   DWORD age = 0;
   DWORD dateStamp = 0;
   DWORD size = 0;

   // Settings for SymFindFileInPath
   void* id = nullptr;
   DWORD flags = 0;
   DWORD two = 0;

   const char* ext = strrchr(fileName, '.');
   if (!ext)
   {
     printf("No extension found on %s. Fatal error.\n", fileName);
     return 0;
   }

   if (_stricmp(ext, ".pdb") == 0)
   {
     std::string gText;
     // Scan the GUID argument and remove all non-hex characters. This allows
     // passing GUIDs with '-', '{', and '}' characters.
     for (auto c : gTextArg)
     {
       if (isxdigit(c))
       {
         gText.push_back(c);
       }
     }
     printf("Parsing symbol data for a PDB file.\n");
     if (gText.size() != 32)
     {
         printf("Error: GUIDs must be exactly 32 characters"
                " (%s was stripped to %s).\n", gTextArg.c_str(), gText.c_str());
         return 10;
     }

     int count = sscanf_s(gText.substr(0, 8).c_str(), "%x", &g.Data1);
     DWORD temp;
     count += sscanf_s(gText.substr(8, 4).c_str(), "%x", &temp);
     g.Data2 = (unsigned short)temp;
     count += sscanf_s(gText.substr(12, 4).c_str(), "%x", &temp);
     g.Data3 = (unsigned short)temp;
     for (auto i = 0; i < ARRAYSIZE(g.Data4); ++i)
     {
         count += sscanf_s(gText.substr(16 + i * 2, 2).c_str(), "%x", &temp);
         g.Data4[i] = (unsigned char)temp;
     }
     count += sscanf_s(ageText, "%x", &age);

     if (count != 12)
     {
         printf("Error: couldn't parse the GUID/age string. Sorry.\n");
         return 10;
     }
     flags = SSRVOPT_GUIDPTR;
     id = &g;
     two = age;
     printf("Looking for %s %s %s.\n", gText.c_str(), ageText, fileName);
   }
   else
   {
     printf("Parsing symbol data for a PE (.dll or .exe) file.\n");
     if (strlen(dateStampText) != 8)
       printf("Warning!!! The datestamp (%s) is not eight characters long. "
              "This is usually wrong.\n", dateStampText);
     int count = sscanf_s(dateStampText, "%x", &dateStamp);
     count += sscanf_s(sizeText, "%x", &size);
     flags = SSRVOPT_DWORDPTR;
     id = &dateStamp;
     two = size;
     printf("Looking for %s %x %x.\n", fileName, dateStamp, two);
   }

   char filePath[MAX_PATH] = {};
   DWORD three = 0;

   if (SymFindFileInPath(fakeProcess, NULL, fileName, id, two, three,
               flags, filePath, NULL, NULL))
   {
       printf("Found symbol file - placed it in %s.\n", filePath);
   }
   else
   {
       printf("Error: symbols not found - error %u. Are dbghelp.dll and "
               "symsrv.dll in the same directory as this executable?\n",
               GetLastError());
       printf("Note that symbol server lookups sometimes fail randomly. "
              "Try again?\n");
   }

   SymCleanup(fakeProcess);

   return 0;
}
