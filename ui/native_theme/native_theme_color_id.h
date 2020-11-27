// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_
#define UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_

#include "build/chromeos_buildflags.h"

// Clang format mangles sectioned lists like the below badly.
// clang-format off
#define NATIVE_THEME_CROSS_PLATFORM_COLOR_IDS                                  \
  /* Windows */                                                                \
  OP(kColorId_WindowBackground),                                               \
  /* Dialogs */                                                                \
  OP(kColorId_DialogBackground),                                               \
  OP(kColorId_DialogForeground),                                               \
  OP(kColorId_BubbleBackground),                                               \
  OP(kColorId_BubbleFooterBackground),                                         \
  /* Avatar and Header Asset */                                                \
  OP(kColorId_AvatarHeaderArt),                                                \
  OP(kColorId_AvatarIconGuest),                                                \
  OP(kColorId_AvatarIconIncognito),                                            \
  /* FocusableBorder */                                                        \
  OP(kColorId_FocusedBorderColor),                                             \
  OP(kColorId_UnfocusedBorderColor),                                           \
  /* Button */                                                                 \
  OP(kColorId_ButtonColor),                                                    \
  OP(kColorId_ButtonBorderColor),                                              \
  OP(kColorId_DisabledButtonBorderColor),                                      \
  OP(kColorId_ButtonCheckedColor),                                             \
  OP(kColorId_ButtonUncheckedColor),                                           \
  OP(kColorId_ButtonEnabledColor),                                             \
  OP(kColorId_ButtonDisabledColor),                                            \
  OP(kColorId_ButtonHoverColor),                                               \
  OP(kColorId_ButtonInkDropFillColor),                                         \
  OP(kColorId_ButtonInkDropShadowColor),                                       \
  OP(kColorId_ProminentButtonColor),                                           \
  OP(kColorId_ProminentButtonDisabledColor),                                   \
  OP(kColorId_ProminentButtonFocusedColor),                                    \
  OP(kColorId_ProminentButtonHoverColor),                                      \
  OP(kColorId_ProminentButtonInkDropShadowColor),                              \
  OP(kColorId_ProminentButtonInkDropFillColor),                                \
  OP(kColorId_TextOnProminentButtonColor),                                     \
  OP(kColorId_PaddedButtonInkDropColor),                                       \
  /* ToggleButton */                                                           \
  OP(kColorId_ToggleButtonShadowColor),                                        \
  OP(kColorId_ToggleButtonTrackColorOff),                                      \
  OP(kColorId_ToggleButtonTrackColorOn),                                       \
  /* MenuItem */                                                               \
  OP(kColorId_EnabledMenuItemForegroundColor),                                 \
  OP(kColorId_DisabledMenuItemForegroundColor),                                \
  OP(kColorId_SelectedMenuItemForegroundColor),                                \
  OP(kColorId_FocusedMenuItemBackgroundColor),                                 \
  OP(kColorId_MenuDropIndicator),                                              \
  OP(kColorId_MenuItemMinorTextColor),                                         \
  OP(kColorId_MenuSeparatorColor),                                             \
  OP(kColorId_MenuBackgroundColor),                                            \
  OP(kColorId_MenuBorderColor),                                                \
  /* Colors for icons displayed in a menu context. */                          \
  OP(kColorId_MenuIconColor),                                                  \
  OP(kColorId_HighlightedMenuItemBackgroundColor),                             \
  OP(kColorId_HighlightedMenuItemForegroundColor),                             \
  OP(kColorId_MenuItemInitialAlertBackgroundColor),                            \
  OP(kColorId_MenuItemTargetAlertBackgroundColor),                             \
  /* Custom frame view */                                                      \
  OP(kColorId_CustomFrameActiveColor),                                         \
  OP(kColorId_CustomFrameInactiveColor),                                       \
  /* Custom tab bar */                                                         \
  OP(kColorId_CustomTabBarBackgroundColor),                                    \
  OP(kColorId_CustomTabBarForegroundColor),                                    \
  OP(kColorId_CustomTabBarSecurityChipDangerousColor),                         \
  OP(kColorId_CustomTabBarSecurityChipDefaultColor),                           \
  OP(kColorId_CustomTabBarSecurityChipSecureColor),                            \
  OP(kColorId_CustomTabBarSecurityChipWithCertColor),                          \
  /* Dropdown */                                                               \
  OP(kColorId_DropdownBackgroundColor),                                        \
  OP(kColorId_DropdownForegroundColor),                                        \
  OP(kColorId_DropdownSelectedBackgroundColor),                                \
  OP(kColorId_DropdownSelectedForegroundColor),                                \
  /* Label */                                                                  \
  OP(kColorId_LabelEnabledColor),                                              \
  OP(kColorId_LabelDisabledColor),                                             \
  OP(kColorId_LabelSecondaryColor),                                            \
  OP(kColorId_LabelTextSelectionColor),                                        \
  OP(kColorId_LabelTextSelectionBackgroundFocused),                            \
  /* Link */                                                                   \
  OP(kColorId_LinkDisabled),                                                   \
  OP(kColorId_LinkEnabled),                                                    \
  OP(kColorId_LinkPressed),                                                    \
  OP(kColorId_OverlayScrollbarThumbBackground),                                \
  OP(kColorId_OverlayScrollbarThumbForeground),                                \
  /* Notification view */                                                      \
  OP(kColorId_NotificationDefaultBackground),                                  \
  OP(kColorId_NotificationActionsRowBackground),                               \
  OP(kColorId_NotificationInlineSettingsBackground),                           \
  OP(kColorId_NotificationLargeImageBackground),                               \
  OP(kColorId_NotificationPlaceholderIconColor),                               \
  OP(kColorId_NotificationEmptyPlaceholderIconColor),                          \
  OP(kColorId_NotificationEmptyPlaceholderTextColor),                          \
  OP(kColorId_NotificationDefaultAccentColor),                                 \
  OP(kColorId_NotificationInkDropBase),                                        \
  /* Slider */                                                                 \
  OP(kColorId_SliderThumbDefault),                                             \
  OP(kColorId_SliderTroughDefault),                                            \
  OP(kColorId_SliderThumbMinimal),                                             \
  OP(kColorId_SliderTroughMinimal),                                            \
  /* Separator */                                                              \
  OP(kColorId_SeparatorColor),                                                 \
  /* Sync info container */                                                    \
  OP(kColorId_SyncInfoContainerPaused),                                        \
  OP(kColorId_SyncInfoContainerError),                                         \
  OP(kColorId_SyncInfoContainerNoPrimaryAccount),                              \
  /* TabbedPane */                                                             \
  OP(kColorId_TabTitleColorActive),                                            \
  OP(kColorId_TabTitleColorInactive),                                          \
  OP(kColorId_TabBottomBorder),                                                \
  OP(kColorId_TabHighlightBackground),                                         \
  OP(kColorId_TabHighlightFocusedBackground),                                  \
  OP(kColorId_TabSelectedBorderColor),                                         \
  /* Textfield */                                                              \
  OP(kColorId_TextfieldDefaultColor),                                          \
  OP(kColorId_TextfieldDefaultBackground),                                     \
  OP(kColorId_TextfieldPlaceholderColor),                                      \
  OP(kColorId_TextfieldReadOnlyColor),                                         \
  OP(kColorId_TextfieldReadOnlyBackground),                                    \
  OP(kColorId_TextfieldSelectionColor),                                        \
  OP(kColorId_TextfieldSelectionBackgroundFocused),                            \
  /* Tooltip */                                                                \
  OP(kColorId_TooltipBackground),                                              \
  OP(kColorId_TooltipIcon),                                                    \
  OP(kColorId_TooltipIconHovered),                                             \
  OP(kColorId_TooltipText),                                                    \
  /* Tree */                                                                   \
  OP(kColorId_TreeBackground),                                                 \
  OP(kColorId_TreeText),                                                       \
  OP(kColorId_TreeSelectedText),                                               \
  OP(kColorId_TreeSelectedTextUnfocused),                                      \
  OP(kColorId_TreeSelectionBackgroundFocused),                                 \
  OP(kColorId_TreeSelectionBackgroundUnfocused),                               \
  /* Table */                                                                  \
  OP(kColorId_TableBackground),                                                \
  OP(kColorId_TableBackgroundAlternate),                                       \
  OP(kColorId_TableText),                                                      \
  OP(kColorId_TableSelectedText),                                              \
  OP(kColorId_TableSelectedTextUnfocused),                                     \
  OP(kColorId_TableSelectionBackgroundFocused),                                \
  OP(kColorId_TableSelectionBackgroundUnfocused),                              \
  OP(kColorId_TableGroupingIndicatorColor),                                    \
  /* Table Header */                                                           \
  OP(kColorId_TableHeaderText),                                                \
  OP(kColorId_TableHeaderBackground),                                          \
  OP(kColorId_TableHeaderSeparator),                                           \
  /* Colors for the material spinner (aka throbber). */                        \
  OP(kColorId_ThrobberSpinningColor),                                          \
  OP(kColorId_ThrobberWaitingColor),                                           \
  OP(kColorId_ThrobberLightColor),                                             \
  /* Colors for Bubble Border */                                               \
  OP(kColorId_BubbleBorder),                                                   \
  /* Colors for Footnote Container. */                                         \
  OP(kColorId_FootnoteContainerBorder),                                        \
  /* Colors for icons that alert, e.g. upgrade reminders. */                   \
  OP(kColorId_AlertSeverityLow),                                               \
  OP(kColorId_AlertSeverityMedium),                                            \
  OP(kColorId_AlertSeverityHigh),                                              \
  /* Colors for icons in non-menu contexts. */                                 \
  OP(kColorId_DefaultIconColor),                                               \
  OP(kColorId_DisabledIconColor)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#define NATIVE_THEME_CHROMEOS_COLOR_IDS                                        \
  /* Notification view */                                                      \
  OP(kColorId_NotificationButtonBackground)
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#define NATIVE_THEME_COLOR_IDS                                                 \
  NATIVE_THEME_CROSS_PLATFORM_COLOR_IDS,                                       \
  NATIVE_THEME_CHROMEOS_COLOR_IDS
#else
#define NATIVE_THEME_COLOR_IDS NATIVE_THEME_CROSS_PLATFORM_COLOR_IDS
#endif

// clang-format on

#endif  // UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_
