// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "EditTracker.h"

#include <assert.h>
#include <stdio.h>
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace {

const char* GetTag(RenameCategory category) {
  switch (category) {
    case RenameCategory::kEnumValue:
      return "enum";
    case RenameCategory::kField:
      return "var";
    case RenameCategory::kFunction:
      return "func";
    case RenameCategory::kUnresolved:
      return "unresolved";
    case RenameCategory::kVariable:
      return "var";
  }
}

}  // namespace

EditTracker::EditTracker(RenameCategory category) : category_(category) {}

void EditTracker::Add(const clang::SourceManager& source_manager,
                      clang::SourceLocation location,
                      llvm::StringRef original_text,
                      llvm::StringRef new_text) {
  llvm::StringRef filename;
  for (int i = 0; i < 10; i++) {
    filename = source_manager.getFilename(location);
    if (!filename.empty() || !location.isMacroID())
      break;
    // Otherwise, no filename and the SourceLocation is a macro ID. Look one
    // level up the stack...
    location = source_manager.getImmediateMacroCallerLoc(location);
  }
  assert(!filename.empty() && "Can't track edit with no filename!");
  auto result = tracked_edits_.try_emplace(original_text);
  if (result.second) {
    result.first->getValue().new_text = new_text;
  }
  result.first->getValue().filenames.try_emplace(filename);
}

void EditTracker::SerializeTo(llvm::raw_ostream& output) const {
  const char* tag = GetTag(category_);
  for (const auto& edit : tracked_edits_) {
    for (const auto& filename : edit.getValue().filenames) {
      output << filename.getKey() << ":" << tag << ":" << edit.getKey() << ":"
             << edit.getValue().new_text << "\n";
    }
  }
}
