// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_constants.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/buildflags.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/drag_controller.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/selection_controller.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/word_lookup_client.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <vector>
#endif

namespace base {
class TimeDelta;
}

#if BUILDFLAG(IS_MAC)
namespace ui {
class ScopedPasswordInputEnabler;
}
#endif  // BUILDFLAG(IS_MAC)

namespace views {

class MenuRunner;
class TextfieldController;
class ViewsTextServicesContextMenu;

// A views/skia textfield implementation. No platform-specific code is used.
class VIEWS_EXPORT Textfield : public View,
                               public TextfieldModel::Delegate,
                               public ContextMenuController,
                               public DragController,
                               public WordLookupClient,
                               public SelectionControllerDelegate,
                               public ui::TouchEditable,
                               public ui::TextInputClient,
                               public views::ViewObserver {
  METADATA_HEADER(Textfield, View)

 public:
  enum MenuCommands {
    kUndo = kLastTouchEditableCommandId + 1,
    kDelete,
    kLastCommandId = kDelete,
  };

#if BUILDFLAG(IS_MAC)
  static constexpr gfx::SelectionBehavior kLineSelectionBehavior =
      gfx::SELECTION_EXTEND;
  static constexpr gfx::SelectionBehavior kWordSelectionBehavior =
      gfx::SELECTION_CARET;
  static constexpr gfx::SelectionBehavior kMoveParagraphSelectionBehavior =
      gfx::SELECTION_CARET;
  static constexpr gfx::SelectionBehavior kPageSelectionBehavior =
      gfx::SELECTION_EXTEND;
#else
  static constexpr gfx::SelectionBehavior kLineSelectionBehavior =
      gfx::SELECTION_RETAIN;
  static constexpr gfx::SelectionBehavior kWordSelectionBehavior =
      gfx::SELECTION_RETAIN;
  static constexpr gfx::SelectionBehavior kMoveParagraphSelectionBehavior =
      gfx::SELECTION_RETAIN;
  static constexpr gfx::SelectionBehavior kPageSelectionBehavior =
      gfx::SELECTION_RETAIN;
#endif

  // Pair of |text_changed|, |cursor_changed|.
  using EditCommandResult = std::pair<bool, bool>;

  // Returns the text cursor blink time, or 0 for no blinking.
  static base::TimeDelta GetCaretBlinkInterval();

  // Returns the default FontList used by all textfields.
  static const gfx::FontList& GetDefaultFontList();

  Textfield();
  Textfield(const Textfield&) = delete;
  Textfield& operator=(const Textfield&) = delete;
  ~Textfield() override;

  // Set the controller for this textfield.
  void set_controller(TextfieldController* controller) {
    controller_ = controller;
  }

  // TODD (kylixrd): Remove set_controller and refactor codebase.
  void SetController(TextfieldController* controller);

  // Gets/Sets whether or not the Textfield is read-only.
  bool GetReadOnly() const;
  void SetReadOnly(bool read_only);

  // Sets the input type; displays only asterisks for TEXT_INPUT_TYPE_PASSWORD.
  void SetTextInputType(ui::TextInputType type);

  // Sets the input flags so that the system input methods can turn on/off some
  // features. The flags is the bit map of ui::TextInputFlags.
  void SetTextInputFlags(int flags);

  // Gets the text for the Textfield.
  // NOTE: Call sites should take care to not reveal the text for a password
  // textfield.
  const std::u16string& GetText() const;

  // Sets the text currently displayed in the Textfield.
  void SetText(const std::u16string& new_text);

  // Sets the text currently displayed in the Textfield and the cursor position.
  // Does not fire notifications about the caret bounds changing. This is
  // intended for low-level use, where callers need precise control over what
  // notifications are fired when, e.g. to avoid firing duplicate accessibility
  // notifications, which can cause issues for accessibility tools. Updating the
  // selection or cursor separately afterwards does not update the edit history,
  // i.e. the cursor position after redoing this change will be determined by
  // |cursor_position| and not by subsequent calls to e.g. SetSelectedRange().
  void SetTextWithoutCaretBoundsChangeNotification(const std::u16string& text,
                                                   size_t cursor_position);

  // Scrolls all of |scroll_positions| into view, if possible. For each
  // position, the minimum scrolling change necessary to just bring the position
  // into view is applied. |scroll_positions| are applied in order, so later
  // positions will have priority over earlier positions if not all can be
  // visible simultaneously.
  // NOTE: Unlike MoveCursorTo(), this will not fire any accessibility
  // notifications.
  void Scroll(const std::vector<size_t>& scroll_positions);

  // Appends the given string to the previously-existing text in the field.
  void AppendText(const std::u16string& new_text);

  // Inserts |new_text| at the cursor position, replacing any selected text.
  // This method is used to handle user input via paths Textfield doesn't
  // normally handle, so it calls UpdateAfterChange() and notifies observers of
  // changes.
  void InsertOrReplaceText(const std::u16string& new_text);

  // Returns the text that is currently selected.
  // NOTE: Call sites should take care to not reveal the text for a password
  // textfield.
  std::u16string GetSelectedText() const;

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // Selects the word at which the cursor is currently positioned. If there is a
  // non-empty selection, the selection bounds are extended to their nearest
  // word boundaries.
  void SelectWord();

  // A convenience method to select the word closest to |point|.
  void SelectWordAt(const gfx::Point& point);

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection();

  // Checks if there is any selected text. |primary_only| indicates whether
  // secondary selections should also be considered.
  bool HasSelection(bool primary_only = false) const;

  // Gets/sets the text color to be used when painting the Textfield.
  SkColor GetTextColor() const;
  void SetTextColor(SkColor color);

  // Gets/sets the background color to be used when painting the Textfield.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);

  // Getter/Setter methods for `is_background_enabled_` which controls
  // whether a background is drawn for this view.
  bool GetBackgroundEnabled() const;
  void SetBackgroundEnabled(bool enabled);

  // Gets/sets the selection text color to be used when painting the Textfield.
  SkColor GetSelectionTextColor() const;
  void SetSelectionTextColor(SkColor color);

  // Gets/sets the selection background color to be used when painting the
  // Textfield.
  SkColor GetSelectionBackgroundColor() const;
  void SetSelectionBackgroundColor(SkColor color);

  // Gets/Sets whether or not the cursor is enabled.
  bool GetCursorEnabled() const;
  void SetCursorEnabled(bool enabled);

  // Gets/Sets the fonts used when rendering the text within the Textfield.
  const gfx::FontList& GetFontList() const;
  void SetFontList(const gfx::FontList& font_list);

  // Sets the default width of the text control. See default_width_in_chars_.
  void SetDefaultWidthInChars(int default_width);

  // Sets the minimum width of the text control. See minimum_width_in_chars_.
  void SetMinimumWidthInChars(int minimum_width);

  // Gets/Sets the text to display when empty.
  const std::u16string& GetPlaceholderText() const;
  void SetPlaceholderText(const std::u16string& text);

  void set_placeholder_text_color(SkColor color) {
    placeholder_text_color_ = color;
  }

  void set_placeholder_font_list(const gfx::FontList& font_list) {
    placeholder_font_list_ = font_list;
  }

  int placeholder_text_draw_flags() const {
    return placeholder_text_draw_flags_;
  }
  void set_placeholder_text_draw_flags(int flags) {
    placeholder_text_draw_flags_ = flags;
  }

  bool force_text_directionality() const { return force_text_directionality_; }
  void set_force_text_directionality(bool force) {
    force_text_directionality_ = force;
  }

  bool drop_cursor_visible() const { return drop_cursor_visible_; }

  // Gets/Sets whether to indicate the textfield has invalid content.
  bool GetInvalid() const;
  void SetInvalid(bool invalid);

  // Get or set the horizontal alignment used for the button from the underlying
  // RenderText object.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Displays a virtual keyboard or alternate input view if enabled.
  void ShowVirtualKeyboardIfEnabled();

  // Returns whether or not an IME is composing text.
  bool IsIMEComposing() const;

  // Gets the selected logical text range.
  const gfx::Range& GetSelectedRange() const;

  // Selects the specified logical text range.
  void SetSelectedRange(const gfx::Range& range);

  // Without clearing the current selected range, adds |range| as an additional
  // selection.
  // NOTE: Unlike SetSelectedRange(), this will not fire any accessibility
  // notifications.
  void AddSecondarySelectedRange(const gfx::Range& range);

  // Gets the text selection model.
  const gfx::SelectionModel& GetSelectionModel() const;

  // Sets the specified text selection model.
  void SelectSelectionModel(const gfx::SelectionModel& sel);

  // Returns the current cursor position.
  size_t GetCursorPosition() const;

  // Set the text color over the entire text or a logical character range.
  // Empty and invalid ranges are ignored.
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const gfx::Range& range);

