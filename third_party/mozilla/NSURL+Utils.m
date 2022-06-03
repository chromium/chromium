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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nate Weaver (Wevah) - wevah@derailer.org
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

#import "NSURL+Utils.h"


@implementation NSURL (CaminoExtensions)

//
// Reads the URL from a .webloc/.ftploc file.
// Returns the URL, or nil on failure.
//
+(NSURL*)URLFromInetloc:(NSString*)inFile
{
  //// Begin Google Modified
  NSDictionary *plist = [NSDictionary dictionaryWithContentsOfFile:inFile];
  return [NSURL URLWithString:[plist objectForKey:@"URL"]];
  //// End Google Modified
}

//
// Reads the URL from a .url file.
// Returns the URL or nil on failure.
//
+(NSURL*)URLFromIEURLFile:(NSString*)inFile
{
  NSURL *ret = nil;
  
  // Is this really an IE .url file?
  if (inFile) {
    NSCharacterSet *newlines = [NSCharacterSet characterSetWithCharactersInString:@"\r\n"];
    // Begin Google Modified
//    NSScanner *scanner = [NSScanner scannerWithString:[NSString stringWithContentsOfFile:inFile]];
    NSString *fileString = [NSString stringWithContentsOfFile:inFile
                                                     encoding:NSWindowsCP1252StringEncoding  // best guess here
                                                        error:nil];
    NSScanner *scanner = [NSScanner scannerWithString:fileString];
    // End Google Modified
    [scanner scanUpToString:@"[InternetShortcut]" intoString:nil];
    
    if ([scanner scanString:@"[InternetShortcut]" intoString:nil]) {
      // Scan each non-empty line in this section. We don't need to explicitly scan the newlines or
      // whitespace because NSScanner ignores these by default.
      NSString *line;
      
      while ([scanner scanUpToCharactersFromSet:newlines intoString:&line]) {
        if ([line hasPrefix:@"URL="]) {
          ret = [NSURL URLWithString:[line substringFromIndex:4]];
          break;
        }
        else if ([line hasPrefix:@"["]) {
          // This is the start of a new section, so if we haven't found an URL yet, we should bail.
          break;
        }
      }
    }
  }
  
  return ret;
}

@end
