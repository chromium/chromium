// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Possible reasons why a plaintext password was requested.
enum PlaintextReason {
  // The user wants to view the password.
  "VIEW",
  // The user wants to copy the password.
  "COPY",
  // The user wants to edit the password.
  "EDIT"
};

enum ExportProgressStatus {
  // No export was started.
  "NOT_STARTED",
  // Data is being written to the destination.
  "IN_PROGRESS",
  // Data has been written.
  "SUCCEEDED",
  // The user rejected the file selection prompts.
  "FAILED_CANCELLED",
  // Writing to the destination failed.
  "FAILED_WRITE_FAILED"
};

enum CompromiseType {
  // If the credentials was leaked by a data breach.
  "LEAKED",
  // If the credentials was reused on a phishing site.
  "PHISHED",
  // If the credentials have a password which was reused by other credentials.
  "REUSED",
  // If the credentials have a weak password.
  "WEAK"
};

enum PasswordStoreSet {
  // Corresponds to profile-scoped password store.
  "DEVICE",
  // Corresponds to Gaia-account-scoped password store (i.e. account store).
  "ACCOUNT",
  // Corresponds to both profile-scoped and Gaia-account-scoped password
  // stores.
  "DEVICE_AND_ACCOUNT"
};

enum PasswordCheckState {
  // idle state, e.g. after successfully running to completion.
  "IDLE",
  // Running, following an explicit user action to start the check.
  "RUNNING",
  // Canceled, entered when the user explicitly cancels a check.
  "CANCELED",
  // Offline, the user is offline.
  "OFFLINE",
  // The user is not signed into Chrome.
  "SIGNED_OUT",
  // The user does not have any passwords saved.
  "NO_PASSWORDS",
  // The user hit the quota limit.
  "QUOTA_LIMIT",
  // Any other error state.
  "OTHER_ERROR"
};

enum ImportResultsStatus {
  // Any other error state.
  "UNKNOWN_ERROR",
  // Data was fully or partially imported.
  "SUCCESS",
  // Failed to read provided file.
  "IO_ERROR",
  // Header is missing, invalid or could not be read.
  "BAD_FORMAT",
  // File selection dismissed.
  "DISMISSED",
  // Size of the chosen file exceeds the limit.
  "MAX_FILE_SIZE",
  // User has already started the import flow in a different window.
  "IMPORT_ALREADY_ACTIVE",
  // User tried to import too many passwords from one file.
  "NUM_PASSWORDS_EXCEEDED",
  // Conflicts found and they need to be resolved by the user.
  "CONFLICTS"
};

enum ImportEntryStatus {
  // Any other error state.
  "UNKNOWN_ERROR",
  // Missing password field.
  "MISSING_PASSWORD",
  // Missing url field.
  "MISSING_URL",
  // Bad url formatting.
  "INVALID_URL",
  // URL contains non-ASCII chars.
  "NON_ASCII_URL",
  // URL is too long.
  "LONG_URL",
  // Password is too long.
  "LONG_PASSWORD",
  // Username is too long.
  "LONG_USERNAME",
  // Credential is already stored in profile store.
  "CONFLICT_PROFILE",
  // Credential is already stored in account store.
  "CONFLICT_ACCOUNT",
  // Note is too long.
  "LONG_NOTE",
  // Concatenation of imported and local notes is too long.
  "LONG_CONCATENATED_NOTE",
  // Valid credential.
  "VALID"
};

enum FamilyFetchStatus {
  // Unknown or network error.
  "UNKNOWN_ERROR",
  // No family members found.
  "NO_MEMBERS",
  // At least one family member found.
  "SUCCESS"
};

enum PasswordManagerActionableError {
  "NO_ERROR",
  "INACTIONABLE",
  "INACTIONABLE_TEMPORARY_ERROR",
  "SIGN_IN_NEEDED",
  "KEYCHAIN_ERROR",
  "TRUSTED_VAULT_KEY_NEEDED",
  "NEEDS_PASSPHRASE"
};

dictionary PublicKey {
  // The value of the public key.
  required DOMString value;
  // The version of the public key.
  required long version;
};

dictionary RecipientInfo {
  // User ID of the recipient.
  required DOMString userId;
  // Email of the recipient.
  required DOMString email;
  // Name of the recipient.
  required DOMString displayName;
  // Profile image URL of the recipient.
  required DOMString profileImageUrl;
  // Whether the user can receive passwords.
  required boolean isEligible;
  // The public key of the recipient.
  PublicKey publicKey;
};

