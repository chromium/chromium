// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_
#define TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "tools/traffic_annotation/auditor/instance.h"

class TrafficAnnotationExporter {
 public:
  struct ArchivedAnnotation {
    ArchivedAnnotation();
    ArchivedAnnotation(const ArchivedAnnotation& other);
    ~ArchivedAnnotation();

    AnnotationInstance::Type type;

    int unique_id_hash_code;
    int second_id_hash_code;
    int content_hash_code;

    std::string deprecation_date;
    std::vector<std::string> os_list;

    std::set<int> semantics_fields;
    std::set<int> policy_fields;
    std::string file_path;
  };

  TrafficAnnotationExporter(const base::FilePath& source_path);
  ~TrafficAnnotationExporter();
  TrafficAnnotationExporter(const TrafficAnnotationExporter&) = delete;
  TrafficAnnotationExporter(TrafficAnnotationExporter&&) = delete;

  // Loads annotations from annotations.xml file into |archive_|.
  bool LoadAnnotationsXML();

  // Updates |archive_| with current set of extracted annotations and reserved
  // ids. Sets the |modified_| flag if any item is updated. Appends errors to
  // |errors|.
  void UpdateAnnotations(const std::vector<AnnotationInstance>& annotations,
                         const std::map<int, std::string>& reserved_ids,
                         std::vector<AuditorResult>* errors);

  // Saves |archive_| into annotations.xml.
  bool SaveAnnotationsXML() const;

  // Returns the required updates for annotations.xml.
  std::string GetRequiredUpdates();

  // Produces the list of deprecated hash codes. Requires annotations.xml to be
  // loaded.
  void GetDeprecatedHashCodes(std::set<int>* hash_codes);

  bool modified() const { return modified_; }

  // Runs tests on content of |archive_|.
  void CheckArchivedAnnotations(std::vector<AuditorResult>* errors);

  const std::map<std::string, ArchivedAnnotation>& GetArchivedAnnotations()
      const {
    return archive_;
  }

  // Checks if the current platform is in the os list of archived annotation.
  bool MatchesCurrentPlatform(const ArchivedAnnotation& annotation) const {
    return base::ContainsValue(annotation.os_list, current_platform_);
  }

  // Produces the list of annotations that are not defined in this platform.
  // Returns false if annotations.xml is not loaded.
  bool GetOtherPlatformsAnnotationIDs(std::vector<std::string>* ids) const;

  // Returns the number of items in annotations.xml for testing.
  unsigned GetXMLItemsCountForTesting();

  std::string GetXMLDifferencesForTesting(const std::string& old_xml,
                                          const std::string& new_xml) {
    return GetXMLDifferences(old_xml, new_xml);
  }

 private:
  // Generates a text serialized XML for current report items.
  std::string GenerateSerializedXML() const;

  // Returns the required updates to convert one serialized XML to another.
  std::string GetXMLDifferences(const std::string& old_xml,
                                const std::string& new_xml);

  std::vector<std::string> all_supported_platforms_;
  std::map<std::string, ArchivedAnnotation> archive_;
  const base::FilePath source_path_;
  std::string current_platform_;
  bool modified_;
};

#endif  // TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_
