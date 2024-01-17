/// WebauthnCredentialSpecifics is an entity that backs a WebAuthn
/// PublicKeyCredential. Since it contains the authenticator’s view of this
/// object, it has a private key rather than a public key.
/// (<https://www.w3.org/TR/webauthn-2/#iface-pkcredential>).
///
/// Names of fields are taken from WebAuthn where possible. E.g.
/// user.displayName in WebAuthn becomes user_display_name here.
///
/// All fields are immutable after creation except for user_name and
/// user_display_name, which may be updated by a user.
#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct WebauthnCredentialSpecifics {
    /// Sync's ID for this entity (sometimes called the client unique tag), 16
    /// random bytes. This value is used within Sync to identify this entity.
    /// The credential ID is not used because the (hashed) sync_id is
    /// exposed to the Sync server, and we don’t want Google to be able to
    /// map a credential ID to an account. Password entities construct this
    /// value from the concatenation of many fields and depend on the fact
    /// that the server only sees a hash of it. But the only high-entropy
    /// secret here is the private key, which will have different encryption
    /// in the future, and private keys are not the sort of data to copy
    /// into other fields. Therefore this independent value is provided to
    /// form the client's ID.
    #[prost(bytes = "vec", optional, tag = "1")]
    pub sync_id: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    /// The credential ID, 16 random bytes. This is a value surfaced in
    /// the WebAuthn API (<https://www.w3.org/TR/webauthn-2/#credential-id>).
    #[prost(bytes = "vec", optional, tag = "2")]
    pub credential_id: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    /// An RP ID is a WebAuthn concept:
    /// <https://www.w3.org/TR/webauthn-2/#rp-id.> It’s usually a domain name,
    /// although in non-Web contexts it can be a URL with a non-Web scheme.
    #[prost(string, optional, tag = "3")]
    pub rp_id: ::core::option::Option<::prost::alloc::string::String>,
    /// The user ID, which is also called a “user handle” in WebAuthn
    /// (<https://www.w3.org/TR/webauthn-2/#user-handle>), is an RP-specific
    /// identifier that is up to 64-bytes long. An authenticator conceptually
    /// only stores a single credential for a given (rp_id, user_id) pair,
    /// but there may be several credentials in Sync. They are prioritised
    /// using newly_shadowed_credential_ids and creation_time. See below.
    ///
    /// (We wish to be able to retain several entities for a single (rp_id,
    /// user_id) pair because there’s an edge case where we may wish to revert
    /// to an older entry and thus need to keep the older entry around in
    /// Sync. The revert could happen on a different device too.)
    #[prost(bytes = "vec", optional, tag = "4")]
    pub user_id: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    /// The id of credentials with the same (rp_id, user_id) that were
    /// shadowed by the creation of this entity.
    ///
    /// A credential is shadowed if one or more other credentials (from the same
    /// account, and with the same (rp_id, user_id)) include its credential_id
    /// in their list of shadowed IDs. Shadowed credentials are ignored when
    /// finding a credential to sign with. If there is more than one
    /// candidate remaining after filtering shadowed credentials then the
    /// most recently created (based on creation_time) is used.
    ///
    /// The reason for all this is that sites can replace a credential by
    /// creating another one with the same (rp_id, user_id) pair. However,
    /// we don't immediately know whether the WebAuthn response reached the
    /// website's server. Consider a user with a poor internet connection.
    /// Javascript in the site's origin triggers a credential creation that
    /// “overwrites” an existing credential, but the Javascript is unable to
    /// send the new public key to the website's server. The user is now
    /// locked out: the old credential has been over-written but the
    /// website's server doesn't know about the new one.
    ///
    /// Thus we wish to keep “overwritten” credentials around for a while to
    /// allow for some sort of recovery. In the simple case, a new
    /// credential shadows the single, previous old credential. We could
    /// depend on creation_time, but client clocks aren't always accurate,
    /// thus this field.
    ///
    /// In complicated cases two devices might race to replace a credential, in
    /// which case (after mutual syncing) two candidate credentials exist for
    /// the same (rp_id, user_id) pair because neither shadows the other. In
    /// this case we pick the newest based on |creation_time| but it's quite
    /// possible that some recovery will be needed because the website's
    /// server thinks the other one is correct.
    ///
    /// A generation counter isn't used because a single device might replace a
    /// series of credentials as it tries to update the website's server. But
    /// that doesn't mean that it should dominate a different device that
    /// replaced it only once, but later.
    #[prost(bytes = "vec", repeated, tag = "5")]
    pub newly_shadowed_credential_ids: ::prost::alloc::vec::Vec<::prost::alloc::vec::Vec<u8>>,
    /// The local time on the device when this credential was created. Given in
    /// milliseconds since the UNIX epoch. This is used to break ties between
    /// credentials. See newly_shadowed_credential_ids.
    #[prost(int64, optional, tag = "6")]
    pub creation_time: ::core::option::Option<i64>,
    /// The human-readable account identifier. Usually an email address. This is
    /// mutable.
    /// <https://www.w3.org/TR/webauthn-2/#dom-publickeycredentialentity-name>
    #[prost(string, optional, tag = "7")]
    pub user_name: ::core::option::Option<::prost::alloc::string::String>,
    /// The human-readable name. Usually a legal name. This is mutable.
    /// <https://www.w3.org/TR/webauthn-2/#dom-publickeycredentialuserentity-displayname.>
    #[prost(string, optional, tag = "8")]
    pub user_display_name: ::core::option::Option<::prost::alloc::string::String>,
    /// Credentials may optionally be enabled for Secure Payment
    /// Confirmation\[1\] on third-party sites. This is opt-in at creation
    /// time.
    ///
    /// \[1\] <https://www.w3.org/TR/secure-payment-confirmation/>
    #[prost(bool, optional, tag = "11")]
    pub third_party_payments_support: ::core::option::Option<bool>,
    /// Time when this passkey was last successfully asserted. Number of
    /// microseconds since 1601, aka Windows epoch. This mirrors the
    /// `date_last_used` field in PasswordSpecificsData.
    #[prost(int64, optional, tag = "13")]
    pub last_used_time_windows_epoch_micros: ::core::option::Option<i64>,
    /// The "version" (sometimes called the epoch) of the key that
    /// `encrypted_data` is encrypted with. This allows trial decryption to
    /// be avoided when present.
    #[prost(int32, optional, tag = "14")]
    pub key_version: ::core::option::Option<i32>,
    #[prost(oneof = "webauthn_credential_specifics::EncryptedData", tags = "9, 12")]
    pub encrypted_data: ::core::option::Option<webauthn_credential_specifics::EncryptedData>,
}
/// Nested message and enum types in `WebauthnCredentialSpecifics`.
pub mod webauthn_credential_specifics {
    #[allow(clippy::derive_partial_eq_without_eq)]
    #[derive(Clone, PartialEq, ::prost::Message)]
    pub struct Encrypted {
        /// The bytes of the private key, in PKCS#8 format.
        #[prost(bytes = "vec", optional, tag = "1")]
        pub private_key: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
        /// The secret for implementing the hmac-secret extension
        /// <https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#sctn-hmac-secret-extension>
        #[prost(bytes = "vec", optional, tag = "2")]
        pub hmac_secret: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
        /// The contents of the credential's credBlob.
        /// <https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#sctn-credBlob-extension>
        #[prost(bytes = "vec", optional, tag = "3")]
        pub cred_blob: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
        /// The contents of the credential's largeBlob value(*). Unlike with
        /// security keys, largeBlob data is not stored in a single lump
        /// for all credentials, but as per-credential data. This data
        /// is presented to the authenticator over CTAP and thus has
        /// already had the required DEFLATE compression applied by the
        /// remote platform. The uncompressed size of this data is in
        /// the next field.
        ///
        /// (*) "large" with respect to embedded devices. Maximum length is 2KiB
        /// for Google Password Manager.
        ///
        /// <https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#authenticatorLargeBlobs>
        #[prost(bytes = "vec", optional, tag = "4")]
        pub large_blob: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
        /// The claimed uncompressed size of the DEFLATE-compressed data in
        /// `large_blob`. This corresponds to the `origSize` field from the
        /// spec: <https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#large-blob>
        #[prost(uint64, optional, tag = "5")]
        pub large_blob_uncompressed_size: ::core::option::Option<u64>,
    }
    #[allow(clippy::derive_partial_eq_without_eq)]
    #[derive(Clone, PartialEq, ::prost::Oneof)]
    pub enum EncryptedData {
        /// The bytes of the private key, encrypted with a feature-specific
        /// security domain.
        #[prost(bytes, tag = "9")]
        PrivateKey(::prost::alloc::vec::Vec<u8>),
        /// A serialized, `Encrypted` message, encrypted with a feature-specific
        /// security domain.
        #[prost(bytes, tag = "12")]
        Encrypted(::prost::bytes::Bytes),
    }
}
