// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_H_
#define URL_URL_CANON_H_

#include <stdlib.h>
#include <string.h>

#include <array>
#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/export_template.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/numerics/clamped_math.h"
#include "url/third_party/mozilla/url_parse.h"

namespace url {

// Represents the different behavior between canonicalizing special URLs
// (https://url.spec.whatwg.org/#is-special) and canonicalizing URLs which are
// not special.
//
// Examples:
// - Special URLs: "https://host/path", "ftp://host/path"
// - Non Special URLs: "about:blank", "data:xxx", "git://host/path"
//
// kFileURL (file://) is a special case of kSpecialURL that allows space
// characters but otherwise behaves identically to kSpecialURL.
// See crbug.com/40256677
enum class CanonMode { kSpecialURL, kNonSpecialURL, kFileURL };

// Canonicalizer output
// -------------------------------------------------------

// Base class for the canonicalizer output, this maintains a buffer and
// supports simple resizing and append operations on it.
//
// It is VERY IMPORTANT that no virtual function calls be made on the common
// code path. We only have two virtual function calls, the destructor and a
// resize function that is called when the existing buffer is not big enough.
// The derived class is then in charge of setting up our buffer which we will
// manage.
template <typename T>
class CanonOutputT {
 public:
  CanonOutputT() = default;
  virtual ~CanonOutputT() = default;

  // Implemented to resize the buffer. This function should update the buffer
  // pointer to point to the new buffer, and any old data up to |cur_len_| in
  // the buffer must be copied over.
  //
  // The new size |sz| must be larger than buffer_len_.
  virtual void Resize(size_t sz) = 0;

  // Accessor for returning a character at a given position. The input offset
  // must be in the valid range.
  inline T at(size_t offset) const { return UNSAFE_TODO(buffer_[offset]); }

  // Sets the character at the given position. The given position MUST be less
  // than the length().
  inline void set(size_t offset, T ch) { UNSAFE_TODO(buffer_[offset]) = ch; }

  // Returns the number of characters currently in the buffer.
  inline size_t length() const { return cur_len_; }

  // Returns the current capacity of the buffer. The length() is the number of
  // characters that have been declared to be written, but the capacity() is
  // the number that can be written without reallocation. If the caller must
  // write many characters at once, it can make sure there is enough capacity,
  // write the data, then use set_size() to declare the new length().
  size_t capacity() const { return buffer_len_; }

  // Returns the contents of the buffer as a string_view.
  std::basic_string_view<T> view() const {
    return std::basic_string_view<T>(data(), length());
  }

  // Called by the user of this class to get the output. The output will NOT
  // be NULL-terminated. Call length() to get the
  // length.
  const T* data() const { return buffer_; }
  T* data() { return buffer_; }

  // Shortens the URL to the new length. Used for "backing up" when processing
  // relative paths. This can also be used if an external function writes a lot
  // of data to the buffer (when using the "Raw" version below) beyond the end,
  // to declare the new length.
  //
  // This MUST NOT be used to expand the size of the buffer beyond capacity().
  void set_length(size_t new_len) { cur_len_ = new_len; }

  // This is the most performance critical function, since it is called for
  // every character.
  void push_back(T ch) {
    // In VC2005, putting this common case first speeds up execution
    // dramatically because this branch is predicted as taken.
    if (cur_len_ < buffer_len_) {
      UNSAFE_TODO(buffer_[cur_len_]) = ch;
      cur_len_++;
      return;
    }

    // Grow the buffer to hold at least one more item. Hopefully we won't have
    // to do this very often.
    if (!Grow(1))
      return;

    // Actually do the insertion.
    UNSAFE_TODO(buffer_[cur_len_]) = ch;
    cur_len_++;
  }

  // Appends the given string to the output.
  void Append(const T* str, size_t str_len) {
    if (str_len > buffer_len_ - cur_len_) {
      if (!Grow(str_len - (buffer_len_ - cur_len_)))
        return;
    }
    UNSAFE_TODO(memcpy(buffer_ + cur_len_, str, str_len * sizeof(T)));
    cur_len_ += str_len;
  }

  void Append(std::basic_string_view<T> str) { Append(str.data(), str.size()); }

  void ReserveSizeIfNeeded(size_t estimated_size) {
    // Reserve a bit extra to account for escaped chars.
    if (estimated_size > buffer_len_)
      Resize((base::ClampedNumeric<size_t>(estimated_size) + 8).RawValue());
  }

  // Insert `str` at `pos`. Used for post-processing non-special URL's pathname.
  // Since this takes O(N), don't use this unless there is a strong reason.
  void Insert(size_t pos, std::basic_string_view<T> str) {
    DCHECK_LE(pos, cur_len_);
    std::basic_string<T> copy(view().substr(pos));
    set_length(pos);
    Append(str);
    Append(copy);
  }

 protected:
  // Grows the given buffer so that it can fit at least |min_additional|
  // characters. Returns true if the buffer could be resized, false on OOM.
  bool Grow(size_t min_additional) {
    static const size_t kMinBufferLen = 16;
    size_t new_len = (buffer_len_ == 0) ? kMinBufferLen : buffer_len_;
    do {
      if (new_len >= (1 << 30))  // Prevent overflow below.
        return false;
      new_len *= 2;
    } while (new_len < buffer_len_ + min_additional);
    Resize(new_len);
    return true;
  }

  // RAW_PTR_EXCLUSION: Performance (based on analysis of sampling profiler
  // data).
  RAW_PTR_EXCLUSION T* buffer_ = nullptr;
  size_t buffer_len_ = 0;