dictionary FamilyFetchResults {
  // Status of the family members fetch.
  required FamilyFetchStatus status;
  // List of family members.
  required sequence<RecipientInfo> familyMembers;
};

dictionary ImportEntry {
  // The parsing status of individual row that represents
  // credential during import process.
  required ImportEntryStatus status;
  // The url of the credential.
  required DOMString url;
  // The username of the credential.
  required DOMString username;
  // The password of the credential.
  required DOMString password;
  // Unique identifier of the credential.
  required long id;
};

dictionary ImportResults {
  // General status of the triggered passwords import process.
  required ImportResultsStatus status;
  // Number of successfully imported passwords.
  required long numberImported;
  // Possibly emtpy, list of credentials that couldn't be imported.
  required sequence<ImportEntry> displayedEntries;
  // Name of file that user has chosen for the import.
  required DOMString fileName;
};

dictionary UrlCollection {
  // The signon realm of the credential.
  required DOMString signonRealm;

  // A human readable version of the URL of the credential's origin. For
  // android credentials this is usually App name.
  required DOMString shown;

  // The URL that will be linked to when an entry is clicked.
  required DOMString link;
};

// Information specific to compromised credentials.
dictionary CompromisedInfo {
  // The timestamp of when this credential was determined to be compromised.
  // Specified in milliseconds since the UNIX epoch. Intended to be passed to
  // the JavaScript Date() constructor.
  required double compromiseTime;

  // The elapsed time since this credential was determined to be compromised.
  // This is passed as an already formatted string, since JavaScript lacks the
  // required formatting APIs. Example: "5 minutes ago"
  required DOMString elapsedTimeSinceCompromise;

  // The types of credential issues.
  required sequence<CompromiseType> compromiseTypes;

  // Indicates whether this credential is muted.
  required boolean isMuted;
};

// Structure which hold required information to display a link.
dictionary DomainInfo {
  // A human readable version of the URL of the credential's origin. For
  // android credentials this is usually the app name.
  required DOMString name;

  // The URL that will be linked to when an entry is clicked.
  required DOMString url;

  // The signon_realm of corresponding PasswordForm.
  required DOMString signonRealm;
};

// Entry that carries the value and the creation timestamp of a recovery
// password.
dictionary BackupPasswordInfo {
  // The value of the backup password.
  required DOMString value;

  // Internationalized date on which the backup password was created.
  // e.g. Mar 17.
  required DOMString creationDate;
};

// Entry used to display a password in the settings UI.
dictionary PasswordUiEntry {
  // The URL collection corresponding to this saved password entry.
  required sequence<DomainInfo> affiliatedDomains;

  // The username used in conjunction with the saved password.
  required DOMString username;

  // If this is a passkey, the user's display name. Empty otherwise.
  DOMString displayName;

  // The password of the credential. Empty by default, only set if explicitly
  // requested.
  DOMString password;

  // Recovery password for the password change flow.
  BackupPasswordInfo backupPassword;

  // Text shown if the password was obtained via a federated identity.
  DOMString federationText;

  // An index to refer back to a unique password entry record.
  required long id;

  // Corresponds to where the credential is stored.
  required PasswordStoreSet storedIn;

  // Indicates whether this credential is a passkey.
  required boolean isPasskey;

  // The value of the attached note.
  DOMString note;

  // The URL where the insecure password can be changed. Might be not set for
  // Android apps.
  DOMString changePasswordUrl;

  // Indicates whether automatic password change is supported for this
  // credential.
  required boolean isAutomaticPasswordChangeSupported;

  // Additional information in case a credential is compromised.
  CompromisedInfo compromisedInfo;

  // The timestamp of when this credential was created, or undefined if not a
  // passkey. Specified in milliseconds since the UNIX epoch. Intended to be
  // passed to the JavaScript Date() constructor.
  double creationTime;

  // Indicates that the credential was marked for deletion (e.g. by a website)
  // and should be marked as such in management surfaces. Used for passkeys
  // only. Always false for other credential types.
  required boolean hidden;
};

// Group representing affiliated PasswordUiEntries.
dictionary CredentialGroup {
  // Group name being displayed.
  required DOMString name;

  // Icon url for the given group.
  required DOMString iconUrl;

  // Entries in the group.
  required sequence<PasswordUiEntry> entries;
};

dictionary ExceptionEntry {
  // The URL collection corresponding to this exception entry.
  required UrlCollection urls;

  // An id to refer back to a unique exception entry record.
  required long id;
};

