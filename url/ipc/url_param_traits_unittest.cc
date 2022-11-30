// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

namespace {

GURL BounceUrl(const GURL& input) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<GURL>::Write(&msg, input);

  GURL output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));

  return output;
}

void ExpectSerializationRoundtrips(const GURL& input) {
  SCOPED_TRACE(testing::Message()
               << "Input GURL: " << input.possibly_invalid_spec());
  GURL output = BounceUrl(input);

  // We want to test each component individually to make sure its range was
  // correctly serialized and deserialized, not just the spec.
  EXPECT_EQ(input.possibly_invalid_spec(), output.possibly_invalid_spec());
  EXPECT_EQ(input.is_valid(), output.is_valid());
  EXPECT_EQ(input.scheme(), output.scheme());
  EXPECT_EQ(input.username(), output.username());
  EXPECT_EQ(input.password(), output.password());
  EXPECT_EQ(input.host(), output.host());
  EXPECT_EQ(input.port(), output.port());
  EXPECT_EQ(input.path(), output.path());
  EXPECT_EQ(input.query(), output.query());
  EXPECT_EQ(input.ref(), output.ref());
}

}  // namespace

// Tests that serialize/deserialize correctly understand each other.
TEST(IPCMessageTest, SerializeGurl_Basic) {
  const char* serialize_cases[] = {
    "http://www.google.com/",
    "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (const char* test_input : serialize_cases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test_input);
    GURL input(test_input);
    ExpectSerializationRoundtrips(input);
  }
}

// Test of an excessively long GURL.
TEST(IPCMessageTest, SerializeGurl_ExcessivelyLong) {
  const std::string url =
      std::string("http://example.org/").append(url::kMaxURLChars + 1, 'a');
  GURL input(url.c_str());
  GURL output = BounceUrl(input);
  EXPECT_TRUE(output.is_empty());
}

// Test of an invalid GURL.
TEST(IPCMessageTest, SerializeGurl_InvalidUrl) {
  IPC::Message msg;
  msg.WriteString("#inva://idurl/");
  GURL output;
  base::PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));
}

// Test of a corrupt deserialization input.
TEST(IPCMessageTest, SerializeGurl_CorruptPayload) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  msg.WriteInt(99);
  GURL output;
  base::PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));
}

// Test for the GURL testcase based on https://crbug.com/1214098 (which in turn
// was based on ContentSecurityPolicyBrowserTest.FileURLs).
TEST(IPCMessageTest, SerializeGurl_WindowsDriveInPathReplacement) {
  {
    // #1: Try creating a file URL with a non-empty hostname.
    GURL url_without_windows_drive_letter("file://hostname/");
    EXPECT_EQ("/", url_without_windows_drive_letter.path());
    EXPECT_EQ("hostname", url_without_windows_drive_letter.host());
    ExpectSerializationRoundtrips(url_without_windows_drive_letter);
  }

  {
    // #2: Use GURL::Replacement to create a GURL with 1) a path that starts
    // with a Windows drive letter and 2) has a non-empty hostname (inherited
    // from `url_without_windows_drive_letter` above). This used to not go
    // through the DoParseUNC path that normally strips the hostname (for more
    // details, see https://crbug.com/1214098#c4).
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://hostname/").ReplaceComponents(repl);

    EXPECT_EQ(kNewPath, url_made_with_replace_components.path());
    EXPECT_EQ("hostname", url_made_with_replace_components.host());
    EXPECT_EQ("file://hostname/C:/dir/file.txt",
              url_made_with_replace_components.spec());
    // This is the MAIN VERIFICATION in this test. This used to fail on Windows,
    // see https://crbug.com/1214098.
    ExpectSerializationRoundtrips(url_made_with_replace_components);
  }

  {
    // #3: Try to create a URL with a Windows drive letter and a non-empty
    // hostname directly.
    GURL url_created_directly("file://hostname/C:/dir/file.txt");
    EXPECT_EQ("/C:/dir/file.txt", url_created_directly.path());
    EXPECT_EQ("hostname", url_created_directly.host());
    EXPECT_EQ("file://hostname/C:/dir/file.txt", url_created_directly.spec());
    ExpectSerializationRoundtrips(url_created_directly);

    // The URL created directly and the URL created through ReplaceComponents
    // should be the same.
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://hostname/").ReplaceComponents(repl);
    EXPECT_EQ(url_created_directly.spec(),
              url_made_with_replace_components.spec());
  }

  {
    // #4: Try to create a URL with a Windows drive letter and "localhost" as
    // hostname directly.
    GURL url_created_directly("file://localhost/C:/dir/file.txt");
    EXPECT_EQ("/C:/dir/file.txt", url_created_directly.path());
    EXPECT_EQ("", url_created_directly.host());
    EXPECT_EQ("file:///C:/dir/file.txt", url_created_directly.spec());
    ExpectSerializationRoundtrips(url_created_directly);

    // The URL created directly and the URL created through ReplaceComponents
    // should be the same.
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://localhost/").ReplaceComponents(repl);
    EXPECT_EQ(url_created_directly.spec(),
              url_made_with_replace_components.spec());
  }
}