  // Set various text styles over the entire text or a logical character range.
  // The respective |style| is applied if |value| is true, or removed if false.
  // Empty and invalid ranges are ignored.
  void SetStyle(gfx::TextStyle style, bool value);
  void ApplyStyle(gfx::TextStyle style, bool value, const gfx::Range& range);

  // Clears Edit history.
  void ClearEditHistory();

  // Set extra spacing placed between glyphs; used for obscured text styling.
  void SetObscuredGlyphSpacing(int spacing);

  std::optional<size_t> GetPasswordCharRevealIndex() const {
    return password_char_reveal_index_;
  }

  void SetExtraInsets(const gfx::Insets& insets);

  // Fits the textfield to the local bounds, applying internal padding and
  // updating the cursor position and visibility.
  void FitToLocalBounds();

  // Getter/Setter methods for `use_default_border_`.
  bool GetUseDefaultBorder() const;
  void SetUseDefaultBorder(bool use_default_border);

  // Removes the Inkdrop hover effect.
  void RemoveHoverEffect();

  // View overrides:
  int GetBaseline() const override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void SetBorder(std::unique_ptr<Border> b) override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  WordLookupClient* GetWordLookupClient() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void OnDragDone() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  gfx::Point GetKeyboardContextMenuLocation() override;
  void OnThemeChanged() override;