  // Used characters in the buffer.
  size_t cur_len_ = 0;
};

// Simple implementation of the CanonOutput using new[]. This class
// also supports a static buffer so if it is allocated on the stack, most
// URLs can be canonicalized with no heap allocations.
template <typename T, int fixed_capacity = 1024>
class RawCanonOutputT : public CanonOutputT<T> {
 public:
  RawCanonOutputT() : CanonOutputT<T>() {
    this->buffer_ = fixed_buffer_;
    this->buffer_len_ = fixed_capacity;
  }
  ~RawCanonOutputT() override {
    if (this->buffer_ != fixed_buffer_)
      delete[] this->buffer_;
  }

  void Resize(size_t sz) override {
    T* new_buf = new T[sz];
    UNSAFE_TODO(
        memcpy(new_buf, this->buffer_,
               sizeof(T) * (this->cur_len_ < sz ? this->cur_len_ : sz)));
    if (this->buffer_ != fixed_buffer_)
      delete[] this->buffer_;
    this->buffer_ = new_buf;
    this->buffer_len_ = sz;
  }

 protected:
  T fixed_buffer_[fixed_capacity];
};

// Explicitely instantiate commonly used instatiations.
extern template class EXPORT_TEMPLATE_DECLARE(COMPONENT_EXPORT(URL))
    CanonOutputT<char>;
extern template class EXPORT_TEMPLATE_DECLARE(COMPONENT_EXPORT(URL))
    CanonOutputT<char16_t>;

// Normally, all canonicalization output is in narrow characters. We support
// the templates so it can also be used internally if a wide buffer is
// required.
typedef CanonOutputT<char> CanonOutput;
typedef CanonOutputT<char16_t> CanonOutputW;

template <int fixed_capacity>
class RawCanonOutput : public RawCanonOutputT<char, fixed_capacity> {};
template <int fixed_capacity>
class RawCanonOutputW : public RawCanonOutputT<char16_t, fixed_capacity> {};

// Character set converter ----------------------------------------------------
//
// Converts query strings into a custom encoding. The embedder can supply an
// implementation of this class to interface with their own character set
// conversion libraries.
//
// Embedders will want to see the unit test for the ICU version.

class COMPONENT_EXPORT(URL) CharsetConverter {
 public:
  CharsetConverter() {}
  virtual ~CharsetConverter() {}

  // Converts the given input string from UTF-16 to whatever output format the
  // converter supports. This is used only for the query encoding conversion,
  // which does not fail. Instead, the converter should insert "invalid
  // character" characters in the output for invalid sequences, and do the
  // best it can.
  //
  // If the input contains a character not representable in the output
  // character set, the converter should append the HTML entity sequence in
  // decimal, (such as "&#20320;") with escaping of the ampersand, number
  // sign, and semicolon (in the previous example it would be
  // "%26%2320320%3B"). This rule is based on what IE does in this situation.
  virtual void ConvertFromUTF16(std::u16string_view input,
                                CanonOutput* output) = 0;
};

// Schemes --------------------------------------------------------------------

// Types of a scheme representing the requirements on the data represented by
// the authority component of a URL with the scheme.
enum SchemeType {
  // The authority component of a URL with the scheme has the form
  // "username:password@host:port". The username and password entries are
  // optional; the host may not be empty. The default value of the port can be
  // omitted in serialization. This type occurs with network schemes like http,
  // https, and ftp.
  SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION,
  // The authority component of a URL with the scheme has the form "host:port",
  // and does not include username or password. The default value of the port
  // can be omitted in serialization. Used by inner URLs of filesystem URLs of
  // origins with network hosts, from which the username and password are
  // stripped.
  SCHEME_WITH_HOST_AND_PORT,
  // The authority component of an URL with the scheme has the form "host", and
  // does not include port, username, or password. Used when the hosts are not
  // network addresses; for example, schemes used internally by the browser.
  SCHEME_WITH_HOST,
  // A URL with the scheme doesn't have the authority component.
  SCHEME_WITHOUT_AUTHORITY,
};

// Whitespace -----------------------------------------------------------------

// Searches for whitespace that should be removed from the middle of URLs, and
// removes it. Removed whitespace are tabs and newlines, but NOT spaces. Spaces
// are preserved, which is what most browsers do. A string_view pointing to the
// output will be returned.
//
// This should be called before parsing if whitespace removal is desired (which
// it normally is when you are canonicalizing).
//
// If no whitespace is removed, this function will not use the buffer and will
// return the input string_view, to avoid the extra copy. If modification is
// required, the given |buffer| will be used and the returned string_view will
// point to the beginning of the buffer.
//
// Therefore, callers should not use the buffer, since it may actually be empty,
// use the returned string_view instead.
//
// If |input| contained both removable whitespace and a raw `<` character,
// |potentially_dangling_markup| will be set to `true`. Otherwise, it will be
// left untouched.
COMPONENT_EXPORT(URL)
std::string_view RemoveUrlWhitespace(std::string_view input,
                                     CanonOutputT<char>* buffer,
                                     bool* potentially_dangling_markup);
COMPONENT_EXPORT(URL)
std::u16string_view RemoveUrlWhitespace(std::u16string_view input,
                                        CanonOutputT<char16_t>* buffer,
                                        bool* potentially_dangling_markup);

// IDN ------------------------------------------------------------------------

// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must fall in the ASCII range, but will be encoded in UTF-16.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, returns false. The output in this case is undefined.
COMPONENT_EXPORT(URL)
bool IDNToASCII(std::u16string_view src, CanonOutputW* output);

// Piece-by-piece canonicalizers ----------------------------------------------
//
// These individual canonicalizers append the canonicalized versions of the
// corresponding URL component to the given CanonOutput. The spec and the
// previously-identified range of that component are the input. The range of
// the canonicalized component will be written to the output component.
//
// These functions all append to the output so they can be chained. Make sure
// the output is empty when you start.
//
// These functions returns boolean values indicating success. On failure, they
// will attempt to write something reasonable to the output so that, if
// displayed to the user, they will recognise it as something that's messed up.
// Nothing more should ever be done with these invalid URLs, however.

// Scheme: Appends the scheme and colon to the URL. The output component will
// indicate the range of characters up to but not including the colon.
//
// Canonical URLs always have a scheme. If the scheme is not present in the
// input, this will just write the colon to indicate an empty scheme. Does not
// append slashes which will be needed before any authority components for most
// URLs.
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
bool CanonicalizeScheme(std::optional<std::string_view> input,
                        CanonOutput* output,
                        Component* out_scheme);
COMPONENT_EXPORT(URL)
bool CanonicalizeScheme(std::optional<std::u16string_view> input,
                        CanonOutput* output,
                        Component* out_scheme);

// User info: username/password. If present, this will add the delimiters so
// the output will be "<username>:<password>@" or "<username>@". Empty
// username/password pairs, or empty passwords, will get converted to
// nonexistent in the canonical version.
//
// The components for the username and password refer to ranges in the
// respective source strings. Usually, these will be the same string, which
// is legal as long as the two components don't overlap.
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
bool CanonicalizeUserInfo(std::optional<std::string_view> username,
                          std::optional<std::string_view> password,
                          CanonOutput* output,
                          Component* out_username,
                          Component* out_password);
COMPONENT_EXPORT(URL)
bool CanonicalizeUserInfo(std::optional<std::u16string_view> username,
                          std::optional<std::u16string_view> password,
                          CanonOutput* output,
                          Component* out_username,
                          Component* out_password);

// This structure holds detailed state exported from the IP/Host canonicalizers.
// Additional fields may be added as callers require them.
struct CanonHostInfo {
  CanonHostInfo() : family(NEUTRAL), num_ipv4_components(0), out_host() {}

