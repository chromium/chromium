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
    public static final String URL_1_WITH_PATH = "https://www.one.com/some_path.html";
    public static final String URL_2 = "https://www.two.com";
    public static final String SEARCH_URL = "https://www.google.com/search?q=test";
    public static final String SEARCH_2_URL = "https://www.google.com/search?q=query";
    public static final String INITIAL_URL = "https://initial.com";
    public static final String SPECULATED_URL = "https://speculated.com";
    public static final String NTP_URL = "chrome://newtab";
    public static final String DOM_DISILLER_URL = "distiller://url";
    public static final String RED_1 = "https://www.red.com/page1";
    public static final String RED_2 = "https://www.red.com/page2";
    public static final String RED_3 = "https://www.red.com/page3";
    public static final String BLUE_1 = "https://www.blue.com/page1";
    public static final String BLUE_2 = "https://www.blue.com/page2";
    public static final String BLUE_3 = "https://www.blue.com/page3";

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
        map.put(URL_1_WITH_PATH,
                "93,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,15,0,-1,0,-1,"
                        + "false,false,https://www.one.com/some_path.html");
        map.put(URL_2,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://www.two.com/");
        map.put(RED_1,
                "83,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,6,0,-1,0,-1,"
                        + "false,false,https://www.red.com/page1");
        map.put(RED_2,
                "83,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,6,0,-1,0,-1,"
                        + "false,false,https://www.red.com/page2");
        map.put(RED_3,
                "83,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,6,0,-1,0,-1,"
                        + "false,false,https://www.red.com/page3");
        map.put(BLUE_1,
                "84,1,true,0,5,0,-1,0,-1,8,12,0,-1,20,6,0,-1,0,-1,"
                        + "false,false,https://www.blue.com/page1");
        map.put(BLUE_2,
                "84,1,true,0,5,0,-1,0,-1,8,12,0,-1,20,6,0,-1,0,-1,"
                        + "false,false,https://www.blue.com/page2");
        map.put(BLUE_3,
                "84,1,true,0,5,0,-1,0,-1,8,12,0,-1,20,6,0,-1,0,-1,"
                        + "false,false,https://www.blue.com/page3");
        map.put(SEARCH_URL,
                "94,1,true,0,5,0,-1,0,-1,8,14,0,-1,22,7,30,6,0,-1,"
                        + "false,false,https://www.google.com/search?q=test");
        map.put(SEARCH_2_URL,
                "95,1,true,0,5,0,-1,0,-1,8,14,0,-1,22,7,30,7,0,-1,"
                        + "false,false,https://www.google.com/search?q=query");
        map.put(INITIAL_URL,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://initial.com/");
        map.put(SPECULATED_URL,
                "81,1,true,0,5,0,-1,0,-1,8,14,0,-1,22,1,0,-1,0,-1,"
                        + "false,false,https://speculated.com/");
        map.put(NTP_URL,
                "73,1,true,0,6,0,-1,0,-1,9,6,0,-1,15,1,0,-1,0,-1,"
                        + "false,false,chrome://newtab/");
        map.put(DOM_DISILLER_URL,
                "73,1,true,0,9,0,-1,0,-1,0,-1,0,-1,10,5,0,-1,0,-1,"
                        + "false,false,distiller://url");

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
