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
 *   David Haas   <haasd@cae.wisc.edu>
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

#import <AppKit/AppKit.h>		// for NSStringDrawing.h

#import "NSString+Utils.h"
#include "url/gurl.h"


@implementation NSString (ChimeraStringUtils)

+ (id)ellipsisString
{
  static NSString* sEllipsisString = nil;
  if (!sEllipsisString) {
    unichar ellipsisChar = 0x2026;
    sEllipsisString = [[NSString alloc] initWithCharacters:&ellipsisChar length:1];
  }

  return sEllipsisString;
}

+ (NSString*)stringWithUUID
{
  NSString* uuidString = nil;
  CFUUIDRef newUUID = CFUUIDCreate(kCFAllocatorDefault);
  if (newUUID) {
    uuidString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, newUUID);
    CFRelease(newUUID);
  }
  return [uuidString autorelease];
}

- (BOOL)isEqualToStringIgnoringCase:(NSString*)inString
{
  return ([self compare:inString options:NSCaseInsensitiveSearch] == NSOrderedSame);
}

- (BOOL)hasCaseInsensitivePrefix:(NSString*)inString
{
  if ([self length] < [inString length])
    return NO;
  return ([self compare:inString options:NSCaseInsensitiveSearch range:NSMakeRange(0, [inString length])] == NSOrderedSame);
}

- (BOOL)isLooselyValidatedURI
{
  return ([self hasCaseInsensitivePrefix:@"javascript:"] || [self hasCaseInsensitivePrefix:@"data:"]);
}

- (BOOL)isPotentiallyDangerousURI
{
  return ([self hasCaseInsensitivePrefix:@"javascript:"] || [self hasCaseInsensitivePrefix:@"data:"]);
}

- (BOOL)isValidURI
{
  // isValid() will only be true for valid, well-formed URI strings
  GURL testURL([self UTF8String]);

  // |javascript:| and |data:| URIs might not have passed the test,
  // but spaces will work OK, so evaluate them separately.
  if ((testURL.is_valid()) || [self isLooselyValidatedURI]) {
    return YES;
  }
  return NO;
}

- (NSString *)stringByRemovingCharactersInSet:(NSCharacterSet*)characterSet
{
  NSScanner*       cleanerScanner = [NSScanner scannerWithString:self];
  NSMutableString* cleanString    = [NSMutableString stringWithCapacity:[self length]];
  // Make sure we don't skip whitespace, which NSScanner does by default
  [cleanerScanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@""]];

  while (![cleanerScanner isAtEnd]) {
    NSString* stringFragment;
    if ([cleanerScanner scanUpToCharactersFromSet:characterSet intoString:&stringFragment])
      [cleanString appendString:stringFragment];

    [cleanerScanner scanCharactersFromSet:characterSet intoString:nil];
  }

  return cleanString;
}

- (NSString *)stringByReplacingCharactersInSet:(NSCharacterSet*)characterSet
                                    withString:(NSString*)string
{
  NSScanner*       cleanerScanner = [NSScanner scannerWithString:self];
  NSMutableString* cleanString    = [NSMutableString stringWithCapacity:[self length]];
  // Make sure we don't skip whitespace, which NSScanner does by default
  [cleanerScanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@""]];

  while (![cleanerScanner isAtEnd])
  {
    NSString* stringFragment;
    if ([cleanerScanner scanUpToCharactersFromSet:characterSet intoString:&stringFragment])
      [cleanString appendString:stringFragment];

    if ([cleanerScanner scanCharactersFromSet:characterSet intoString:nil])
      [cleanString appendString:string];
  }

  return cleanString;
}

- (NSString*)stringByTruncatingTo:(unsigned int)maxCharacters at:(ETruncationType)truncationType
{
  if ([self length] > maxCharacters)
  {
    NSMutableString *mutableCopy = [self mutableCopy];
    [mutableCopy truncateTo:maxCharacters at:truncationType];
    return [mutableCopy autorelease];
  }

  return self;
}

- (NSString *)stringByTruncatingToWidth:(float)inWidth at:(ETruncationType)truncationType
                         withAttributes:(NSDictionary *)attributes
{
  if ([self sizeWithAttributes:attributes].width > inWidth)
  {
    NSMutableString *mutableCopy = [self mutableCopy];
    [mutableCopy truncateToWidth:inWidth at:truncationType withAttributes:attributes];
    return [mutableCopy autorelease];
  }

  return self;
}