  // TextfieldModel::Delegate overrides:
  void OnCompositionTextConfirmedOrCleared() override;
  void OnTextChanged() override;

  // ContextMenuController overrides:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // DragController overrides:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // WordLookupClient overrides:
  bool GetWordLookupDataAtPoint(const gfx::Point& point,
                                gfx::DecoratedText* decorated_word,
                                gfx::Rect* rect) override;
  bool GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                      gfx::Rect* rect) override;

  // SelectionControllerDelegate overrides:
  bool HasTextBeingDragged() const override;

  // ui::TouchEditable overrides:
  void MoveCaret(const gfx::Point& position) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void SelectBetweenCoordinates(const gfx::Point& base,
                                const gfx::Point& extent) override;
  void GetSelectionEndPoints(gfx::SelectionBound* anchor,
                             gfx::SelectionBound* focus) override;
  gfx::Rect GetBounds() override;
  gfx::NativeView GetNativeView() const override;
  bool IsSelectionDragging() const override;
  void ConvertPointToScreen(gfx::Point* point) override;
  void ConvertPointFromScreen(gfx::Point* point) override;
  void OpenContextMenu(const gfx::Point& anchor) override;
  void DestroyTouchSelection() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // ui::TextInputClient overrides:
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const ui::CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
  bool GetCompositionCharacterBounds(size_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
#if BUILDFLAG(IS_MAC)
  bool DeleteRange(const gfx::Range& range) override;
#else
  bool DeleteRange(const gfx::Range& range);
#endif
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

  // Set whether the text should be used to improve typing suggestions.
  void SetShouldDoLearning(bool value) { should_do_learning_ = value; }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  void GetActiveTextInputControlLayoutBounds(
      std::optional<gfx::Rect>* control_bounds,
      std::optional<gfx::Rect>* selection_bounds) override;
#endif

#if BUILDFLAG(IS_WIN)
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const std::u16string& active_composition_text,
      bool is_composition_committed) override;
