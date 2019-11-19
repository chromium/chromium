// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CANDIDATE_WINDOW_H_
#define UI_BASE_IME_CANDIDATE_WINDOW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/infolist_entry.h"

namespace ui {

// CandidateWindow represents the structure of candidates generated from IME.
class COMPONENT_EXPORT(UI_BASE_IME_TYPES) CandidateWindow {
 public:
  enum Orientation {
    HORIZONTAL = 0,
    VERTICAL = 1,
  };

  struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) CandidateWindowProperty {
    CandidateWindowProperty();
    virtual ~CandidateWindowProperty();
    int page_size;
    int cursor_position;
    bool is_cursor_visible;
    bool is_vertical;
    bool show_window_at_composition;

    // Auxiliary text is typically displayed in the footer of the candidate
    // window.
    std::string auxiliary_text;
    bool is_auxiliary_text_visible;
  };

  // Represents a candidate entry.
  struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) Entry {
    Entry();
    Entry(const Entry& other);
    virtual ~Entry();
    base::string16 value;
    base::string16 label;
    base::string16 annotation;
    base::string16 description_title;
    base::string16 description_body;
  };

  CandidateWindow();
  virtual ~CandidateWindow();

  // Returns true if the given |candidate_window| is equal to myself.
  bool IsEqual(const CandidateWindow& candidate_window) const;

  // Copies |candidate_window| to myself.
  void CopyFrom(const CandidateWindow& candidate_window);

  const CandidateWindowProperty& GetProperty() const {
    return *property_;
  }
  void SetProperty(const CandidateWindowProperty& property) {
    *property_ = property;
  }

  // Gets the infolist entry models. Sets |has_highlighted| to true if |entries|
  // contains highlighted entry.
  void GetInfolistEntries(std::vector<InfolistEntry>* entries,
                          bool* has_highlighted) const;

  // Returns the number of candidates in one page.
  uint32_t page_size() const { return property_->page_size; }
  void set_page_size(uint32_t page_size) { property_->page_size = page_size; }

  // Returns the cursor index of the currently selected candidate.
  uint32_t cursor_position() const { return property_->cursor_position; }
  void set_cursor_position(uint32_t cursor_position) {
    property_->cursor_position = cursor_position;
  }

  // Returns true if the cursor is visible.
  bool is_cursor_visible() const { return property_->is_cursor_visible; }
  void set_is_cursor_visible(bool is_cursor_visible) {
    property_->is_cursor_visible = is_cursor_visible;
  }

  // Returns the orientation of the candidate window.
  Orientation orientation() const {
    return property_->is_vertical ? VERTICAL : HORIZONTAL;
  }
  void set_orientation(Orientation orientation) {
    property_->is_vertical = (orientation == VERTICAL);
  }

  // Returns true if the auxiliary text is visible.
  bool is_auxiliary_text_visible() const {
    return property_->is_auxiliary_text_visible;
  }
  void set_is_auxiliary_text_visible(bool is_auxiliary_text_visible) const {
    property_->is_auxiliary_text_visible = is_auxiliary_text_visible;
  }

  // Accessors of auxiliary_text.
  const std::string& auxiliary_text() const {
    return property_->auxiliary_text;
  }
  void set_auxiliary_text(const std::string& auxiliary_text) const {
    property_->auxiliary_text = auxiliary_text;
  }

  const std::vector<Entry>& candidates() const { return candidates_; }
  std::vector<Entry>* mutable_candidates() { return &candidates_; }

  bool show_window_at_composition() const {
    return property_->show_window_at_composition;
  }
  void set_show_window_at_composition(bool show_window_at_composition) {
    property_->show_window_at_composition = show_window_at_composition;
  }

 private:
  std::unique_ptr<CandidateWindowProperty> property_;
  std::vector<Entry> candidates_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindow);
};

}  // namespace ui

#endif  // UI_BASE_IME_CANDIDATE_WINDOW_H_
