// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

/** Classes that hold an instance of {@link ModalDialogManager} should implement this interface. */
public interface ModalDialogManagerHolder {
    /**
     * @return The {@link ModalDialogManager} associated with this class
     */
    ModalDialogManager getModalDialogManager();
}
