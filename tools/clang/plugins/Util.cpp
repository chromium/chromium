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
                        clang::SourceLocation location) {
  clang::SourceLocation spelling_location =
      source_manager.getSpellingLoc(location);
  clang::PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
  if (ploc.isInvalid()) {
    // If we're in an invalid location, we're looking at things that aren't
    // actually stated in the source.
    return "";
  }

  std::string name = ploc.getFilename();

  // File paths can have separators which differ from ones at this platform.
  // Make them consistent.
  std::replace(name.begin(), name.end(), '\\', '/');
  return name;
}

namespace chrome_checker {

LocationClassification ClassifySourceLocation(const clang::SourceManager& sm,
                                              clang::SourceLocation loc) {
  if (sm.isInSystemHeader(loc)) {
    return LocationClassification::kSystem;
  }

  std::string filename = GetFilename(sm, loc);
  if (filename.empty()) {
    // If the filename cannot be determined, simply treat this as third-party
    // code, where we avoid enforcing rules, instead of going through the full
    // lookup process.
    return LocationClassification::kThirdParty;
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

}  // namespace chrome_checker
