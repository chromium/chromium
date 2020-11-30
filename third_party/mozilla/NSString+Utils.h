/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import <Foundation/Foundation.h>

typedef enum
{
  kTruncateAtStart,
  kTruncateAtMiddle,
  kTruncateAtEnd
} ETruncationType;


// a category to extend NSString
@interface NSString (ChimeraStringUtils)

+ (id)ellipsisString;
+ (NSString*)stringWithUUID;

- (BOOL)isEqualToStringIgnoringCase:(NSString*)inString;
- (BOOL)hasCaseInsensitivePrefix:(NSString*)inString;

// Some URIs can contain spaces and still work, even though they aren't strictly valid
// per RFC2396. This method allows us to account for those URIs.
- (BOOL)isLooselyValidatedURI;

// Utility method to identify URIs that can be run in the context of the current page.
// These URIs could be used as attack vectors via AppleScript, for example.
- (BOOL)isPotentiallyDangerousURI;

// Utility method to ensure validity of URI strings. NSURL is used to validate
// most of them, but the NSURL test may fail for |javascript:| and |data:| URIs
// because they often contain invalid (per RFC2396) characters such as spaces.
- (BOOL)isValidURI;

- (NSString *)stringByRemovingCharactersInSet:(NSCharacterSet*)characterSet;
- (NSString *)stringByReplacingCharactersInSet:(NSCharacterSet*)characterSet withString:(NSString*)string;
- (NSString *)stringByTruncatingTo:(unsigned int)maxCharacters at:(ETruncationType)truncationType;
- (NSString *)stringByTruncatingToWidth:(float)inWidth at:(ETruncationType)truncationType withAttributes:(NSDictionary *)attributes;
- (NSString *)stringByTrimmingWhitespace;
- (NSString *)stringByRemovingAmpEscapes;
- (NSString *)stringByAddingAmpEscapes;

@end

@interface NSMutableString (ChimeraMutableStringUtils)

- (void)truncateTo:(unsigned)maxCharacters at:(ETruncationType)truncationType;
- (void)truncateToWidth:(float)maxWidth at:(ETruncationType)truncationType withAttributes:(NSDictionary *)attributes;

@end

@interface NSString (ChimeraFilePathStringUtils)

- (NSString*)volumeNamePathComponent;
- (NSString*)displayNameOfLastPathComponent;

@end

@interface NSString (CaminoURLStringUtils)

// Returns true if the string represents a "blank" URL ("" or "about:blank")
- (BOOL)isBlankURL;
// Begin Google Modified
#if 0
// Returns a URI that looks good in a location field
- (NSString *)unescapedURI;
#endif
// End Google Modified

@end
