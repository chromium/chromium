// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.AndroidRuntimeException;
import android.webkit.ValueCallback;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.contacts_picker.ContactDetails;
import org.chromium.components.browser_ui.contacts_picker.PickerAdapter;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;

/**
 * A {@link PickerAdapter} for WebLayer.
 * This class synthesizes the self contact based on data provided by the client.
 */
public class ContactsPickerAdapter extends PickerAdapter {
    private final WindowAndroid mWindowAndroid;

    // The avatar returned by the client, or null if it hasn't been returned.
    private Bitmap mAvatar;

    // True after the self contact has been synthesized and prepended to the contact list.
    private boolean mSelfContactSynthesized;

    ContactsPickerAdapter(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    // PickerAdapter:

    @Override
    protected String findOwnerEmail() {
        IUserIdentityCallbackClient identityCallback = getUserIdentityCallback();
        if (identityCallback == null) return null;

        String email;
        try {
            email = identityCallback.getEmail();
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
        return TextUtils.isEmpty(email) ? null : email;
    }

    @Override
    protected void addOwnerInfoToContacts(ArrayList<ContactDetails> contacts) {
        // This method should only be called if there was a valid email returned in
        // findOwnerEmail().
        IUserIdentityCallbackClient identityCallback = getUserIdentityCallback();
        assert identityCallback != null;

        String name;
        // Weak ref so that the outstanding callback doesn't hold a ref to |this|.
        final WeakReference<ContactsPickerAdapter> weakThis =
                new WeakReference<ContactsPickerAdapter>(this);
        try {
            name = identityCallback.getFullName();
            ValueCallback<Bitmap> onAvatarLoaded = (Bitmap returnedAvatar) -> {
                ContactsPickerAdapter strongThis = weakThis.get();
                if (strongThis == null) return;

                strongThis.updateOwnerInfoWithIcon(returnedAvatar);
            };
            identityCallback.getAvatar(getIconRawPixelSize(), ObjectWrapper.wrap(onAvatarLoaded));
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }

        if (TextUtils.isEmpty(name)) {
            name = getOwnerEmail();
        }

        ContactDetails contact = new ContactDetails(ContactDetails.SELF_CONTACT_ID, name,
                Collections.singletonList(getOwnerEmail()), /*phoneNumbers=*/null,
                /*addresses=*/null);
        contact.setIsSelf(true);
        contact.setSelfIcon(createAvatarDrawable());
        contacts.add(0, contact);

        mSelfContactSynthesized = true;
    }

    @Nullable
    private IUserIdentityCallbackClient getUserIdentityCallback() {
        return BrowserFragmentImpl.fromWindowAndroid(mWindowAndroid)
                .getBrowser()
                .getProfile()
                .getUserIdentityCallbackClient();
    }

    private void updateOwnerInfoWithIcon(Bitmap icon) {
        if (icon == null) return;

        mAvatar = icon.copy(icon.getConfig(), true);
        mAvatar.setDensity(
                mWindowAndroid.getContext().get().getResources().getConfiguration().densityDpi);

        if (mSelfContactSynthesized) {
            getAllContacts().get(0).setSelfIcon(createAvatarDrawable());
            update();
        }
    }

    private Drawable createAvatarDrawable() {
        if (mAvatar == null) return null;

        Resources res = mWindowAndroid.getContext().get().getResources();
        int sideLength = getIconRawPixelSize();
        return new BitmapDrawable(
                res, Bitmap.createScaledBitmap(mAvatar, sideLength, sideLength, true));
    }

    private int getIconRawPixelSize() {
        Resources res = mWindowAndroid.getContext().get().getResources();
        return res.getDimensionPixelSize(R.dimen.contact_picker_icon_size);
    }
}