#endif

  // ViewObserver overrides:
  void OnViewFocused(views::View* observed_view) override;

  [[nodiscard]] base::CallbackListSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

 protected:
  TextfieldModel* textfield_model() { return model_.get(); }

  // Inserts or appends a character in response to an IME operation.
  virtual void DoInsertChar(char16_t ch);

  // Returns the TextfieldModel's text/cursor/selection rendering model.
  gfx::RenderText* GetRenderText() const;

  // Returns the last click root location (relative to the root window).
  gfx::Point GetLastClickRootLocation() const;

  // Get the text from the selection clipboard.
  virtual std::u16string GetSelectionClipboardText() const;

  // Executes the given |command|.
  virtual void ExecuteTextEditCommand(ui::TextEditCommand command);

  // Offsets the double-clicked word's range. This is only used in the unusual
  // case where the text changes on the second mousedown of a double-click.
  // This is harmless if there is not a currently double-clicked word.
  void OffsetDoubleClickWord(size_t offset);

  // Returns true if the drop cursor is for insertion at a target text location,
  // the standard behavior/style. Returns false when drop will do something
  // else (like replace the text entirely).
  virtual bool IsDropCursorForInsertion() const;

  // Returns true if the placeholder text should be shown. Subclasses may
  // override this to customize when the placeholder text is shown.
  virtual bool ShouldShowPlaceholderText() const;

  // Like RequestFocus, but explicitly states that the focus is triggered by
  // a pointer event.
  void RequestFocusWithPointer(ui::EventPointerType pointer_type);

  // Like RequestFocus, but explicitly states that the focus is triggered by a
  // gesture event.
  void RequestFocusForGesture(const ui::GestureEventDetails& details);

  virtual Textfield::EditCommandResult DoExecuteTextEditCommand(
      ui::TextEditCommand command);

  // Handles key press event ahead of OnKeyPressed(). This is used for Textarea
  // to handle the return key. Use TextfieldController::HandleKeyEvent to
  // intercept the key event in other cases.
  virtual bool PreHandleKeyPressed(const ui::KeyEvent& event);

  // Get the default command for a given key |event|.
  virtual ui::TextEditCommand GetCommandForKeyEvent(const ui::KeyEvent& event);

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // Called when the accessible text offsets for the textfield need to be
  // recomputed on the next pass.
  virtual void SetNeedsAccessibleTextOffsetsUpdate();
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  // Update the cursor position in the text field.
  void UpdateCursorViewPosition();

  // A callback function to periodically update the cursor node_data.
  void UpdateCursorVisibility();

  // Returns true if a context menu for this view is showing.
  bool IsMenuShowing() const;

  virtual void UpdateAccessibleTextSelection() {}

  void AddedToWidget() override;

 private:
  friend class TextfieldTestApi;

  enum class TextChangeType { kNone, kInternal, kUserTriggered };

  // View overrides:
  // Declared final since overriding by subclasses would interfere with the
  // accounting related to the scheduled text edit command. Subclasses should
  // use TextfieldController::HandleKeyEvent, to intercept the key event.
  bool OnKeyPressed(const ui::KeyEvent& event) final;
  bool OnKeyReleased(const ui::KeyEvent& event) final;

  // SelectionControllerDelegate overrides:
  gfx::RenderText* GetRenderTextForSelectionController() override;
  bool IsReadOnly() const override;
  bool SupportsDrag() const override;
  void SetTextBeingDragged(bool value) override;
  int GetViewHeight() const override;
  int GetViewWidth() const override;
  int GetDragSelectionDelay() const override;
  void OnBeforePointerAction() override;
  void OnAfterPointerAction(bool text_changed, bool selection_changed) override;
  // Callers within Textfield should call UpdateAfterChange depending on the
  // return value.
  bool PasteSelectionClipboard() override;
  void UpdateSelectionClipboard() override;

  // Updates the painted background color.
  void UpdateBackgroundColor();

  // Updates the border per the state of the textfield (i.e. Normal, Invalid,
  // Readonly, Disabled). This will not do anything if a custom border has been
  // set by SetBorder().
  void UpdateDefaultBorder();

  // Updates the selection text color.
  void UpdateSelectionTextColor();

  // Updates the selection background color.
  void UpdateSelectionBackgroundColor();

  // Does necessary updates when the text and/or cursor position changes.
  // If |notify_caret_bounds_changed| is not explicitly set, it will be computed
  // based on whether either of the other arguments is set.
  void UpdateAfterChange(
      TextChangeType text_change_type,
      bool cursor_changed,
      std::optional<bool> notify_caret_bounds_changed = std::nullopt);

  virtual void UpdateAccessibilityTextDirection();

  // Subclass OmniboxViewViews is overriding this method to update the
  // accessible value.
  virtual void UpdateAccessibleValue();

  // Updates cursor visibility and blinks the cursor if needed.
  void ShowCursor();

  // Gets the style::TextStyle that should be used.
  int GetTextStyle() const;

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Helper function to call MoveCursorTo on the TextfieldModel.
  void MoveCursorTo(const gfx::Point& point, bool select);

  // Recalculates cursor view bounds based on model_.
  gfx::Rect CalculateCursorViewBounds() const;

  // Convenience method to notify the InputMethod and TouchSelectionController.
  void OnCaretBoundsChanged();

  // Convenience method to call TextfieldController::OnBeforeUserAction();
  void OnBeforeUserAction();

  // Convenience method to call TextfieldController::OnAfterUserAction();
  void OnAfterUserAction();

  // Calls |model_->Cut()| and notifies TextfieldController on success.
  bool Cut();

  // Calls |model_->Copy()| and notifies TextfieldController on success.
  bool Copy();

  // Calls |model_->Paste()| and calls TextfieldController::ContentsChanged()
  // explicitly if paste succeeded.
  bool Paste();

  // Utility function to prepare the context menu.
  void UpdateContextMenu();

  // Returns true if the current text input type allows access by the IME.
  bool ImeEditingAllowed() const;

  // Reveals the password character at |index| for a set duration.
  // If |index| is nullopt, the existing revealed character will be reset.
  // |duration| is the time to remain the password char to be visible.
  void RevealPasswordChar(std::optional<size_t> index,
                          base::TimeDelta duration);

  void CreateTouchSelectionControllerAndNotifyIt();

  // Called when editing a textfield fails because the textfield is readonly.
  void OnEditFailed();

  // Returns true if an insertion cursor should be visible (a vertical bar,
  // placed at the point new text will be inserted).
  bool ShouldShowCursor() const;

  // Converts a textfield width in "average characters" to the required number
  // of DIPs, accounting for insets and cursor.
  int CharsToDips(int width_in_chars) const;

  // Returns true if an insertion cursor should be visible and blinking.
  bool ShouldBlinkCursor() const;

  // Starts and stops blinking the cursor, respectively. These are both
  // idempotent if the cursor is already blinking/not blinking.
  void StartBlinkingCursor();
  void StopBlinkingCursor();

  // Callback for the cursor blink timer. Called every
  // Textfield::GetCaretBlinkMs().
  void OnCursorBlinkTimerFired();

  void OnEnabledChanged();

  // Drops the dragged text.
  void DropDraggedText(
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Returns the corner radius of the text field.
  float GetCornerRadius();

  // Prepares the Textfield for gesture scrolling by setting the drag start
  // state.
  void OnGestureScrollBegin(int drag_start_location_x);

  // Performs gesture scrolling.
  void GestureScroll(int drag_location_x);

  // Performs gesture handling needed for touch selection dragging. Sets `event`
  // as handled and returns true if the event should not be processed further.
  bool HandleGestureForSelectionDragging(ui::GestureEvent* event);

  // Determines whether touch selection dragging should start and updates the
  // selection dragging state if needed. Returns true if selection dragging
  // starts.
  bool StartSelectionDragging(const ui::GestureEvent& event);

  void StopSelectionDragging();

  void UpdateAccessibleDefaultActionVerb();

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // Calculate widths for each grapheme and word starts and ends. Used for
  // accessibility. Currently only on Windows when UIA is enabled.
  void RefreshAccessibleTextOffsets();
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  // The text model.
  std::unique_ptr<TextfieldModel> model_;

  // This is the current listener for events from this Textfield.
  raw_ptr<TextfieldController> controller_ = nullptr;

  // An edit command to execute on the next key event. When set to a valid
  // value, the key event is still passed to |controller_|, but otherwise
  // ignored in favor of the edit command. Set via
  // SetTextEditCommandForNextKeyEvent() during dispatch of that key event (see
  // comment in TextInputClient).
  ui::TextEditCommand scheduled_text_edit_command_ =
      ui::TextEditCommand::INVALID_COMMAND;

  // True if this Textfield cannot accept input and is read-only.
  bool read_only_ = false;

  // The default number of average characters for the width of this text field.
  // This will be reported as the "desired size". Must be set to >=
  // minimum_width_in_chars_. Defaults to 0.
  int default_width_in_chars_ = 0;

  // The minimum allowed width of this text field in average characters. This
  // will be reported as the minimum size. Must be set to <=
  // default_width_in_chars_. Setting this to -1 will cause GetMinimumSize() to
  // return View::GetMinimumSize(). Defaults to -1.
  int minimum_width_in_chars_ = -1;

  // Colors which override default system colors.
  // TODO(tluk): These should be updated to be ColorIds instead of SkColors.
  std::optional<SkColor> text_color_;
  std::optional<SkColor> background_color_;
  std::optional<SkColor> selection_text_color_;
  std::optional<SkColor> selection_background_color_;

  // Text to display when empty.
  std::u16string placeholder_text_;

  // Placeholder text color.
  // TODO(newcomer): Use NativeTheme to define different default placeholder
  // text colors for chrome/CrOS when harmony is enabled by default
  // (https://crbug.com/803279).
  std::optional<SkColor> placeholder_text_color_;

  // The draw flags specified for |placeholder_text_|.
  int placeholder_text_draw_flags_;

  // The font used for the placeholder text. If this value is null, the
  // placeholder text uses the same font list as the underlying RenderText.
  std::optional<gfx::FontList> placeholder_font_list_;

  // True when the contents are deemed unacceptable and should be indicated as
  // such.
  bool invalid_ = false;

  // The input type of this text field.
  ui::TextInputType text_input_type_ = ui::TEXT_INPUT_TYPE_TEXT;

  // The input flags of this text field.
  int text_input_flags_ = 0;

  // The timer to reveal the last typed password character.
  base::OneShotTimer password_reveal_timer_;

  // Tracks whether a user action is being performed which may change the
  // textfield; i.e. OnBeforeUserAction() has been called, but
  // OnAfterUserAction() has not yet been called.
  bool performing_user_action_ = false;

  // True if InputMethod::CancelComposition() should not be called.
  bool skip_input_method_cancel_composition_ = false;

  // Insertion cursor repaint timer and visibility.
  base::RepeatingTimer cursor_blink_timer_;

  // The drop cursor is a visual cue for where dragged text will be dropped.
  bool drop_cursor_visible_ = false;
  gfx::SelectionModel drop_cursor_position_;

  // Is the user potentially dragging and dropping from this view?
  bool initiating_drag_ = false;

  std::unique_ptr<TouchSelectionController> touch_selection_controller_;

  SelectionController selection_controller_;

  // Tracks the touch selection dragging state which is used when determining
  // whether a dragging movement should be used for scrolling, cursor placement
  // or adjusting the text selection.
  enum class SelectionDraggingState {
    // Default state, i.e. no selection dragging gestures being handled.
    kNone,
    // A gesture has used to select all text (e.g. triple press), but dragging
    // has not started yet.
    kSelectedAll,
    // A gesture has used to select word (e.g. long press or double press), but
    // dragging has not started yet.
    kSelectedWord,
    // Dragging gesture is being handled to move the cursor. This state is
    // reached if a dragging gesture begins while the cursor is present (roughly
    // corresponds to the case where no text was selected by a prior gesture).
    kDraggingCursor,
    // Dragging gesture is being handled to adjust the selection. This state is
    // reached if a dragging gesture begins after a word was selected by a prior
    // gesture.
    kDraggingSelectionExtent
  };
  SelectionDraggingState selection_dragging_state_ =
      SelectionDraggingState::kNone;

  // Tracks the type of the current or pending selection drag gesture.
  std::optional<ui::TouchSelectionDragType> selection_drag_type_;

  // The offset applied to the touch drag location when determining selection
  // updates.
  gfx::Vector2d selection_dragging_offset_;

  // Used to track touch drag starting location and offset to enable touch
  // scrolling.
  int drag_start_location_x_;
  int drag_start_display_offset_ = 0;

  // Tracks the selection extent, which is used to determine the logical end of
  // the selection. Roughly, this corresponds to the last drag position of the
  // touch handle or scroll gesture used to update the selection range.
  gfx::Point selection_extent_;

  // Specifies granularity of selection extent updates, i.e. the break type
  // where we can place the end of the selection when the extent is moved. For
  // "expand by word, shrink by character" behaviour, the break type is set to
  // WORD_BREAK if the selection has expanded past the current word boundary and
  // back to CHARACTER_BREAK if the selection is shrinking.
  gfx::BreakType break_type_ = gfx::CHARACTER_BREAK;

  // The horizontal offset applied to selection extent when adjusting the
  // selection. We apply this offset to smoothen the movement of the end of the
  // selection after switching between word and character granularity.
  int extent_offset_x_ = 0;

  // Whether touch selection handles should be shown once the current scroll
  // sequence ends. Handles should be shown if touch editing handles were hidden
  // while scrolling or if part of the scroll sequence was used for cursor
  // placement or adjusting the text selection.
  bool show_touch_handles_after_scroll_ = false;

  // Whether the user should be notified if the clipboard is restricted.
  bool show_rejection_ui_if_any_ = false;

  // Whether the text should be used to improve typing suggestions.
  std::optional<bool> should_do_learning_;

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // The string used to compute the text offsets for accessibility. This is used
  // to determine if the offsets need to be recomputed.
  std::u16string ax_value_used_to_compute_offsets_;

  // Whether the last computed text offsets are still valid.
  bool needs_ax_text_offsets_update_;
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  // Context menu related members.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_contents_;
  std::unique_ptr<ViewsTextServicesContextMenu> text_services_context_menu_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // View containing the text cursor.
  raw_ptr<View> cursor_view_ = nullptr;

#if BUILDFLAG(IS_MAC)
  // Used to track active password input sessions.
  std::unique_ptr<ui::ScopedPasswordInputEnabler> password_input_enabler_;
#endif  // BUILDFLAG(IS_MAC)

  // How this textfield was focused.
  ui::TextInputClient::FocusReason focus_reason_ =
      ui::TextInputClient::FOCUS_REASON_NONE;

  // The password char reveal index, for testing only.
  std::optional<size_t> password_char_reveal_index_;

  // Extra insets, useful to make room for a button for example.
  gfx::Insets extra_insets_ = gfx::Insets();

  // Whether the client forces a specific text directionality for this
  // textfield, which should inhibit the user's ability to control the
  // directionality.
  bool force_text_directionality_ = false;

  // Helper flag that tracks whether SetBorder was called with a custom
  // border.
  bool use_default_border_ = true;

  // Flag to set whether a background is created for this view.
  bool is_background_enabled_ = true;

  bool is_processing_focus_ = false;

  // Holds the subscription object for the enabled changed callback.
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&Textfield::OnEnabledChanged,
                              base::Unretained(this)));

  base::WeakPtrFactory<Textfield> weak_ptr_factory_{this};

  // Used to bind drop callback functions to this object.
  base::WeakPtrFactory<Textfield> drop_weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Textfield, View)
