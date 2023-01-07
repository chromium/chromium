// Copyright 2020 The Chromium Authors
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
    public static final String EXAMPLE_URL = "https://www.example.com/";
    public static final String HTTP_URL = "http://www.example.com/";
    public static final String URL_1 = "https://www.one.com/";
    public static final String URL_1_NUMERAL = "https://www.1.com/";
    public static final String URL_1_WITH_PATH = "https://www.one.com/some_path.html";
    public static final String URL_2 = "https://www.two.com/";
    public static final String URL_3 = "https://www.three.com/";
    public static final String MAPS_URL = "https://maps.google.com/";
    public static final String SEARCH_URL = "https://www.google.com/search?q=test";
    public static final String SEARCH_2_URL = "https://www.google.com/search?q=query";
    public static final String INITIAL_URL = "https://initial.com";
    public static final String SPECULATED_URL = "https://speculated.com";
    public static final String NTP_URL = "chrome://newtab/";
    public static final String DOM_DISILLER_URL = "distiller://url";
    public static final String RED_1 = "https://www.red.com/page1";
    public static final String RED_2 = "https://www.red.com/page2";
    public static final String RED_3 = "https://www.red.com/page3";
    public static final String BLUE_1 = "https://www.blue.com/page1";
    public static final String BLUE_2 = "https://www.blue.com/page2";
    public static final String BLUE_3 = "https://www.blue.com/page3";
    public static final String AMP_URL =
            "https://www.google.com/amp/www.nyt.com/ampthml/blogs.html";
    public static final String AMP_CACHE_URL =
            "https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html";
    public static final String TEXT_FRAGMENT_URL = "https://www.example.com/#:~:text=selector";
    public static final String MULTI_TEXT_FRAGMENT_URL =
            "https://www.example.com/#:~:text=selector1&text=selector2&text=selector3";
    public static final String INVALID_URL = "http://0x100.0/";
    public static final String GOOGLE_URL = "http://www.google.com/";
    public static final String GOOGLE_URL_DOGS = "http://www.google.com/dogs";
    public static final String GOOGLE_URL_DOGS_FUN = "http://www.google.com/dogs-are-fun";
    public static final String GOOGLE_URL_DOG = "http://www.google.com/dog";
    public static final String GOOGLE_URL_CAT = "http://www.google.com/cat";
    public static final String GOOGLE_URL_PIG = "http://www.google.com/pig";
    public static final String ABOUT_BLANK = "about:blank";
    public static final String CHROME_ABOUT = "chrome://about";

    // Map of URL string to GURL serialization.
    /* package */ static final Map<String, String> sGURLMap;
    static {
        Map<String, String> map = new HashMap<>();
        map.put(EXAMPLE_URL,
                "82,1,true,0,5,0,-1,0,-1,8,15,0,-1,23,1,0,-1,0,-1,"
                        + "false,false,https://www.example.com/");
        map.put(HTTP_URL,
                "81,1,true,0,4,0,-1,0,-1,7,15,0,-1,22,1,0,-1,0,-1,"
                        + "false,false,http://www.example.com/");
        map.put(URL_1,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://www.one.com/");
        map.put(URL_1_NUMERAL,
                "75,1,true,0,5,0,-1,0,-1,8,9,0,-1,17,1,0,-1,0,-1,"
                        + "false,false,https://www.1.com/");
        map.put(URL_1_WITH_PATH,
                "93,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,15,0,-1,0,-1,"
                        + "false,false,https://www.one.com/some_path.html");
        map.put(URL_2,
                "78,1,true,0,5,0,-1,0,-1,8,11,0,-1,19,1,0,-1,0,-1,"
                        + "false,false,https://www.two.com/");
        map.put(URL_3,
                "80,1,true,0,5,0,-1,0,-1,8,13,0,-1,21,1,0,-1,0,-1,false,false,https://www.three.com/");
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
        map.put(MAPS_URL,
                "82,1,true,0,5,0,-1,0,-1,8,15,0,-1,23,1,0,-1,0,-1,false,false,https://maps.google.com/");
        map.put(AMP_URL,
                "116,1,true,0,5,0,-1,0,-1,8,14,0,-1,22,35,0,-1,0,-1,false,false,https://www.google.com/amp/www.nyt.com/ampthml/blogs.html");
        map.put(AMP_CACHE_URL,
                "118,1,true,0,5,0,-1,0,-1,8,14,0,-1,22,37,0,-1,0,-1,false,false,https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html");
        map.put(TEXT_FRAGMENT_URL,
                "100,1,true,0,5,0,-1,0,-1,8,15,0,-1,23,1,0,-1,25,16,false,false,https://www.example.com/#:~:text=selector");
        map.put(MULTI_TEXT_FRAGMENT_URL,
                "131,1,true,0,5,0,-1,0,-1,8,15,0,-1,23,1,0,-1,25,47,false,false,https://www.example.com/#:~:text=selector1&text=selector2&text=selector3");
        map.put(INVALID_URL,
                "73,1,false,0,4,0,-1,0,-1,7,7,0,-1,14,1,0,-1,0,-1,false,false,http://0x100.0/");
        map.put(GOOGLE_URL,
                "80,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,1,0,-1,0,-1,false,false,http://www.google.com/");
        map.put(GOOGLE_URL_DOGS,
                "84,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,5,0,-1,0,-1,false,false,http://www.google.com/dogs");
        map.put(GOOGLE_URL_DOGS_FUN,
                "93,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,13,0,-1,0,-1,false,false,http://www.google.com/dogs-are-fun");
        map.put(GOOGLE_URL_DOG,
                "83,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,4,0,-1,0,-1,false,false,http://www.google.com/dog");
        map.put(GOOGLE_URL_CAT,
                "83,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,4,0,-1,0,-1,false,false,http://www.google.com/cat");
        map.put(GOOGLE_URL_PIG,
                "83,1,true,0,4,0,-1,0,-1,7,14,0,-1,21,4,0,-1,0,-1,false,false,http://www.google.com/pig");
        map.put(ABOUT_BLANK,
                "68,1,true,0,5,0,-1,0,-1,0,-1,0,-1,6,5,0,-1,0,-1,false,false,about:blank");
        map.put(CHROME_ABOUT,
                "72,1,true,0,6,0,-1,0,-1,9,5,0,-1,14,1,0,-1,0,-1,false,false,chrome://about/");
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