- (NSString *)stringByTrimmingWhitespace
{
  return [self stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

-(NSString *)stringByRemovingAmpEscapes
{
  NSMutableString* dirtyStringMutant = [NSMutableString stringWithString:self];
  [dirtyStringMutant replaceOccurrencesOfString:@"&amp;"
                                     withString:@"&"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&quot;"
                                     withString:@"\""
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&lt;"
                                     withString:@"<"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&gt;"
                                     withString:@">"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&mdash;"
                                     withString:@"-"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&apos;"
                                     withString:@"'"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  // fix import from old Firefox versions, which exported &#39; instead of a plain apostrophe
  [dirtyStringMutant replaceOccurrencesOfString:@"&#39;"
                                     withString:@"'"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  return [dirtyStringMutant stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
}

-(NSString *)stringByAddingAmpEscapes
{
  NSMutableString* dirtyStringMutant = [NSMutableString stringWithString:self];
  [dirtyStringMutant replaceOccurrencesOfString:@"&"
                                     withString:@"&amp;"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"\""
                                     withString:@"&quot;"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"<"
                                     withString:@"&lt;"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@">"
                                     withString:@"&gt;"
                                        options:NSLiteralSearch
                                          range:NSMakeRange(0,[dirtyStringMutant length])];
  return [NSString stringWithString:dirtyStringMutant];
}

@end


@implementation NSMutableString (ChimeraMutableStringUtils)

- (void)truncateTo:(unsigned)maxCharacters at:(ETruncationType)truncationType
{
  if ([self length] <= maxCharacters)
    return;

  NSRange replaceRange;
  replaceRange.length = [self length] - maxCharacters;

  switch (truncationType) {
    case kTruncateAtStart:
      replaceRange.location = 0;
      break;

    case kTruncateAtMiddle:
      replaceRange.location = maxCharacters / 2;
      break;

    case kTruncateAtEnd:
      replaceRange.location = maxCharacters;
      break;

    default:
#if DEBUG
      NSLog(@"Unknown truncation type in stringByTruncatingTo::");
#endif
      replaceRange.location = maxCharacters;
      break;
  }

  [self replaceCharactersInRange:replaceRange withString:[NSString ellipsisString]];
}


- (void)truncateToWidth:(float)maxWidth
                     at:(ETruncationType)truncationType
         withAttributes:(NSDictionary *)attributes
{
  // First check if we have to truncate at all.
  if ([self sizeWithAttributes:attributes].width <= maxWidth)
    return;

  // Essentially, we perform a binary search on the string length
  // which fits best into maxWidth.

  float width = maxWidth;
  int lo = 0;
  int hi = [self length];
  int mid;

  // Make a backup copy of the string so that we can restore it if we fail low.
  NSMutableString *backup = [self mutableCopy];

  while (hi >= lo) {
    mid = (hi + lo) / 2;

    // Cut to mid chars and calculate the resulting width
    [self truncateTo:mid at:truncationType];
    width = [self sizeWithAttributes:attributes].width;

    if (width > maxWidth) {
      // Fail high - string is still to wide. For the next cut, we can simply
      // work on the already cut string, so we don't restore using the backup.
      hi = mid - 1;
    }
    else if (width == maxWidth) {
      // Perfect match, abort the search.
      break;
    }
    else {
      // Fail low - we cut off too much. Restore the string before cutting again.
      lo = mid + 1;
      [self setString:backup];
    }
  }
  // Perform the final cut (unless this was already a perfect match).
  if (width != maxWidth)
    [self truncateTo:hi at:truncationType];
  [backup release];
}

@end

@implementation NSString (ChimeraFilePathStringUtils)

- (NSString*)volumeNamePathComponent
{
  // if the file doesn't exist, then componentsToDisplayForPath will return nil,
  // so back up to the nearest existing dir
  NSString* curPath = self;
  while (![[NSFileManager defaultManager] fileExistsAtPath:curPath])
  {
    NSString* parentDirPath = [curPath stringByDeletingLastPathComponent];
    if ([parentDirPath isEqualToString:curPath])
      break;  // avoid endless loop
    curPath = parentDirPath;
  }

  NSArray* displayComponents = [[NSFileManager defaultManager] componentsToDisplayForPath:curPath];
  if ([displayComponents count] > 0)
    return [displayComponents objectAtIndex:0];

  return self;
}

- (NSString*)displayNameOfLastPathComponent
{
  return [[NSFileManager defaultManager] displayNameAtPath:self];
}

@end

@implementation NSString (CaminoURLStringUtils)

- (BOOL)isBlankURL
{
  return ([self isEqualToString:@"about:blank"] || [self isEqualToString:@""]);
}

// Begin Google Modified
#if 0
// Excluded character list comes from RFC2396 and by examining Safari's behaviour
- (NSString*)unescapedURI
{
  NSString *unescapedURI = (NSString*)CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault,
                                                                            (CFStringRef)self,
                                                                            CFSTR(" \"\';/?:@&=+$,#"),
                                                                            kCFStringEncodingUTF8);
  return unescapedURI ? [unescapedURI autorelease] : self;
}
#endif
// End Google Modified

@end
