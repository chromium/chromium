// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_TRAFFIC_ANNOTATION_AUDITOR_AUDITOR_RESULT_H_
#define TOOLS_TRAFFIC_ANNOTATION_AUDITOR_AUDITOR_RESULT_H_

#include "base/files/file_path.h"

// Holds the auditor processing results on one unit of annotation or function.
class AuditorResult {
 public:
  enum class Type {
    RESULT_OK,               // No error
    RESULT_IGNORE,           // The item does not require further processing.
    ERROR_FATAL,             // A fatal error that should stop process.
    ERROR_MISSING_TAG_USED,  // A function is called with
                             // MISSING_TRAFFIC_ANNOTATION tag.
    ERROR_NO_ANNOTATION,     // A function is called with NO_ANNOTATION tag.
                             // Deprecated, as NO_ANNOTATION is now undefined on
                             // supported platforms.
    ERROR_SYNTAX,            // Annotation syntax is not right.
    ERROR_RESERVED_ID_HASH_CODE,    // An id has a hash code equal to a reserved
                                    // word.
    ERROR_DEPRECATED_ID_HASH_CODE,  // An id has a hash code equal to a
                                    // deprecated one.
    ERROR_HASH_CODE_COLLISION,      // Two ids have equal hash codes.
    ERROR_REPEATED_ID,            // An id is used in two places without proper
                                  // matching conditions. Proper conditions
                                  // include cases that two annotations are
                                  // completing each other or are different
                                  // branches of one completing annotation.
    ERROR_ID_INVALID_CHARACTER,   // An id contanins a characer which is not
                                  // alphanumeric or underline.
    ERROR_MISSING_ANNOTATION,     // A function that requires annotation is not
                                  // annotated.
    ERROR_MISSING_SECOND_ID,      // Annotation does not have a valid second id.
    ERROR_INCOMPLETE_ANNOTATION,  // Annotation has some missing fields.
    ERROR_INCONSISTENT_ANNOTATION,  // Annotation has some inconsistent fields.
    ERROR_MERGE_FAILED,            // Two annotations that are supposed to merge
                                   // cannot merge.
    ERROR_INCOMPLETED_ANNOTATION,  // A partial or [branched_] completing
                                   // annotation is not paired with any other
                                   // annotation to be completed.
    ERROR_DIRECT_ASSIGNMENT,       // A value is directly assigned to a mutable
                                   // annotation or annotation initialized with
                                   // list expresssion.
    ERROR_ANNOTATIONS_XML_UPDATE,  // Annotations XML requires update.
    ERROR_TEST_ANNOTATION,         // Annotation for tests is used.
    ERROR_INVALID_OS,              // Invalid 'os_list' in annotations.xml
    ERROR_DEPRECATED_WITH_OS,      // Marked deprecated, but 'os_list' is not
                                   // empty in annotations.xml.

    // This is the python_script equivalent of ERROR_DIRECT_ASSIGNMENT.
    //
    // TODO(crbug/966883): remove ERROR_DIRECT_ASSIGNMENT and related code
    // when done migrating to python_script.
    ERROR_MUTABLE_TAG,  // Can't create a
                        // |MutableNetworkTrafficAnnotationTag| from
                        // anywhere (except whitelisted files).
  };

  static const int kNoCodeLineSpecified;

  AuditorResult(Type type,
                const std::string& message,
                const std::string& file_path,
                int line);

  AuditorResult(Type type, const std::string& message);

  AuditorResult(Type type);

  ~AuditorResult();

  AuditorResult(const AuditorResult& other);

  void AddDetail(const std::string& message);

  Type type() const { return type_; }

  std::string file_path() const { return file_path_; }
  void set_file_path(const std::string& file_path) { file_path_ = file_path; }

  // Formats the error message into one line of text.
  std::string ToText() const;

  // Formats the error message into one line of text that just includes the
  // error reason and not the annotations and files invloved. It can be used to
  // create a new error based on another one.
  std::string ToShortText() const;

  bool IsOK() { return type_ == Type::RESULT_OK; }

 private:
  Type type_;
  std::vector<std::string> details_;
  std::string file_path_;
  int line_;
};

#endif  // TOOLS_TRAFFIC_ANNOTATION_AUDITOR_AUDITOR_RESULT_H_
