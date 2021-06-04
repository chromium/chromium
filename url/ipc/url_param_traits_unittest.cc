// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/stl_util.h"
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
  GURL url1("file://hostname/");
  ExpectSerializationRoundtrips(url1);
  EXPECT_EQ("/", url1.path());
  EXPECT_EQ("hostname", url1.host());

  // Use GURL::Replacement to create a GURL with 1) a path that starts with a C:
  // drive letter and 2) has a non-empty hostname (inherited from `url1` above).
  // Without GURL::Replacement we would just get `url2` below, with an empty
  // hostname, because of how DoParseUNC resets the hostname on Win32 (for more
  // details see https://crbug.com/1214098#c4).
  GURL::Replacements repl;
  const std::string kNewPath = "/C:/dir/file.txt";
  repl.SetPath(kNewPath.c_str(), url::Component(0, kNewPath.length()));
  GURL url1_with_replaced_path = url1.ReplaceComponents(repl);
  EXPECT_EQ(kNewPath, url1_with_replaced_path.path());
  EXPECT_EQ("hostname", url1_with_replaced_path.host());

#ifdef WIN32
  // TODO(https://crbug.com/1214098): All GURLs should round-trip when bounced
  // through IPC, but this doesn't work for `url1_with_replaced_path` on
  // Windows.
  GURL roundtrip = BounceUrl(url1_with_replaced_path);
  EXPECT_NE(roundtrip.host(), url1_with_replaced_path.host());
#else
  // This is the MAIN VERIFICATION in this test.  The fact that this
  // verification fails on Windows is the bug tracked in
  // https://crbug.com/1214098.
  ExpectSerializationRoundtrips(url1_with_replaced_path);
#endif

  // On Windows, `url1_with_replaced_path` will round-trip as `url2`.  (There is
  // nothing wrong with `url2` - its serialization round-trips just fine;  the
  // test assertions below just help explain the lack of round-tripping of
  // `url1_with_replaced_path` above.)
  GURL url2("file://hostname/C:/dir/file.txt");
  ExpectSerializationRoundtrips(url2);
#ifdef WIN32
  EXPECT_EQ(url2.spec(), url1_with_replaced_path.spec());
  EXPECT_EQ(url2.path(), url1_with_replaced_path.path());
  EXPECT_EQ(url2.host(), url1_with_replaced_path.host());
  EXPECT_EQ("/C:/dir/file.txt", url2.path());
  EXPECT_EQ("", url2.host());
#else
  EXPECT_EQ("/C:/dir/file.txt", url2.path());
  EXPECT_EQ("hostname", url2.host());
#endif
}
