// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.spnegoauthenticator;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.net.HttpNegotiateConstants;

/** Utility class to get and set account data. */
class AccountData {
    private static final String TAG = Constants.TAG;
    private static final String OPT_KEY_AUTH = "isAuthenticated";
    private static final String OPT_VALUE_AUTH = "YES";
    private final Account mAccount;
    private final String mPassword;
    private boolean mIsAuthenticated;

    private AccountData(Account account, boolean isAuthenticated) {
        Log.d(TAG, "AccountData(name=%s, isAuthenticated=%s", account.name, isAuthenticated);
        mAccount = account;
        mIsAuthenticated = isAuthenticated;
        mPassword = "userPass";
    }

    /** Creates some new account data. */
    public static AccountData create(String name, Context context) {
        Account account = new Account(name, context.getString(R.string.account_type));
        boolean isAuthenticated = Constants.ACCOUNT_1_NAME.equals(name);

        return new AccountData(account, isAuthenticated);
    }

    /**
     * Creates a new {@link AccountData} object, looking at previously saved data to
     * initialize it.
     */
    public static AccountData get(String accountName, Context context) {
        Account account = new Account(accountName, context.getString(R.string.account_type));

        AccountManager am = AccountManager.get(context);
        String authValue = am.getUserData(account, OPT_KEY_AUTH);
        boolean isAuthenticated = TextUtils.equals(authValue, OPT_VALUE_AUTH);

        return new AccountData(account, isAuthenticated);
    }

    /**
     * Saves the account data with the AccountManager. If the account did not previously
     * exist, it will be created.
     */
    public void save(Context context) {
        AccountManager am = AccountManager.get(context);

        // Does nothing if the account already exists
        am.addAccountExplicitly(mAccount, mPassword, null);

        am.setUserData(mAccount, OPT_KEY_AUTH, mIsAuthenticated ? OPT_VALUE_AUTH : null);

        // Is supposed to be send by AccountsService when accounts are modified, but it looks like
        // the authenticator has to do it itself.
        context.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));
    }

    /** Returns an intent as expected for answers to {@link SpnegoAuthenticator#addAccount}. */
    public Intent getAccountAddedIntent() {
        Intent intent = new Intent();
        intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, mAccount.name);
        intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE, mAccount.type);
        return intent;
    }

    /** Returns an intent as expected for answers to {@link SpnegoAuthenticator#getAuthToken}. */
    public Intent getCredentialsConfirmedIntent() {
        Intent intent = new Intent();
        intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, mAccount.name);
        intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE, mAccount.type);
        intent.putExtra(AccountManager.KEY_AUTHTOKEN, Constants.AUTH_TOKEN);
        intent.putExtra(HttpNegotiateConstants.KEY_SPNEGO_RESULT, 0);
        return intent;
    }

    public boolean isAuthenticated() {
        return mIsAuthenticated;
    }

    public void setIsAuthenticated(boolean value) {
        mIsAuthenticated = value;
    }

    public Account getAccount() {
        return mAccount;
    }
}
