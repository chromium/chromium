// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A general interface for filtering and only acting on classes in Chromium C++
// code.

#include "ChromeClassTester.h"

#include "Util.h"
#include "clang/AST/AST.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"

#ifdef LLVM_ON_UNIX
#include <sys/param.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

using namespace clang;
using chrome_checker::Options;

namespace {

bool ends_with(const std::string& one, const std::string& two) {
  if (two.size() > one.size())
    return false;

  return one.compare(one.size() - two.size(), two.size(), two) == 0;
}

}  // namespace

ChromeClassTester::ChromeClassTester(CompilerInstance& instance,
                                     const Options& options)
    : options_(options),
      instance_(instance),
      diagnostic_(instance.getDiagnostics()) {
  BuildBannedLists();
}

ChromeClassTester::~ChromeClassTester() {}

void ChromeClassTester::CheckTag(TagDecl* tag) {
  // We handle class types here where we have semantic information. We can only
  // check structs/classes/enums here, but we get a bunch of nice semantic
  // information instead of just parsing information.
  SourceLocation location = tag->getInnerLocStart();
  LocationType location_type = ClassifyLocation(location);
  if (location_type == LocationType::kThirdParty)
    return;

  if (CXXRecordDecl* record = dyn_cast<CXXRecordDecl>(tag)) {
    // We sadly need to maintain a blocklist of types that violate these
    // rules, but do so for good reason or due to limitations of this
    // checker (i.e., we don't handle extern templates very well).
    std::string base_name = record->getNameAsString();
    if (IsIgnoredType(base_name))
      return;

    CheckChromeClass(location_type, location, record);
  }
}

ChromeClassTester::LocationType ChromeClassTester::ClassifyLocation(
    SourceLocation loc) {
  auto classification = chrome_checker::ClassifySourceLocation(
      instance().getHeaderSearchOpts(), instance().getSourceManager(), loc);

  // Convert to a less granular legacy classificatoin.
  switch (classification) {
    case chrome_checker::LocationClassification::kFirstParty:
      return LocationType::kChrome;
    case chrome_checker::LocationClassification::kBlink:
      return LocationType::kBlink;
    case chrome_checker::LocationClassification::kChromiumThirdParty:
      return LocationType::kThirdParty;
    case chrome_checker::LocationClassification::kThirdParty:
      return LocationType::kThirdParty;
    case chrome_checker::LocationClassification::kGenerated:
      return LocationType::kThirdParty;
    case chrome_checker::LocationClassification::kMacro:
      return LocationType::kThirdParty;
    case chrome_checker::LocationClassification::kSystem:
      return LocationType::kThirdParty;
  }
  assert(false);
}

bool ChromeClassTester::HasIgnoredBases(const CXXRecordDecl* record) {
  for (const auto& base : record->bases()) {
    CXXRecordDecl* base_record = base.getType()->getAsCXXRecordDecl();
    if (!base_record)
      continue;

    const std::string& base_name = base_record->getQualifiedNameAsString();
    if (ignored_base_classes_.count(base_name) > 0)
      return true;
    if (HasIgnoredBases(base_record))
      return true;
  }
  return false;
}

bool ChromeClassTester::InImplementationFile(SourceLocation record_location) {
  std::string filename;

  // If |record_location| is a macro, check the whole chain of expansions.
  const SourceManager& source_manager = instance_.getSourceManager();
  while (true) {
    filename = GetFilename(instance().getSourceManager(), record_location,
                           FilenameLocationType::kSpellingLoc);
    if (ends_with(filename, ".cc") || ends_with(filename, ".cpp") ||
        ends_with(filename, ".mm")) {
      return true;
    }
    if (!record_location.isMacroID()) {
      break;
    }
    record_location =
        source_manager.getImmediateExpansionRange(record_location).getBegin();
  }

  return false;
}

void ChromeClassTester::BuildBannedLists() {
  // A complicated pickle derived struct that is all packed integers.
  ignored_record_names_.emplace("Header");

  // Part of the GPU system that uses multiple included header
  // weirdness. Never getting this right.
  ignored_record_names_.emplace("Validators");

  // Has a UNIT_TEST only constructor. Isn't *terribly* complex...
  ignored_record_names_.emplace("AutocompleteController");
  ignored_record_names_.emplace("HistoryURLProvider");

  // Used over in the net unittests. A large enough bundle of integers with 1
  // non-pod class member. Probably harmless.
  ignored_record_names_.emplace("MockTransaction");

  // Used heavily in ui_base_unittests and once in views_unittests. Fixing this
  // isn't worth the overhead of an additional library.
  ignored_record_names_.emplace("TestAnimationDelegate");

  // Part of our public interface that nacl and friends use. (Arguably, this
  // should mean that this is a higher priority but fixing this looks hard.)
  ignored_record_names_.emplace("PluginVersionInfo");

  // Measured performance improvement on cc_perftests. See
  // https://codereview.chromium.org/11299290/
  ignored_record_names_.emplace("QuadF");

  // Ignore IPC::NoParams bases, since these structs are generated via
  // macros and it makes it difficult to add explicit ctors.
  ignored_base_classes_.emplace("IPC::NoParams");
}

bool ChromeClassTester::IsIgnoredType(std::string_view base_name) {
  return ignored_record_names_.count(base_name) != 0u;
}

DiagnosticsEngine::Level ChromeClassTester::getErrorLevel() {
  return diagnostic().getWarningsAsErrors() ? DiagnosticsEngine::Error
                                            : DiagnosticsEngine::Warning;
}
