// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_API_H_
#define UI_VIEWS_TEST_WIDGET_TEST_API_H_

namespace views {

// Makes Widget::OnNativeWidgetActivationChanged return false, which prevents
// handling of the corresponding event (if the native widget implementation
// takes this into account).
void DisableActivationChangeHandlingForTests();

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_API_H_