  // Convenience function to test if family is an IP address.
  bool IsIPAddress() const { return family == IPV4 || family == IPV6; }

  // This field summarizes how the input was classified by the canonicalizer.
  enum Family {
    NEUTRAL,  // - Doesn't resemble an IP address. As far as the IP
              //   canonicalizer is concerned, it should be treated as a
              //   hostname.
    BROKEN,   // - Almost an IP, but was not canonicalized. This could be an
              //   IPv4 address where truncation occurred, or something
              //   containing the special characters :[] which did not parse
              //   as an IPv6 address. Never attempt to connect to this
              //   address, because it might actually succeed!
    IPV4,     // - Successfully canonicalized as an IPv4 address.
    IPV6,     // - Successfully canonicalized as an IPv6 address.
  };
  Family family;

  // If |family| is IPV4, then this is the number of nonempty dot-separated
  // components in the input text, from 1 to 4. If |family| is not IPV4,
  // this value is undefined.
  int num_ipv4_components;

  // Location of host within the canonicalized output.
  // CanonicalizeIPAddress() only sets this field if |family| is IPV4 or IPV6.
  // CanonicalizeHostVerbose() always sets it.
  Component out_host;

  // |address| contains the parsed IP Address (if any) in its first
  // AddressLength() bytes, in network order. If IsIPAddress() is false
  // AddressLength() will return zero and the content of |address| is undefined.
  std::array<uint8_t, 16> address;

  // Convenience function to calculate the length of an IP address corresponding
  // to the current IP version in |family|, if any. For use with |address|.
  int AddressLength() const {
    return family == IPV4 ? 4 : (family == IPV6 ? 16 : 0);
  }

