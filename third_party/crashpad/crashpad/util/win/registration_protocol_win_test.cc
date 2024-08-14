// Copyright 2016 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/win/registration_protocol_win.h"

#include <aclapi.h>
#include <sddl.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "util/win/scoped_handle.h"
#include "util/win/scoped_local_alloc.h"

namespace crashpad {
namespace test {
namespace {

std::wstring GetStringFromSid(PSID sid) {
  LPWSTR sid_str;
  if (!ConvertSidToStringSid(sid, &sid_str)) {
    PLOG(ERROR) << "ConvertSidToStringSid";
    return std::wstring();
  }
  ScopedLocalAlloc sid_str_ptr(sid_str);
  return sid_str;
}

std::wstring GetUserSidString() {
  HANDLE token_handle;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token_handle)) {
    PLOG(ERROR) << "OpenProcessToken";
    return std::wstring();
  }

  ScopedKernelHANDLE token(token_handle);
  DWORD user_size = 0;
  GetTokenInformation(token.get(), TokenUser, nullptr, 0, &user_size);
  if (user_size == 0) {
    PLOG(ERROR) << "GetTokenInformation Size";
    return std::wstring();
  }

  std::vector<char> user(user_size);
  if (!GetTokenInformation(
          token.get(), TokenUser, user.data(), user_size, &user_size)) {
    PLOG(ERROR) << "GetTokenInformation";
    return std::wstring();
  }

  TOKEN_USER* user_ptr = reinterpret_cast<TOKEN_USER*>(user.data());
  return GetStringFromSid(user_ptr->User.Sid);
}

void CheckAce(PACL acl,
              DWORD index,
              BYTE check_ace_type,
              ACCESS_MASK check_mask,
              const std::wstring& check_sid) {
  ASSERT_FALSE(check_sid.empty());
  void* ace_ptr;
  ASSERT_TRUE(GetAce(acl, index, &ace_ptr));

  ACE_HEADER* header = static_cast<ACE_HEADER*>(ace_ptr);
  ASSERT_EQ(check_ace_type, header->AceType);
  ASSERT_EQ(0, header->AceFlags);

  PSID sid = nullptr;
  ACCESS_MASK mask = 0;
  switch (header->AceType) {
    case ACCESS_ALLOWED_ACE_TYPE: {
      ACCESS_ALLOWED_ACE* allowed_ace =
          static_cast<ACCESS_ALLOWED_ACE*>(ace_ptr);
      sid = &allowed_ace->SidStart;
      mask = allowed_ace->Mask;
    } break;
    case SYSTEM_MANDATORY_LABEL_ACE_TYPE: {
      SYSTEM_MANDATORY_LABEL_ACE* label_ace =
          static_cast<SYSTEM_MANDATORY_LABEL_ACE*>(ace_ptr);
      sid = &label_ace->SidStart;
      mask = label_ace->Mask;
    } break;
    default:
      NOTREACHED();
  }

  ASSERT_EQ(check_mask, mask);
  ASSERT_EQ(check_sid, GetStringFromSid(sid));
}

TEST(SecurityDescriptor, NamedPipeDefault) {
  const void* sec_desc = GetSecurityDescriptorForNamedPipeInstance(nullptr);

  PACL acl;
  BOOL acl_present;
  BOOL acl_defaulted;
  ASSERT_TRUE(GetSecurityDescriptorDacl(
      const_cast<void*>(sec_desc), &acl_present, &acl, &acl_defaulted));
  ASSERT_EQ(3, acl->AceCount);
  CheckAce(acl, 0, ACCESS_ALLOWED_ACE_TYPE, GENERIC_ALL, GetUserSidString());
  // Check SYSTEM user SID.
  CheckAce(acl, 1, ACCESS_ALLOWED_ACE_TYPE, GENERIC_ALL, L"S-1-5-18");
  // Check ALL APPLICATION PACKAGES group SID.
  CheckAce(acl,
           2,
           ACCESS_ALLOWED_ACE_TYPE,
           GENERIC_READ | GENERIC_WRITE,
           L"S-1-15-2-1");

  ASSERT_TRUE(GetSecurityDescriptorSacl(
      const_cast<void*>(sec_desc), &acl_present, &acl, &acl_defaulted));
  ASSERT_EQ(1, acl->AceCount);
  CheckAce(acl, 0, SYSTEM_MANDATORY_LABEL_ACE_TYPE, 0, L"S-1-16-0");
}

TEST(SecurityDescriptor, MatchesAdvapi32) {
  // This security descriptor is built manually in the connection code to avoid
  // calling the advapi32 functions. Verify that it returns the same thing as
  // ConvertStringSecurityDescriptorToSecurityDescriptor() would.

  // Mandatory Label, no ACE flags, no ObjectType, integrity level
  // untrusted.
  static constexpr wchar_t kSddl[] = L"S:(ML;;;;;S-1-16-0)";
  PSECURITY_DESCRIPTOR sec_desc;
  ULONG sec_desc_len;
  ASSERT_TRUE(ConvertStringSecurityDescriptorToSecurityDescriptor(
      kSddl, SDDL_REVISION_1, &sec_desc, &sec_desc_len))
      << ErrorMessage("ConvertStringSecurityDescriptorToSecurityDescriptor");
  ScopedLocalAlloc sec_desc_owner(sec_desc);

  size_t created_len;
  const void* const created =
      GetFallbackSecurityDescriptorForNamedPipeInstance(&created_len);
  ASSERT_EQ(created_len, sec_desc_len);
  EXPECT_EQ(memcmp(sec_desc, created, sec_desc_len), 0);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
