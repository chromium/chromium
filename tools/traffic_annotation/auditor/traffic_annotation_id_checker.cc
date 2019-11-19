// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_id_checker.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

TrafficAnnotationIDChecker::TrafficAnnotationIDChecker(
    const std::set<int>& reserved_ids,
    const std::set<int>& deprecated_ids)
    : deprecated_ids_(deprecated_ids), reserved_ids_(reserved_ids) {}

TrafficAnnotationIDChecker::~TrafficAnnotationIDChecker() = default;

void TrafficAnnotationIDChecker::Load(
    const std::vector<AnnotationInstance>& extracted_annotations) {
  annotations_.clear();
  for (const AnnotationInstance& instance : extracted_annotations) {
    AnnotationItem item;
    item.type = instance.type;
    item.ids[0].hash_code = instance.unique_id_hash_code;
    item.ids[0].text = instance.proto.unique_id();
    if (instance.NeedsTwoIDs()) {
      item.ids_count = 2;
      item.ids[1].hash_code = instance.second_id_hash_code;
      item.ids[1].text = instance.second_id;
    } else {
      item.ids_count = 1;
    }
    item.file_path = instance.proto.source().file();
    item.line_number = instance.proto.source().line();
    item.loaded_from_archive = instance.is_loaded_from_archive;
    annotations_.push_back(item);
  }
}

void TrafficAnnotationIDChecker::CheckIDs(std::vector<AuditorResult>* errors) {
  CheckIDsFormat(errors);
  CheckForSecondIDs(errors);
  CheckForInvalidValues(
      reserved_ids_, AuditorResult::Type::ERROR_RESERVED_ID_HASH_CODE, errors);
  CheckForInvalidValues(deprecated_ids_,
                        AuditorResult::Type::ERROR_DEPRECATED_ID_HASH_CODE,
                        errors);
  CheckForHashCollisions(errors);
  CheckForInvalidRepeatedIDs(errors);
}

void TrafficAnnotationIDChecker::CheckForInvalidValues(
    const std::set<int>& invalid_set,
    AuditorResult::Type error_type,
    std::vector<AuditorResult>* errors) {
  for (AnnotationItem& item : annotations_) {
    for (int i = 0; i < item.ids_count; i++) {
      if (base::Contains(invalid_set, item.ids[i].hash_code)) {
        errors->push_back(AuditorResult(error_type, item.ids[i].text,
                                        item.file_path, item.line_number));
      }
    }
  }
}

void TrafficAnnotationIDChecker::CheckForSecondIDs(
    std::vector<AuditorResult>* errors) {
  for (AnnotationItem& item : annotations_) {
    if (item.ids_count == 2 &&
        (item.ids[1].text.empty() ||
         item.ids[0].hash_code == item.ids[1].hash_code)) {
      errors->push_back(
          AuditorResult(AuditorResult::Type::ERROR_MISSING_SECOND_ID,
                        std::string(), item.file_path, item.line_number));
      // Remove this id from next tests.
      item.ids_count = 0;
    }
  }
}

void TrafficAnnotationIDChecker::CheckForHashCollisions(
    std::vector<AuditorResult>* errors) {
  std::map<int, std::string> collisions;
  for (AnnotationItem& item : annotations_) {
    for (int i = 0; i < item.ids_count; i++) {
      if (!base::Contains(collisions, item.ids[i].hash_code)) {
        // If item is loaded from archive, and it is the second id, do not keep
        // the id for checks. Archive just keeps the hash code of the second id
        // and the text value of it is not correct.
        if (!item.loaded_from_archive || !i) {
          collisions.insert(
              std::make_pair(item.ids[i].hash_code, item.ids[i].text));
        }
      } else {
        if (item.loaded_from_archive && i)
          continue;
        if (item.ids[i].text != collisions[item.ids[i].hash_code]) {
          AuditorResult error(AuditorResult::Type::ERROR_HASH_CODE_COLLISION,
                              item.ids[i].text);
          error.AddDetail(collisions[item.ids[i].hash_code]);
          errors->push_back(error);
        }
      }
    }
  }
}

void TrafficAnnotationIDChecker::CheckForInvalidRepeatedIDs(
    std::vector<AuditorResult>* errors) {
  std::map<int, AnnotationItem*> first_ids;
  std::map<int, AnnotationItem*> second_ids;

  // Check if first ids are unique.
  for (AnnotationItem& item : annotations_) {
    if (!base::Contains(first_ids, item.ids[0].hash_code)) {
      first_ids[item.ids[0].hash_code] = &item;
    } else {
      errors->push_back(CreateRepeatedIDError(
          item.ids[0].text, item, *first_ids[item.ids[0].hash_code]));
    }
  }

  // If a second id is equal to a first id, owner of the second id should be of
  // type PARTIAL and owner of the first id should be of type COMPLETING.
  for (AnnotationItem& item : annotations_) {
    if (item.ids_count == 2 &&
        base::Contains(first_ids, item.ids[1].hash_code)) {
      if (item.type != AnnotationInstance::Type::ANNOTATION_PARTIAL ||
          first_ids[item.ids[1].hash_code]->type !=
              AnnotationInstance::Type::ANNOTATION_COMPLETING) {
        errors->push_back(CreateRepeatedIDError(
            item.ids[1].text, item, *first_ids[item.ids[1].hash_code]));
      }
    }
  }

  // If two second ids are equal, they should be either PARTIAL or
  // BRANCHED_COMPLETING.
  for (AnnotationItem& item : annotations_) {
    if (item.ids_count != 2)
      continue;
    if (!base::Contains(second_ids, item.ids[1].hash_code)) {
      second_ids[item.ids[1].hash_code] = &item;
    } else {
      AnnotationInstance::Type other_type =
          second_ids[item.ids[1].hash_code]->type;
      if ((item.type != AnnotationInstance::Type::ANNOTATION_PARTIAL &&
           item.type !=
               AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING) ||
          (other_type != AnnotationInstance::Type::ANNOTATION_PARTIAL &&
           other_type !=
               AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING)) {
        errors->push_back(CreateRepeatedIDError(
            item.ids[1].text, item, *second_ids[item.ids[1].hash_code]));
      }
    }
  }
}

void TrafficAnnotationIDChecker::CheckIDsFormat(
    std::vector<AuditorResult>* errors) {
  for (AnnotationItem& item : annotations_) {
    bool any_failed = false;
    for (int i = 0; i < item.ids_count; i++) {
      if (!base::ContainsOnlyChars(base::ToLowerASCII(item.ids[i].text),
                                   "0123456789_abcdefghijklmnopqrstuvwxyz")) {
        errors->push_back(
            AuditorResult(AuditorResult::Type::ERROR_ID_INVALID_CHARACTER,
                          item.ids[i].text, item.file_path, item.line_number));
        any_failed = true;
      }
    }
    // Remove this id from next tests.
    if (any_failed)
      item.ids_count = 0;
  }
}

AuditorResult TrafficAnnotationIDChecker::CreateRepeatedIDError(
    const std::string& common_id,
    const AnnotationItem& item1,
    const AnnotationItem& item2) {
  AuditorResult error(
      AuditorResult::Type::ERROR_REPEATED_ID,
      base::StringPrintf("%s in '%s:%i'", common_id.c_str(),
                         item1.file_path.c_str(), item1.line_number));
  error.AddDetail(base::StringPrintf("'%s:%i'", item2.file_path.c_str(),
                                     item2.line_number));
  return error;
}