  // Returns a span pointing a valid range of `address`. The size of the
  // resultant span is 4, 16, or 0.
  base::span<const uint8_t> AddressSpan() const LIFETIME_BOUND {
    return base::span(address).first(static_cast<size_t>(AddressLength()));
  }
};

// Deprecated. Please call either CanonicalizeSpecialHost or
// CanonicalizeNonSpecialHost.
//
// TODO(crbug.com/40063064): Check the callers of these functions.
COMPONENT_EXPORT(URL)
bool CanonicalizeHost(std::string_view spec,
                      const Component& host,
                      CanonOutput* output,
                      Component* out_host);
COMPONENT_EXPORT(URL)
bool CanonicalizeHost(std::u16string_view spec,
                      const Component& host,
                      CanonOutput* output,
                      Component* out_host);

// Host in special URLs.
//
// The 8-bit version requires UTF-8 encoding. Use this version when you only
// need to know whether canonicalization succeeded.
COMPONENT_EXPORT(URL)
bool CanonicalizeSpecialHost(std::string_view spec,
                             const Component& host,
                             CanonOutput& output,
                             Component& out_host);
COMPONENT_EXPORT(URL)
bool CanonicalizeSpecialHost(std::u16string_view spec,
                             const Component& host,
                             CanonOutput& output,
                             Component& out_host);

// Host in special URLs.
//
// The 8-bit version requires UTF-8 encoding. Use this version when you only
// need to know whether canonicalization succeeded.
COMPONENT_EXPORT(URL)
bool CanonicalizeFileHost(std::string_view spec,
                          const Component& host,
                          CanonOutput& output,
                          Component& out_host);
COMPONENT_EXPORT(URL)
bool CanonicalizeFileHost(std::u16string_view spec,
                          const Component& host,
                          CanonOutput& output,
                          Component& out_host);

// Deprecated. Please call either CanonicalizeSpecialHostVerbose or
// CanonicalizeNonSpecialHostVerbose.
//
// TODO(crbug.com/40063064): Check the callers of these functions.
COMPONENT_EXPORT(URL)
void CanonicalizeHostVerbose(const char* spec,
                             const Component& host,
                             CanonOutput* output,
                             CanonHostInfo* host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeHostVerbose(std::string_view spec,
                             const Component& host,
                             CanonOutput* output,
                             CanonHostInfo* host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeHostVerbose(std::u16string_view spec,
                             const Component& host,
                             CanonOutput* output,
                             CanonHostInfo* host_info);

// Extended version of CanonicalizeSpecialHost, which returns additional
// information. Use this when you need to know whether the hostname was an IP
// address. A successful return is indicated by host_info->family != BROKEN. See
// the definition of CanonHostInfo above for details.
COMPONENT_EXPORT(URL)
void CanonicalizeSpecialHostVerbose(std::string_view spec,
                                    const Component& host,
                                    CanonOutput& output,
                                    CanonHostInfo& host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeSpecialHostVerbose(std::u16string_view spec,
                                    const Component& host,
                                    CanonOutput& output,
                                    CanonHostInfo& host_info);

// Extended version of CanonicalizeFileHost, which returns additional
// information. Use this when you need to know whether the hostname was an IP
// address. A successful return is indicated by host_info->family != BROKEN. See
// the definition of CanonHostInfo above for details.
COMPONENT_EXPORT(URL)
void CanonicalizeFileHostVerbose(std::string_view spec,
                                 const Component& host,
                                 CanonOutput& output,
                                 CanonHostInfo& host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeFileHostVerbose(std::u16string_view spec,
                                 const Component& host,
                                 CanonOutput& output,
                                 CanonHostInfo& host_info);

// Canonicalizes a string according to the host canonicalization rules. Unlike
// CanonicalizeHost, this will not check for IP addresses which can change the
// meaning (and canonicalization) of the components. This means it is possible
// to call this for sub-components of a host name without corruption.
//
// As an example, "01.02.03.04.com" is a canonical hostname. If you called
// CanonicalizeHost on the substring "01.02.03.04" it will get "fixed" to
// "1.2.3.4" which will produce an invalid host name when reassembled. This
// can happen more than one might think because all numbers by themselves are
// considered IP addresses; so "5" canonicalizes to "0.0.0.5".
//
// Be careful: Because Punycode works on each dot-separated substring as a
// unit, you should only pass this function substrings that represent complete
// dot-separated subcomponents of the original host. Even if you have ASCII
// input, percent-escaped characters will have different meanings if split in
// the middle.
//
// Returns true if the host was valid. This function will treat a 0-length
// host as valid (because it's designed to be used for substrings) while the
// full version above will mark empty hosts as broken.
COMPONENT_EXPORT(URL)
bool CanonicalizeHostSubstring(std::string_view host_view, CanonOutput* output);
COMPONENT_EXPORT(URL)
bool CanonicalizeHostSubstring(std::u16string_view host_view,
                               CanonOutput* output);

// Host in non-special URLs.
COMPONENT_EXPORT(URL)
bool CanonicalizeNonSpecialHost(std::string_view spec,
                                const Component& host,
                                CanonOutput& output,
                                Component& out_host);
COMPONENT_EXPORT(URL)
bool CanonicalizeNonSpecialHost(std::u16string_view spec,
                                const Component& host,
                                CanonOutput& output,
                                Component& out_host);

// Extended version of CanonicalizeNonSpecialHost, which returns additional
// information. See CanonicalizeSpecialHost for details.
COMPONENT_EXPORT(URL)
void CanonicalizeNonSpecialHostVerbose(std::string_view spec,
                                       const Component& host,
                                       CanonOutput& output,
                                       CanonHostInfo& host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeNonSpecialHostVerbose(std::u16string_view spec,
                                       const Component& host,
                                       CanonOutput& output,
                                       CanonHostInfo& host_info);

// IP addresses.
//
// Tries to interpret the given host name as an IPv4 or IPv6 address. If it is
// an IP address, it will canonicalize it as such, appending it to |output|.
// Additional status information is returned via the |*host_info| parameter.
// See the definition of CanonHostInfo above for details.
//
// This is called AUTOMATICALLY from the host canonicalizer, which ensures that
// the input is unescaped and name-prepped, etc. It should not normally be
// necessary or wise to call this directly.
COMPONENT_EXPORT(URL)
void CanonicalizeIPAddress(std::string_view host_view,
                           CanonOutput* output,
                           CanonHostInfo* host_info);
COMPONENT_EXPORT(URL)
void CanonicalizeIPAddress(std::u16string_view host_view,
                           CanonOutput* output,
                           CanonHostInfo* host_info);

// Similar to CanonicalizeIPAddress, but supports only IPv6 address.
COMPONENT_EXPORT(URL)
void CanonicalizeIPv6Address(std::string_view host_view,
                             CanonOutput& output,
                             CanonHostInfo& host_info);

COMPONENT_EXPORT(URL)
void CanonicalizeIPv6Address(std::string_view host_view,
                             CanonOutput& output,
                             CanonHostInfo& host_info);

// Port: this function will add the colon for the port if a port is present.
// The caller can pass PORT_UNSPECIFIED as the
// default_port_for_scheme argument if there is no default port.
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
bool CanonicalizePort(std::optional<std::string_view> port_view,
                      int default_port_for_scheme,
                      CanonOutput* output,
                      Component* out_port);
COMPONENT_EXPORT(URL)
bool CanonicalizePort(std::optional<std::u16string_view> port_view,
                      int default_port_for_scheme,
                      CanonOutput* output,
                      Component* out_port);

// Returns the default port for the given canonical scheme, or PORT_UNSPECIFIED
// if the scheme is unknown. Based on https://url.spec.whatwg.org/#default-port
COMPONENT_EXPORT(URL)
int DefaultPortForScheme(std::string_view scheme);

// Path. If the input does not begin in a slash (including if the input is
// empty), we'll prepend a slash to the path to make it canonical.
//
// The 8-bit version assumes UTF-8 encoding, but does not verify the validity
// of the UTF-8 (i.e., you can have invalid UTF-8 sequences, invalid
// characters, etc.). Normally, URLs will come in as UTF-16, so this isn't
// an issue. Somebody giving us an 8-bit path is responsible for generating
// the path that the server expects (we'll escape high-bit characters), so
// if something is invalid, it's their problem.
COMPONENT_EXPORT(URL)
bool CanonicalizePath(std::optional<std::string_view> path,
                      CanonMode canon_mode,
                      CanonOutput* output,
                      Component* out_path);
COMPONENT_EXPORT(URL)
bool CanonicalizePath(std::optional<std::u16string_view> path,
                      CanonMode canon_mode,
                      CanonOutput* output,
                      Component* out_path);

// Deprecated. Please pass CanonMode explicitly.
//
// These functions are also used in net/third_party code. So removing these
// functions requires several steps.
// TODO(crbug.com/422740114): Remove this after `//net/third_party/quiche` is
// not depending on it.
COMPONENT_EXPORT(URL)
bool CanonicalizePath(const char* spec,
                      const Component& path,
                      CanonOutput* output,
                      Component* out_path);
COMPONENT_EXPORT(URL)
bool CanonicalizePath(std::optional<std::string_view> path,
                      CanonOutput* output,
                      Component* out_path);
COMPONENT_EXPORT(URL)
bool CanonicalizePath(std::optional<std::u16string_view> path,
                      CanonOutput* output,
                      Component* out_path);

// Like CanonicalizePath(), but does not assume that its operating on the
// entire path.  It therefore does not prepend a slash, etc.
COMPONENT_EXPORT(URL)
bool CanonicalizePartialPath(std::optional<std::string_view> path,
                             CanonOutput* output,
                             Component* out_path);
COMPONENT_EXPORT(URL)
bool CanonicalizePartialPath(std::optional<std::u16string_view> path,
                             CanonOutput* output,
                             Component* out_path);

// Canonicalizes the input as a file path. This is like CanonicalizePath except
// that it also handles Windows drive specs. For example, the path can begin
// with "c|\" and it will get properly canonicalized to "C:/".
// The string will be appended to |*output| and |*out_path| will be updated.
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
bool FileCanonicalizePath(std::optional<std::string_view> path,
                          CanonOutput* output,
                          Component* out_path);
COMPONENT_EXPORT(URL)
bool FileCanonicalizePath(std::optional<std::u16string_view> path,
                          CanonOutput* output,
                          Component* out_path);

// Query: Prepends the ? if needed.
//
// The 8-bit version requires the input to be UTF-8 encoding. Incorrectly
// encoded characters (in UTF-8 or UTF-16) will be replaced with the Unicode
// "invalid character." This function can not fail, we always just try to do
// our best for crazy input here since web pages can set it themselves.
//
// This will convert the given input into the output encoding that the given
// character set converter object provides. The converter will only be called
// if necessary, for ASCII input, no conversions are necessary.
//
// The converter can be NULL. In this case, the output encoding will be UTF-8.
COMPONENT_EXPORT(URL)
void CanonicalizeQuery(std::optional<std::string_view> input,
                       CharsetConverter* converter,
                       CanonOutput* output,
                       Component* out_query);
COMPONENT_EXPORT(URL)
void CanonicalizeQuery(std::optional<std::u16string_view> input,
                       CharsetConverter* converter,
                       CanonOutput* output,
                       Component* out_query);

// Ref: Prepends the # if needed. The output will be UTF-8 (this is the only
// canonicalizer that does not produce ASCII output). The output is
// guaranteed to be valid UTF-8.
//
// This function will not fail. If the input is invalid UTF-8/UTF-16, we'll use
// the "Unicode replacement character" for the confusing bits and copy the rest.
COMPONENT_EXPORT(URL)
void CanonicalizeRef(std::optional<std::string_view> spec,
                     CanonOutput* output,
                     Component* out_path);
COMPONENT_EXPORT(URL)
void CanonicalizeRef(std::optional<std::u16string_view> spec,
                     CanonOutput* output,
                     Component* out_path);

// Full canonicalizer ---------------------------------------------------------
//
// These functions replace any string contents, rather than append as above.
// See the above piece-by-piece functions for information specific to
// canonicalizing individual components.
//
// The output will be ASCII except the reference fragment, which may be UTF-8.
//
// The 8-bit versions require UTF-8 encoding.

// Use for standard URLs with authorities and paths.
COMPONENT_EXPORT(URL)
bool CanonicalizeStandardUrl(std::string_view spec,
                             const Parsed& parsed,
                             SchemeType scheme_type,
                             CharsetConverter* query_converter,
                             CanonOutput* output,
                             Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizeStandardUrl(std::u16string_view spec,
                             const Parsed& parsed,
                             SchemeType scheme_type,
                             CharsetConverter* query_converter,
                             CanonOutput* output,
                             Parsed* new_parsed);

// Use for non-special URLs.
COMPONENT_EXPORT(URL)
bool CanonicalizeNonSpecialUrl(std::string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput& output,
                               Parsed& new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizeNonSpecialUrl(std::u16string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput& output,
                               Parsed& new_parsed);

// Use for file URLs.
COMPONENT_EXPORT(URL)
bool CanonicalizeFileUrl(std::string_view spec,
                         const Parsed& parsed,
                         CharsetConverter* query_converter,
                         CanonOutput* output,
                         Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizeFileUrl(std::u16string_view spec,
                         const Parsed& parsed,
                         CharsetConverter* query_converter,
                         CanonOutput* output,
                         Parsed* new_parsed);

// Use for filesystem URLs.
COMPONENT_EXPORT(URL)
bool CanonicalizeFileSystemUrl(std::string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput* output,
                               Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizeFileSystemUrl(std::u16string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput* output,
                               Parsed* new_parsed);

// Use for path URLs such as javascript. This does not modify the path in any
// way, for example, by escaping it.
COMPONENT_EXPORT(URL)
bool CanonicalizePathUrl(std::string_view spec,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizePathUrl(std::u16string_view spec,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed);

// Use to canonicalize just the path component of a "path" URL; e.g. the
// path of a javascript URL.
COMPONENT_EXPORT(URL)
void CanonicalizePathUrlPath(std::optional<std::string_view> source,
                             CanonOutput* output,
                             Component* new_component);
COMPONENT_EXPORT(URL)
void CanonicalizePathUrlPath(std::optional<std::u16string_view> source,
                             CanonOutput* output,
                             Component* new_component);

// Use for mailto URLs. This "canonicalizes" the URL into a path and query
// component. It does not attempt to merge "to" fields. It uses UTF-8 for
// the query encoding if there is a query. This is because a mailto URL is
// really intended for an external mail program, and the encoding of a page,
// etc. which would influence a query encoding normally are irrelevant.
COMPONENT_EXPORT(URL)
bool CanonicalizeMailtoUrl(std::string_view spec,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool CanonicalizeMailtoUrl(std::u16string_view spec,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed);

// Part replacer --------------------------------------------------------------

// Internal structure used for storing separate strings for each component.
// The basic canonicalization functions use this structure internally so that
// component replacement (different strings for different components) can be
// treated on the same code path as regular canonicalization (the same string
// for each component).
//
// A Parsed structure usually goes along with this. Those components identify
// offsets within these strings, so that they can all be in the same string,
// or spread arbitrarily across different ones.
//
// This structures does not own any data. It is the caller's responsibility to
// ensure that the data the pointers point to stays in scope and is not
// modified.
template <typename CHAR>
struct URLComponentSource {
  STACK_ALLOCATED();

 public:
  // Constructor normally used by callers wishing to replace components. This
  // will make them all null string_view, which is no replacement. The caller
  // would then override the components they want to replace.
  URLComponentSource() = default;

  // Constructor normally used internally to initialize all the components to
  // point to the same spec.
  explicit URLComponentSource(std::basic_string_view<CHAR> default_value)
      : scheme(default_value),
        username(default_value),
        password(default_value),
        host(default_value),
        port(default_value),
        path(default_value),
        query(default_value),
        ref(default_value) {}

  // The following data members distinguish between std::basic_string_view()
  // and std::basic_string_view(""). The former's data() returns nullptr.
  //
  // This behavior is confusing, but Replacements::SetFooStr() has
  // distinguished these two for a long time, and it is costly to eliminate
  // the distinction.

  std::basic_string_view<CHAR> scheme;
  std::basic_string_view<CHAR> username;
  std::basic_string_view<CHAR> password;
  std::basic_string_view<CHAR> host;
  std::basic_string_view<CHAR> port;
  std::basic_string_view<CHAR> path;
  std::basic_string_view<CHAR> query;
  std::basic_string_view<CHAR> ref;
};

// This structure encapsulates information on modifying a URL. Each component
// may either be left unchanged, replaced, or deleted.
//
// By default, each component is unchanged. For those components that should be
// modified, call either Set* or Clear* to modify it.
//
// The string passed to Set* functions DOES NOT GET COPIED AND MUST BE KEPT
// IN SCOPE BY THE CALLER for as long as this object exists!
// In order to make it harder to misuse the API the setters do not accept rvalue
// references to std::strings and std::u16strings.
// Note: Extra const CHAR* overloads are necessary to break ambiguities that
// would otherwise exist for string literals.
//
// Prefer the 8-bit replacement version if possible since it is more efficient.
template <typename CHAR>
class Replacements {
  using StringViewT = std::basic_string_view<CHAR>;
  using StringT = std::basic_string<CHAR>;

 public:
  // Creates an empty `Replacements` instance, which indicates that no
  // components will be replaced.
  Replacements() = default;

  // Creates a `Replacements` instance from a single spec and its parsed
  // components. This is used to indicate that all components of a URL should
  // be replaced.
  Replacements(StringViewT spec, const Parsed& parsed)
      : sources_(spec), components_(parsed) {}

  // Scheme
  void SetScheme(StringViewT s, const Component& comp) {
    sources_.scheme = s;
    components_.scheme = comp;
  }
  void SetSchemeStr(StringViewT str) { SetScheme(str, Component(str)); }
  void SetSchemeStr(const CHAR* str) { SetSchemeStr(StringViewT(str)); }
  void SetSchemeStr(const StringT&&) = delete;
  // Note: we don't have a ClearScheme since this doesn't make any sense.

  // Indicates the scheme part won't be replaced.
  void SetSchemeUnchanged() {
    sources_.scheme = StringViewT();
    components_.scheme = Component();
  }
  // Return the scheme part of the source string if the scheme component is
  // valid.
  std::optional<StringViewT> MaybeScheme() const {
    return components_.scheme.MaybeAsViewOn(sources_.scheme);
  }
  bool IsSchemeOverridden() const { return sources_.scheme.data(); }

  // Username
  void SetUsername(StringViewT s, const Component& comp) {
    sources_.username = s;
    components_.username = comp;
  }
  void SetUsernameStr(StringViewT str) { SetUsername(str, Component(str)); }
  void SetUsernameStr(const CHAR* str) { SetUsernameStr(StringViewT(str)); }
  void SetUsernameStr(const StringT&&) = delete;
  void ClearUsername() {
    sources_.username = Placeholder();
    components_.username = Component();
  }
  // Return the username part of the source string if the username component is
  // valid.
  std::optional<StringViewT> MaybeUsername() const {
    return components_.username.MaybeAsViewOn(sources_.username);
  }
  bool IsUsernameOverridden() const { return sources_.username.data(); }

  // Password
  void SetPassword(StringViewT s, const Component& comp) {
    sources_.password = s;
    components_.password = comp;
  }
  void SetPasswordStr(StringViewT str) { SetPassword(str, Component(str)); }
  void SetPasswordStr(const CHAR* str) { SetPasswordStr(StringViewT(str)); }
  void SetPasswordStr(const StringT&&) = delete;
  void ClearPassword() {
    sources_.password = Placeholder();
    components_.password = Component();
  }
  // Return the password part of the source string if the password component is
  // valid.
  std::optional<StringViewT> MaybePassword() const {
    return components_.password.MaybeAsViewOn(sources_.password);
  }
  bool IsPasswordOverridden() const { return sources_.password.data(); }

  // Host
  void SetHost(StringViewT s, const Component& comp) {
    sources_.host = s;
    components_.host = comp;
  }
  void SetHostStr(StringViewT str) { SetHost(str, Component(str)); }
  void SetHostStr(const CHAR* str) { SetHostStr(StringViewT(str)); }
  void SetHostStr(const StringT&&) = delete;
  void ClearHost() {
    sources_.host = Placeholder();
    components_.host = Component();
  }
  // Return the host part of the source string if the host component is
  // valid.
  std::optional<StringViewT> MaybeHost() const {
    return components_.host.MaybeAsViewOn(sources_.host);
  }
  // Return the string starting at the beginning of the source string and ending
  // at the end of the host part.  The beginning of the source string is
  // different from the beginning of the host part if components().host.begin()
  // is not 0.
  StringViewT SpecUntilHostOrEmpty() const {
    return sources_.host.substr(
        0, components_.host.is_valid() ? components_.host.end() : 0);
  }
  bool IsHostOverridden() const { return sources_.host.data(); }

  // Port
  void SetPort(StringViewT s, const Component& comp) {
    sources_.port = s;
    components_.port = comp;
  }
  void SetPortStr(StringViewT str) { SetPort(str, Component(str)); }
  void SetPortStr(const CHAR* str) { SetPortStr(StringViewT(str)); }
  void SetPortStr(const StringT&&) = delete;
  void ClearPort() {
    sources_.port = Placeholder();
    components_.port = Component();
  }
  // Return the port part of the source string if the port component is valid.
  std::optional<StringViewT> MaybePort() const {
    return components_.port.MaybeAsViewOn(sources_.port);
  }
  bool IsPortOverridden() const { return sources_.port.data(); }

  // Path
  void SetPath(StringViewT s, const Component& comp) {
    sources_.path = s;
    components_.path = comp;
  }
  void SetPathStr(StringViewT str) { SetPath(str, Component(str)); }
  void SetPathStr(const CHAR* str) { SetPathStr(StringViewT(str)); }
  void SetPathStr(const StringT&&) = delete;
  void ClearPath() {
    sources_.path = Placeholder();
    components_.path = Component();
  }
  // Return the path part of the source string if the path component is alid.
  std::optional<StringViewT> MaybePath() const {
    return components_.path.MaybeAsViewOn(sources_.path);
  }
  bool IsPathOverridden() const { return sources_.path.data(); }

  // Query
  void SetQuery(StringViewT s, const Component& comp) {
    sources_.query = s;
    components_.query = comp;
  }
  void SetQueryStr(StringViewT str) { SetQuery(str, Component(str)); }
  void SetQueryStr(const CHAR* str) { SetQueryStr(StringViewT(str)); }
  void SetQueryStr(const StringT&&) = delete;
  void ClearQuery() {
    sources_.query = Placeholder();
    components_.query = Component();
  }
  // Return the query part of the source string if the query component is
  // valid.
  std::optional<StringViewT> MaybeQuery() const {
    return components_.query.MaybeAsViewOn(sources_.query);
  }
  bool IsQueryOverridden() const { return sources_.query.data(); }

  // Ref
  void SetRef(StringViewT s, const Component& comp) {
    sources_.ref = s;
    components_.ref = comp;
  }
  void SetRefStr(StringViewT str) { SetRef(str, Component(str)); }
  void SetRefStr(const CHAR* str) { SetRefStr(StringViewT(str)); }
  void SetRefStr(const StringT&&) = delete;
  void ClearRef() {
    sources_.ref = Placeholder();
    components_.ref = Component();
  }
  // Return the ref part of the source string if the ref component is valid.
  std::optional<StringViewT> MaybeRef() const {
    return components_.ref.MaybeAsViewOn(sources_.ref);
  }
  bool IsRefOverridden() const { return sources_.ref.data(); }

  // Getters for the internal data. See the variables below for how the
  // information is encoded.
  const URLComponentSource<CHAR>& sources() const { return sources_; }
  const Parsed& components() const { return components_; }

 private:
  friend void SetupOverrideComponents(const Replacements<char>& repl,
                                      Replacements<char>& overridden);
  friend bool SetupUtf16OverrideComponents(const Replacements<char16_t>& repl,
                                           CanonOutput& utf8_buffer,
                                           Replacements<char>& overridden);

  // Returns a string_view on a static empty string that is used as a
  // placeholder to indicate a component should be deleted (see below).
  StringViewT Placeholder() {
    static const CHAR empty_cstr = 0;
    // NOLINTNEXTLINE(bugprone-string-constructor)
    return StringViewT(&empty_cstr, 0);
  }

  // We support three states:
  //
  // Action                 | Source                Component
  // -----------------------+--------------------------------------------------
  // Don't change component | null string_view      (unused)
  // Replace component      | (replacement string)  (replacement component)
  // Delete component       | empty string_view     (invalid component: (0,-1))
  URLComponentSource<CHAR> sources_;
  Parsed components_;
};

// The base must be an 8-bit canonical URL.
COMPONENT_EXPORT(URL)
bool ReplaceStandardUrl(std::string_view base,
                        const Parsed& base_parsed,
                        const Replacements<char>& replacements,
                        SchemeType scheme_type,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool ReplaceStandardUrl(std::string_view base,
                        const Parsed& base_parsed,
                        const Replacements<char16_t>& replacements,
                        SchemeType scheme_type,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* new_parsed);

// For non-special URLs.
COMPONENT_EXPORT(URL)
bool ReplaceNonSpecialUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput& output,
                          Parsed& new_parsed);
COMPONENT_EXPORT(URL)
bool ReplaceNonSpecialUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char16_t>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput& output,
                          Parsed& new_parsed);

// Filesystem URLs can only have the path, query, or ref replaced.
// All other components will be ignored.
COMPONENT_EXPORT(URL)
bool ReplaceFileSystemUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput* output,
                          Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool ReplaceFileSystemUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char16_t>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput* output,
                          Parsed* new_parsed);

// Replacing some parts of a file URL is not permitted. Everything except
// the host, path, query, and ref will be ignored.
COMPONENT_EXPORT(URL)
bool ReplaceFileUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CharsetConverter* query_converter,
                    CanonOutput* output,
                    Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool ReplaceFileUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char16_t>& replacements,
                    CharsetConverter* query_converter,
                    CanonOutput* output,
                    Parsed* new_parsed);

// Path URLs can only have the scheme and path replaced. All other components
// will be ignored.
COMPONENT_EXPORT(URL)
bool ReplacePathUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool ReplacePathUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char16_t>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed);

// Mailto URLs can only have the scheme, path, and query replaced.
// All other components will be ignored.
COMPONENT_EXPORT(URL)
bool ReplaceMailtoUrl(std::string_view base,
                      const Parsed& base_parsed,
                      const Replacements<char>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed);
COMPONENT_EXPORT(URL)
bool ReplaceMailtoUrl(std::string_view base,
                      const Parsed& base_parsed,
                      const Replacements<char16_t>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed);

// Relative URL ---------------------------------------------------------------

// Given an input URL or URL fragment |fragment|, determines if it is a
// relative or absolute URL and places the result into |*is_relative|. If it is
// relative, the relevant portion of the URL will be placed into
// |*relative_component| (there may have been trimmed whitespace, for example).
// This value is passed to ResolveRelativeUrl. If the input is not relative,
// this value is UNDEFINED (it may be changed by the function).
//
// Returns true on success (we successfully determined the URL is relative or
// not). Failure means that the combination of URLs doesn't make any sense.
//
// The base URL should always be canonical, therefore is ASCII.
COMPONENT_EXPORT(URL)
bool IsRelativeUrl(std::string_view base,
                   const Parsed& base_parsed,
                   std::string_view fragment,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   Component* relative_component);
COMPONENT_EXPORT(URL)
bool IsRelativeUrl(std::string_view base,
                   const Parsed& base_parsed,
                   std::u16string_view fragment,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   Component* relative_component);

// Given a canonical parsed source URL, a URL fragment known to be relative,
// and the identified relevant portion of the relative URL (computed by
// IsRelativeUrl), this produces a new parsed canonical URL in |output| and
// |out_parsed|.
//
// It also requires a flag indicating whether the base URL is a file: URL
// which triggers additional logic.
//
// The base URL should be canonical and have a host (may be empty for file
// URLs) and a path. If it doesn't have these, we can't resolve relative
// URLs off of it and will return the base as the output with an error flag.
// Because it is canonical is should also be ASCII.
//
// The query charset converter follows the same rules as CanonicalizeQuery.
//
// Returns true on success. On failure, the output will be "something
// reasonable" that will be consistent and valid, just probably not what
// was intended by the web page author or caller.
COMPONENT_EXPORT(URL)
bool ResolveRelativeUrl(std::string_view base_url,
                        const Parsed& base_parsed,
                        bool base_is_file,
                        std::string_view relative_url,
                        const Component& relative_component,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* out_parsed);
COMPONENT_EXPORT(URL)
bool ResolveRelativeUrl(std::string_view base_url,
                        const Parsed& base_parsed,
                        bool base_is_file,
                        std::u16string_view relative_url,
                        const Component& relative_component,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* out_parsed);

}  // namespace url

#endif  // URL_URL_CANON_H_