dictionary PasswordExportProgress {
  // The current status of the export task.
  required ExportProgressStatus status;

  // If |status| is $ref(ExportProgressStatus.SUCCEEDED), this will
  // be the full path of the written file.
  DOMString filePath;

  // If |status| is $ref(ExportProgressStatus.FAILED_WRITE_FAILED), this will
  // be the name of the selected folder to export to.
  DOMString folderName;
};

// Object describing the current state of the password check. The check could
// be in any of the above described states.
dictionary PasswordCheckStatus {
  // The state of the password check.
  required PasswordCheckState state;

  // Total number of saved passwords.
  long totalNumberOfPasswords;

  // How many passwords have already been processed. Populated if and only if
  // the password check is currently running.
  long alreadyProcessed;

  // How many passwords are remaining in the queue. Populated if and only if
  // the password check is currently running.
  long remainingInQueue;

  // The elapsed time since the last full password check was performed. This
  // is passed as a string, since JavaScript lacks the required formatting
  // APIs. If no check has been performed yet this is not set.
  DOMString elapsedTimeSinceLastCheck;
};

// Object describing a password entry to be saved and storage to be used.
dictionary AddPasswordOptions {
  // The url to save the password for.
  required DOMString url;

  // The username to save the password for.
  required DOMString username;

  // The password value to be saved.
  required DOMString password;

  // The note attached the password.
  required DOMString note;

  // True for account store, false for device store.
  required boolean useAccountStore;
};

// An object holding an array of PasswordUiEntries.
dictionary PasswordUiEntryList {
  required sequence<PasswordUiEntry> entries;
};

// |entries|: The updated list of password entries.
callback OnSavedPasswordsListChangedListener = undefined (
    sequence<PasswordUiEntry> entries);

interface OnSavedPasswordsListChangedEvent : ExtensionEvent {
  static undefined addListener(OnSavedPasswordsListChangedListener listener);
  static undefined removeListener(OnSavedPasswordsListChangedListener listener);
  static boolean hasListener(OnSavedPasswordsListChangedListener listener);
};

// |exceptions|: The updated list of password exceptions.
callback OnPasswordExceptionsListChangedListener = undefined (
    sequence<ExceptionEntry> exceptions);

interface OnPasswordExceptionsListChangedEvent : ExtensionEvent {
  static undefined addListener(
      OnPasswordExceptionsListChangedListener listener);
  static undefined removeListener(
      OnPasswordExceptionsListChangedListener listener);
  static boolean hasListener(OnPasswordExceptionsListChangedListener listener);
};

// |status|: The progress status and an optional UI message.
callback OnPasswordsFileExportProgressListener = undefined (
    PasswordExportProgress status);

interface OnPasswordsFileExportProgressEvent : ExtensionEvent {
  static undefined addListener(OnPasswordsFileExportProgressListener listener);
  static undefined removeListener(
      OnPasswordsFileExportProgressListener listener);
  static boolean hasListener(OnPasswordsFileExportProgressListener listener);
};

// |enabled|: The new active state.
callback OnAccountStorageActiveStateChangedListener = undefined (
    boolean enabled);

interface OnAccountStorageActiveStateChangedEvent : ExtensionEvent {
  static undefined addListener(
      OnAccountStorageActiveStateChangedListener listener);
  static undefined removeListener(
      OnAccountStorageActiveStateChangedListener listener);
  static boolean hasListener(
      OnAccountStorageActiveStateChangedListener listener);
};


// |insecureCredentials|: The updated insecure credentials.
callback OnInsecureCredentialsChangedListener = undefined (
    sequence<PasswordUiEntry> insecureCredentials);

interface OnInsecureCredentialsChangedEvent : ExtensionEvent {
  static undefined addListener(OnInsecureCredentialsChangedListener listener);
  static undefined removeListener(
      OnInsecureCredentialsChangedListener listener);
  static boolean hasListener(OnInsecureCredentialsChangedListener listener);
};

// |status|: The updated status of the password check.
callback OnPasswordCheckStatusChangedListener = undefined (
    PasswordCheckStatus status);

interface OnPasswordCheckStatusChangedEvent : ExtensionEvent {
  static undefined addListener(OnPasswordCheckStatusChangedListener listener);
  static undefined removeListener(
      OnPasswordCheckStatusChangedListener listener);
  static boolean hasListener(OnPasswordCheckStatusChangedListener listener);
};

callback OnPasswordManagerAuthTimeoutListener = undefined ();

interface OnPasswordManagerAuthTimeoutEvent : ExtensionEvent {
  static undefined addListener(OnPasswordManagerAuthTimeoutListener listener);
  static undefined removeListener(
      OnPasswordManagerAuthTimeoutListener listener);
  static boolean hasListener(OnPasswordManagerAuthTimeoutListener listener);
};

