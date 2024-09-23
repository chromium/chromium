// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Util.h"

#include <algorithm>

#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace raw_ptr_plugin {

namespace {

// Directories which are treated as third-party code, which can be used to
// prevent emitting diagnostics in them.
//
// Each one must start and end with a `/` to be used correctly.
const char* kTreatAsThirdPartyDirs[] = {
    "/breakpad/",       //
    "/courgette/",      //
    "/frameworks/",     //
    "/native_client/",  //
    "/ppapi/",          //
    "/testing/",        //
    "/v8/",             //
};

std::string GetNamespaceImpl(const clang::DeclContext* context,
                             const std::string& candidate) {
  switch (context->getDeclKind()) {
    case clang::Decl::TranslationUnit: {
      return candidate;
    }
    case clang::Decl::Namespace: {
      const auto* decl = llvm::dyn_cast<clang::NamespaceDecl>(context);
      std::string name_str;
      llvm::raw_string_ostream OS(name_str);
      if (decl->isAnonymousNamespace()) {
        OS << "<anonymous namespace>";
      } else {
        OS << *decl;
      }
      return GetNamespaceImpl(context->getParent(), OS.str());
    }
    default: {
      return GetNamespaceImpl(context->getParent(), candidate);
    }
  }
}

}  // namespace

std::string GetNamespace(const clang::Decl* record) {
  return GetNamespaceImpl(record->getDeclContext(), std::string());
}

std::string GetFilename(const clang::SourceManager& source_manager,
                        clang::SourceLocation loc,
                        FilenameLocationType type,
                        FilenamesFollowPresumed follow_presumed) {
  switch (type) {
    case FilenameLocationType::kExactLoc:
      break;
    case FilenameLocationType::kSpellingLoc:
      loc = source_manager.getSpellingLoc(loc);
      break;
    case FilenameLocationType::kExpansionLoc:
      loc = source_manager.getExpansionLoc(loc);
      break;
  }
  std::string name;
  if (follow_presumed == FilenamesFollowPresumed::kYes) {
    clang::PresumedLoc ploc = source_manager.getPresumedLoc(loc);
    if (ploc.isInvalid()) {
      // If we're in an invalid location, we're looking at things that aren't
      // actually stated in the source.
      return name;
    }
    name = ploc.getFilename();
  } else {
    name = source_manager.getFilename(loc);
  }

  // File paths can have separators which differ from ones at this platform.
  // Make them consistent.
  std::replace(name.begin(), name.end(), '\\', '/');
  return name;
}

LocationClassification ClassifySourceLocation(
    const clang::HeaderSearchOptions& search,
    const clang::SourceManager& sm,
    clang::SourceLocation loc) {
  if (sm.isInSystemHeader(loc)) {
    return LocationClassification::kSystem;
  }

  std::string filename = GetFilename(sm, loc, FilenameLocationType::kExactLoc);
  if (filename.empty()) {
    // If the filename cannot be determined, simply treat this as third-party
    // code, where we avoid enforcing rules, instead of going through the full
    // lookup process.
    return LocationClassification::kThirdParty;
  }

  // Files in the sysroot do not automatically get categorized as system
  // headers, so we do a path comparison. The sysroot can be set to "/" when it
  // was not specified, which is just the whole filesystem, but every file is
  // not a system header, so this is treated as equivalent to not having a
  // sysroot.
  if (!search.Sysroot.empty() && search.Sysroot != "/" &&
      llvm::StringRef(filename).starts_with(search.Sysroot)) {
    return LocationClassification::kSystem;
  }

  // We need to special case scratch space; which is where clang does its macro
  // expansion. We explicitly want to allow people to do otherwise bad things
  // through macros that were defined due to third party libraries.
  //
  // TODO(danakj): We can further classify this as first/third-party code using
  // a macro defined in first/third-party code. See
  // https://github.com/chromium/subspace/blob/f9c481a241961a7be827d31fadb01badac6ee86a/subdoc/lib/visit.cc#L1566-L1577
  if (filename == "<scratch space>") {
    return LocationClassification::kMacro;
  }

  // Ensure that we can search for patterns of the form "/foo/" even
  // if we have a relative path like "foo/bar.cc".  We don't expect
  // this transformed path to exist necessarily.
  if (filename.front() != '/') {
    filename.insert(0, 1, '/');
  }

  if (filename.find("/gen/") != std::string::npos) {
    return LocationClassification::kGenerated;
  }

  // While blink is inside third_party, it's not all treated like third-party
  // code.
  if (auto p = filename.find("/third_party/blink/"); p != std::string::npos) {
    // Browser-side code is treated like first party in order to have all
    // diagnostics applied. Over time we want the rest of blink code to
    // converge as well.
    //
    // TODO(danakj): Use starts_with() when Clang is compiled with C++20.
    if (!llvm::StringRef(filename).substr(p).starts_with("browser/")) {
      return LocationClassification::kBlink;
    }
  }

  if (filename.find("/third_party/") != std::string::npos) {
    return LocationClassification::kThirdParty;
  }

  for (const char* dir : kTreatAsThirdPartyDirs) {
    if (filename.find(dir) != std::string::npos) {
      return LocationClassification::kThirdParty;
    }
  }

  // TODO(danakj): Designate chromium-owned code in third_party as
  // kChromiumFirstParty.
  return LocationClassification::kFirstParty;
}

}  // namespace raw_ptr_plugin
