// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * A Helper class for JUnit tests to be able to use GURLs without requiring native initialization.
 * This should be used sparingly, when converting junit tests to Batched Instrumentation tests is
 * not feasible.
 *
 * If any more complex GURL behaviour is tested, like comparing Origins, the test should be written
 * as an Instrumentation test instead - you should never mock GURL.
 */
public class JUnitTestGURLs {
    // In order to add a test URL:
    // 1. Add the URL String as a constant here.
    // 2. Add the constant to the map below, with a placeholder string for the GURL serialization.
    // 3. Run JUnitTestGURLsTest (eg. './tools/autotest.py -C out/Debug JUnitTestGURLsTest').
    // 4. Check logcat output or test exception for the correct serialization String, and place it
    //    in the map.
    public static final String EXAMPLE_URL = "https://www.example.com";
    public static final String URL_1 = "https://www.one.com";
    public static final String URL_2 = "https://www.two.com";

    // Map of URL string to GURL serialization.
    /* package */ static final Map<String, String> sGURLMap;
    static {
        Map<String, String> map = new HashMap<>();
        map.put(EXAMPLE_URL,
                "82,1,true,0,5,0,-1,0,-1,8,15,0,-1,23,1,0,-1,0,-1,"
                        + "false,false,https://www.example.com/");
        map.put(URL_1,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://www.one.com/");
        map.put(URL_2,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://www.two.com/");
        sGURLMap = Collections.unmodifiableMap(map);
    }

    /**
     * @return the GURL resulting from parsing the provided url. Must be registered in |sGURLMap|.
     */
    public static GURL getGURL(String url) {
        String serialized = sGURLMap.get(url);
        if (serialized == null) {
            throw new IllegalArgumentException("URL " + url + " not found");
        }
        serialized = serialized.replace(',', GURL.SERIALIZER_DELIMITER);
        GURL gurl = GURL.deserialize(serialized);
        // If you're here looking to use an empty GURL, just use GURL.emptyGURL() directly.
        if (gurl.isEmpty()) {
            throw new RuntimeException("Could not deserialize: " + serialized);
        }
        return gurl;
    }
}