callback OnPasswordManagerActionableErrorChangedListener = undefined (
    PasswordManagerActionableError error);

interface OnPasswordManagerActionableErrorChangedEvent : ExtensionEvent {
  static undefined addListener(
      OnPasswordManagerActionableErrorChangedListener listener);
  static undefined removeListener(
      OnPasswordManagerActionableErrorChangedListener listener);
  static boolean hasListener(
      OnPasswordManagerActionableErrorChangedListener listener);
};

// Use the <code>chrome.passwordsPrivate</code> API to add or remove password
// data from the settings UI.
interface PasswordsPrivate {
  // Function that logs that the Passwords page was accessed from the Chrome
  // Settings WebUI.
  static undefined recordPasswordsPageAccessInSettings();

  // Changes the credential. Not all attributes can be updated.
  // Optional attributes that are not set will be unchanged.
  // Returns a promise that resolves if successful, and rejects otherwise.
  // |credential|: The credential to update. This will be matched to the
  // existing credential by id.
  static Promise<undefined> changeCredential(PasswordUiEntry credential);

  // Removes the credential corresponding to |id| in |fromStores|. If no
  // credential for this pair exists, this function is a no-op.
  // |id|: The id for the credential being removed.
  // |fromStores|: The store(s) from which the credential is being removed.
  static undefined removeCredential(long id, PasswordStoreSet fromStores);

  // Removes the saved password exception corresponding to |id|. If
  // no exception with this id exists, this function is a no-op. This will
  // remove exception from both stores.
  // |id|: The id for the exception url entry is being removed.
  static undefined removePasswordException(long id);

  // Undoes the last removal of saved password(s) or exception(s).
  static undefined undoRemoveSavedPasswordOrException();

  // Returns the plaintext password corresponding to |id|. Note that on
  // some operating systems, this call may result in an OS-level
  // reauthentication. Once the password has been fetched, it will be returned
  // via |callback|.
  // |id|: The id for the password entry being being retrieved.
  // |reason|: The reason why the plaintext password is requested.
  // |Returns|: The callback that gets invoked with the retrieved password.
  // |PromiseValue|: password
  static Promise<DOMString> requestPlaintextPassword(
      long id,
      PlaintextReason reason);

  // Returns the PasswordUiEntries (with |password|, |note| field filled)
  // corresponding to |ids|. Note that on some operating systems, this call may
  // result in an OS-level reauthentication. Once the PasswordUiEntry has been
  // fetched, it will be returned via |callback|.
  // |ids|: Ids for the password entries being retrieved.
  // |Returns|: The callback that gets invoked with the retrieved
  // PasswordUiEntries.
  // |PromiseValue|: entries
  static Promise<sequence<PasswordUiEntry>> requestCredentialsDetails(
      sequence<long> ids);

  // Returns the list of saved passwords.
  // |Returns|: Called with the list of saved passwords.
  // |PromiseValue|: entries
  static Promise<sequence<PasswordUiEntry>> getSavedPasswordList();

  // Returns the list of Credential groups.
  // |Returns|: Called with the list of groups.
  // |PromiseValue|: entries
  static Promise<sequence<CredentialGroup>> getCredentialGroups();

  // Returns the list of password exceptions.
  // |Returns|: Called with the list of password exceptions.
  // |PromiseValue|: exceptions
  static Promise<sequence<ExceptionEntry>> getPasswordExceptionList();

  // Moves passwords currently stored on the device to being stored in the
  // signed-in, non-syncing Google Account. For each id, the result is a
  // no-op if any of these is true: |id| is invalid; |id| corresponds to a
  // password already stored in the account; or the user is not using the
  // account-scoped password storage.
  // |ids|: The ids for the password entries being moved.
  static undefined movePasswordsToAccount(sequence<long> ids);

  // Fetches family members (password share recipients).
  // |PromiseValue|: results
  static Promise<FamilyFetchResults> fetchFamilyMembers();

  // Sends sharing invitations to the recipients.
  // |id|: The id of the password entry to be shared.
  // |recipients|: The list of selected recipients.
  // |Returns|: The callback that gets invoked on success.
  static Promise<undefined> sharePassword(
      long id,
      sequence<RecipientInfo> recipients);

  // Triggers the Password Manager password import functionality.
  // |PromiseValue|: results
  static Promise<ImportResults> importPasswords(PasswordStoreSet toStore);

