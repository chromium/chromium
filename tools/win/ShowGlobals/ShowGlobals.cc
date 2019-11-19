// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool scans a PDB file and prints out information about 'interesting'
// global variables. This includes duplicates and large globals. This is often
// helpful inunderstanding code bloat or finding inefficient globals.
//
// Duplicate global variables often happen when constructs like this are placed
// in a header file:
//
//     const double sqrt_two = sqrt(2.0);
//
// Many (although usually not all) of the translation units that include this
// header file will get a copy of sqrt_two, possibly including an initializer.
// Because 'const' implies 'static' there are no warnings or errors from the
// linker. This duplication can happen with float/double, structs and classes,
// and arrays - any non-integral type.
//
// Global variables are not necessarily a problem but it is useful to understand
// them, and monitoring their changes can be instructive.

#include <dia2.h>
#include <stdio.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/win/atl.h"

// Helper function for comparing strings - returns a strcmp/wcscmp compatible
// value.
int StringCompare(const std::wstring& lhs, const std::wstring& rhs) {
  return wcscmp(lhs.c_str(), rhs.c_str());
}

// Use this struct to record data about symbols for sorting and analysis.
struct SymbolData {
  SymbolData(ULONGLONG size, DWORD section, const wchar_t* name)
      : size(size), section(section), name(name) {}

  ULONGLONG size;
  DWORD section;
  std::wstring name;
};

// Comparison function for when sorting symbol data by name, in order to allow
// looking for duplicate symbols. It uses the symbol size as a tiebreaker. This
// is necessary because sometimes there are symbols with matching names but
// different sizes in which case they aren't actually duplicates. These false
// positives happen because namespaces are omitted from the symbol names that
// DIA2 returns.
bool NameCompare(const SymbolData& lhs, const SymbolData& rhs) {
  int nameCompare = StringCompare(lhs.name, rhs.name);
  if (nameCompare == 0)
    return lhs.size < rhs.size;
  return nameCompare < 0;
}

// Comparison function for when sorting symbols by size, in order to allow
// finding the largest global variables. Use the symbol names as a tiebreaker
// in order to get consistent ordering.
bool SizeCompare(const SymbolData& lhs, const SymbolData& rhs) {
  if (lhs.size == rhs.size)
    return StringCompare(lhs.name, rhs.name) < 0;
  return lhs.size < rhs.size;
}

// Use this struct to store data about repeated globals, for later sorting.
struct RepeatData {
  RepeatData(ULONGLONG repeat_count,
             ULONGLONG bytes_wasted,
             const std::wstring& name)
      : repeat_count(repeat_count), bytes_wasted(bytes_wasted), name(name) {}
  bool operator<(const RepeatData& rhs) {
    return bytes_wasted < rhs.bytes_wasted;
  }

  ULONGLONG repeat_count;
  ULONGLONG bytes_wasted;
  std::wstring name;
};

