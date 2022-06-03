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

- (BOOL)isValidURI
{
  // isValid() will only be true for valid, well-formed URI strings
  GURL testURL([self UTF8String]);

  // |javascript:| and |data:| URIs might not have passed the test,
  // but spaces will work OK, so evaluate them separately.
  if (testURL.is_valid() || [self isLooselyValidatedURI]) {
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

- (NSString *)stringByTrimmingWhitespace
{
  return [self stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

@end