VIEW_BUILDER_PROPERTY(SkColor, BackgroundColor)
VIEW_BUILDER_PROPERTY(bool, BackgroundEnabled)
VIEW_BUILDER_PROPERTY(TextfieldController*, Controller)
VIEW_BUILDER_PROPERTY(bool, CursorEnabled)
VIEW_BUILDER_PROPERTY(int, DefaultWidthInChars)
VIEW_BUILDER_PROPERTY(gfx::FontList, FontList)
VIEW_BUILDER_PROPERTY(gfx::HorizontalAlignment, HorizontalAlignment)
VIEW_BUILDER_PROPERTY(bool, Invalid)
VIEW_BUILDER_PROPERTY(int, MinimumWidthInChars)
VIEW_BUILDER_PROPERTY(std::u16string, PlaceholderText)
VIEW_BUILDER_PROPERTY(bool, ReadOnly)
VIEW_BUILDER_PROPERTY(gfx::Range, SelectedRange)
VIEW_BUILDER_PROPERTY(SkColor, SelectionBackgroundColor)
VIEW_BUILDER_PROPERTY(SkColor, SelectionTextColor)
VIEW_BUILDER_PROPERTY(std::u16string, Text)
VIEW_BUILDER_PROPERTY(SkColor, TextColor)
VIEW_BUILDER_PROPERTY(int, TextInputFlags)
VIEW_BUILDER_PROPERTY(ui::TextInputType, TextInputType)
VIEW_BUILDER_PROPERTY(bool, UseDefaultBorder)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Textfield)

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