bool DumpInterestingGlobals(IDiaSymbol* global, const wchar_t* filename) {
  wprintf(L"#Dups\tDupSize\t  Size\tSection\tSymbol name\tPDB name\n");

  // How many bytes must be wasted on repeats before being listed.
  const int kWastageThreshold = 100;
  // How big must an individual symbol be before being listed.
  const int kBigSizeThreshold = 500;

  std::vector<SymbolData> symbols;
  std::vector<RepeatData> repeats;

  Microsoft::WRL::ComPtr<IDiaEnumSymbols> enum_symbols;
  HRESULT result =
      global->findChildren(SymTagData, NULL, nsNone, &enum_symbols);
  if (FAILED(result)) {
    wprintf(L"ERROR - DumpInterestingGlobals() returned no symbols.\n");
    return false;
  }

  Microsoft::WRL::ComPtr<IDiaSymbol> symbol;
  for (ULONG celt = 0;
       SUCCEEDED(enum_symbols->Next(1, &symbol, &celt)) && (celt == 1);) {
    DWORD location_type = 0;
    // If we can't get the location type then we assume the variable is not of
    // interest.
    if (FAILED(symbol->get_locationType(&location_type))) {
      continue;
    }
    // Ignore location types that don't actually correspond to statics and
    // globals.
    if (location_type != LocIsStatic)
      continue;

    // If we call get_length on symbol it works for functions but not for
    // data. For some reason for data we have to call get_type() to get
    // another IDiaSymbol object which we can query for length.
    Microsoft::WRL::ComPtr<IDiaSymbol> type_symbol;
    if (FAILED(symbol->get_type(&type_symbol))) {
      wprintf(L"Get_type failed.\n");
      continue;
    }

    // Errors in the remainder of this loop can be ignored silently.
    ULONGLONG size = 0;
    type_symbol->get_length(&size);

    // Use -1 and -2 as canary values to indicate various failures.
    DWORD section = static_cast<DWORD>(-1);
    if (symbol->get_addressSection(&section) != S_OK)
      section = static_cast<DWORD>(-2);

    CComBSTR name;
    if (symbol->get_name(&name) == S_OK) {
      symbols.push_back(SymbolData(size, section, name));
    }
  }

  // Sort the symbols by name/size so that we can print a report about duplicate
  // variables.
  std::sort(symbols.begin(), symbols.end(), NameCompare);
  for (auto p = symbols.begin(); p != symbols.end(); /**/) {
    auto pScan = p;
    // Scan the data looking for symbols that have the same name
    // and size.
    while (pScan != symbols.end() && p->size == pScan->size &&
           StringCompare(p->name, pScan->name) == 0)
      ++pScan;

    // Calculate how many times the symbol name/size appears in this PDB.
    size_t repeat_count = pScan - p;
    if (repeat_count > 1) {
      // Change the count from how many instances of this variable there are to
      // how many *excess* instances there are.
      --repeat_count;
      ULONGLONG bytes_wasted = repeat_count * p->size;
      if (bytes_wasted > kWastageThreshold) {
        repeats.push_back(RepeatData(repeat_count, bytes_wasted, p->name));
      }
    }

    p = pScan;
  }

  // Print a summary of duplicated variables, sorted to put the worst offenders
  // first.
  std::sort(repeats.begin(), repeats.end());
  std::reverse(repeats.begin(), repeats.end());
  for (const auto& repeat : repeats) {
    // The empty field contain a zero so that Excel/sheets will more easily
    // create the pivot tables that I want.
    wprintf(L"%llu\t%llu\t%6u\t%u\t%s\t%s\n", repeat.repeat_count,
            repeat.bytes_wasted, 0, 0, repeat.name.c_str(), filename);
  }
  wprintf(L"\n");

  // Print a summary of the largest global variables
  std::sort(symbols.begin(), symbols.end(), SizeCompare);
  std::reverse(symbols.begin(), symbols.end());
  for (const auto& s : symbols) {
    if (s.size < kBigSizeThreshold)
      break;
    // The empty fields contain a zero so that the columns line up which can
    // be important when pasting the data into a spreadsheet.
    wprintf(L"%u\t%u\t%6llu\t%u\t%s\t%s\n", 0, 0, s.size, s.section,
            s.name.c_str(), filename);
  }

  return true;
}

bool Initialize(const wchar_t* filename,
                Microsoft::WRL::ComPtr<IDiaDataSource>& source,
                Microsoft::WRL::ComPtr<IDiaSession>& session,
                Microsoft::WRL::ComPtr<IDiaSymbol>& global) {
  // Initialize DIA2
  HRESULT hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IDiaDataSource), (void**)&source);
  if (FAILED(hr)) {
    wprintf(L"Failed to initialized DIA2 - %08X.\n", hr);
    return false;
  }

  // Open the PDB
  hr = source->loadDataFromPdb(filename);
  if (FAILED(hr)) {
    wprintf(L"LoadDataFromPdb failed - %08X.\n", hr);
    return false;
  }

  hr = source->openSession(&session);
  if (FAILED(hr)) {
    wprintf(L"OpenSession failed - %08X.\n", hr);
    return false;
  }

  // Retrieve a reference to the global scope
  hr = session->get_globalScope(&global);
  if (hr != S_OK) {
    wprintf(L"Get_globalScope failed - %08X.\n", hr);
    return false;
  }

  return true;
}

int wmain(int argc, wchar_t* argv[]) {
  if (argc < 2) {
    wprintf(L"Usage: ShowGlobals file.pdb");
    return -1;
  }

  const wchar_t* filename = argv[1];

  HRESULT hr = CoInitialize(NULL);
  if (FAILED(hr)) {
    wprintf(L"CoInitialize failed - %08X.", hr);
    return false;
  }

  // Extra scope so that we can call CoUninitialize after we destroy our local
  // variables.
  {
    Microsoft::WRL::ComPtr<IDiaDataSource> source;
    Microsoft::WRL::ComPtr<IDiaSession> session;
    Microsoft::WRL::ComPtr<IDiaSymbol> global;
    if (!(Initialize(filename, source, session, global)))
      return -1;

    DumpInterestingGlobals(global.Get(), filename);
  }

  CoUninitialize();
}
