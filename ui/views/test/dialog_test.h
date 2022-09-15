// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_DIALOG_TEST_H_
#define UI_VIEWS_TEST_DIALOG_TEST_H_

namespace views {

class Widget;

namespace test {

void AcceptDialog(Widget* widget);
void CancelDialog(Widget* widget);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_DIALOG_TEST_H_