  // Resumes the password import process when user has selected which
  // passwords to replace.
  // |selectedIds|: The ids of passwords that need to be replaced.
  // |PromiseValue|: results
  static Promise<ImportResults> continueImport(sequence<long> selectedIds);

  // Resets the PasswordImporter if it is in the CONFLICTS/FINISHED state
  // and the user closes the dialog. Only when the PasswordImporter is in
  // FINISHED state, |deleteFile| option is taken into account.
  // |deleteFile|: Whether to trigger deletion of the last imported file.
  static Promise<undefined> resetImporter(boolean deleteFile);

  // Triggers the Password Manager password export functionality. Completion
  // Will be signaled by the onPasswordsFileExportProgress event.
  // |callback| will be called when the request is started or rejected. If
  // rejected $(ref:runtime.lastError) will be set to
  // <code>'in-progress'</code> or <code>'reauth-failed'</code>.
  static Promise<undefined> exportPasswords();

  // Requests the export progress status. This is the same as the last value
  // seen on the onPasswordsFileExportProgress event. This function is useful
  // for checking if an export has already been initiated from an older tab,
  // where we might have missed the original event.
  // |PromiseValue|: status
  static Promise<ExportProgressStatus> requestExportProgressStatus();

  // Requests the latest insecure credentials.
  // |PromiseValue|: entries
  static Promise<sequence<PasswordUiEntry>> getInsecureCredentials();

  // Requests group of credentials which reuse passwords. Each group contains
  // credentials with the same password value.
  // |PromiseValue|: entries
  static Promise<sequence<PasswordUiEntryList>>
      getCredentialsWithReusedPassword();

  // Requests to mute |credential| from the password store.
  // Invokes |callback| on completion.
  static Promise<undefined> muteInsecureCredential(PasswordUiEntry credential);

  // Requests to unmute |credential| from the password store.
  // Invokes |callback| on completion.
  static Promise<undefined> unmuteInsecureCredential(
      PasswordUiEntry credential);

  // Starts a check for insecure passwords. Invokes |callback| on completion.
  static Promise<undefined> startPasswordCheck();

  // Returns the current status of the check via |callback|.
  // |PromiseValue|: status
  static Promise<PasswordCheckStatus> getPasswordCheckStatus();

  // Requests whether the given |url| meets the requirements to save a
  // password for it (e.g. valid, has proper scheme etc.) and returns the
  // corresponding URLCollection on success. Otherwise it raises an error.
  // |PromiseValue|: urlCollection
  static Promise<UrlCollection> getUrlCollection(DOMString url);

  // Saves a new password entry described by the given |options|. Invokes
  // |callback| or raises an error depending on whether the operation
  // succeeded.
  // |options|: Details about a new password and storage to be used.
  // |Returns|: The callback that gets invoked on success.
  static Promise<undefined> addPassword(AddPasswordOptions options);

  // Opens a file with exported passwords in the OS shell.
  static undefined showExportedFileInShell(DOMString file_path);

  // Disconnects the Chrome client from the cloud authenticator.
  // |PromiseValue|: success
  static Promise<boolean> disconnectCloudAuthenticator();

  // Checks whether the Chrome client is registered with/connected to
  // the cloud authenticator.
  // |PromiseValue|: connected
  static Promise<boolean> isConnectedToCloudAuthenticator();

  // Fired when the saved passwords list has changed, meaning that an entry
  // has been added or removed.
  static attribute OnSavedPasswordsListChangedEvent onSavedPasswordsListChanged;

  // Fired when the password exceptions list has changed, meaning that an
  // entry has been added or removed.
  static attribute OnPasswordExceptionsListChangedEvent
      onPasswordExceptionsListChanged;

  // Fired when the status of the export has changed.
  static attribute OnPasswordsFileExportProgressEvent
      onPasswordsFileExportProgress;

  // Fired when the active state for the account-scoped storage has changed.
  static attribute OnAccountStorageActiveStateChangedEvent
      onAccountStorageActiveStateChanged;


  // Fired when the insecure credentials changed.
  static attribute OnInsecureCredentialsChangedEvent
      onInsecureCredentialsChanged;

  // Fired when the status of the password check changes.
  static attribute OnPasswordCheckStatusChangedEvent
      onPasswordCheckStatusChanged;

  // Fired when the password manager access timed out.
  static attribute OnPasswordManagerAuthTimeoutEvent
      onPasswordManagerAuthTimeout;

  // Fired when the password manager actionable error changed.
  static attribute OnPasswordManagerActionableErrorChangedEvent
      onPasswordManagerActionableErrorChanged;
};

partial interface Browser {
  static attribute PasswordsPrivate passwordsPrivate;
};
