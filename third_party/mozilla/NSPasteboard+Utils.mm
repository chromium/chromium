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
 *   Bruce Davidson <Bruce.Davidson@ipl.com>
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

#import "NSPasteboard+Utils.h"
#import "NSURL+Utils.h"
#import "NSString+Utils.h"

NSString* const kCorePasteboardFlavorType_url  = @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
NSString* const kCorePasteboardFlavorType_urln = @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title
NSString* const kCorePasteboardFlavorType_urld = @"CorePasteboardFlavorType 0x75726C64"; // 'urld' URL description

NSString* const kWebURLsWithTitlesPboardType  = @"WebURLsWithTitlesPboardType"; // Safari-compatible URL + title arrays

@interface NSPasteboard(ChimeraPasteboardURLUtilsPrivate)

- (NSString*)cleanedStringWithPasteboardString:(NSString*)aString;

@end

@implementation NSPasteboard(ChimeraPasteboardURLUtilsPrivate)

//
// Utility method to ensure strings we're using in |containsURLData|
// and |getURLs:andTitles| are free of internal control characters
// and leading/trailing whitespace
//
- (NSString*)cleanedStringWithPasteboardString:(NSString*)aString
{
  NSString* cleanString = [aString stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
  return [cleanString stringByTrimmingWhitespace];
}

@end

@implementation NSPasteboard(ChimeraPasteboardURLUtils)

//
// Copy a single URL (with an optional title) to the clipboard in all relevant
// formats. Convenience method for clients that can only ever deal with one
// URL and shouldn't have to build up the arrays for setURLs:withTitles:.
//
- (void)setDataForURL:(NSString*)url title:(NSString*)title
{
  NSArray* urlList = [NSArray arrayWithObject:url];
  NSArray* titleList = nil;
  if (title)
    titleList = [NSArray arrayWithObject:title];
  
  [self setURLs:urlList withTitles:titleList];
}

//
// Copy a set of URLs, each of which may have a title, to the pasteboard
// using all the available formats.
// The title array should be nil, or must have the same length as the URL array.
//
- (void)setURLs:(NSArray*)inUrls withTitles:(NSArray*)inTitles
{
  unsigned int urlCount = [inUrls count];

  // Best format that we know about is Safari's URL + title arrays - build these up
  if (!inTitles) {
    NSMutableArray* tmpTitleArray = [NSMutableArray arrayWithCapacity:urlCount];
    for (unsigned int i = 0; i < urlCount; ++i)
      [tmpTitleArray addObject:[inUrls objectAtIndex:i]];
    inTitles = tmpTitleArray;
  }

  NSMutableArray* filePaths = [NSMutableArray array];
  for (unsigned int i = 0; i < urlCount; ++i) {
    NSURL* url = [NSURL URLWithString:[inUrls objectAtIndex:i]];
    if ([url isFileURL] && [[NSFileManager defaultManager] fileExistsAtPath:[url path]])
      [filePaths addObject:[url path]];
  }
  if ([filePaths count] > 0) {
    [self addTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
    [self setPropertyList:filePaths forType:NSFilenamesPboardType];
  }

  NSMutableArray* clipboardData = [NSMutableArray array];
  [clipboardData addObject:[NSArray arrayWithArray:inUrls]];
  [clipboardData addObject:inTitles];

  [self setPropertyList:clipboardData forType:kWebURLsWithTitlesPboardType];

  if (urlCount == 1) {
    NSString* url = [inUrls objectAtIndex:0];
    NSString* title = [inTitles objectAtIndex:0];

    [[NSURL URLWithString:url] writeToPasteboard:self];
    [self setString:url forType:NSStringPboardType];

    const char* tempCString = [url UTF8String];
    [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType:kCorePasteboardFlavorType_url];

    if (inTitles)
      tempCString = [title UTF8String];
    [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType:kCorePasteboardFlavorType_urln];
  }
  else if (urlCount > 1)
  {
    // With multiple URLs there aren't many other formats we can use
    // Just write a string of each URL (ignoring titles) on a separate line
    [self setString:[inUrls componentsJoinedByString:@"\n"] forType:NSStringPboardType];

    // but we have to put something in the carbon style flavors, otherwise apps will think
    // there is data there, but get nothing

    NSString* firstURL   = [inUrls objectAtIndex:0];
    NSString* firstTitle = [inTitles objectAtIndex:0];

    const char* tempCString = [firstURL UTF8String];
    [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType:kCorePasteboardFlavorType_url];

    tempCString = [firstTitle UTF8String];    // not i18n friendly
    [self setData:[NSData dataWithBytes:tempCString length:strlen(tempCString)] forType:kCorePasteboardFlavorType_urln];
  }
}

// Get the set of URLs and their corresponding titles from the pasteboard.
// If there are no URLs in a format we understand on the pasteboard empty
// arrays will be returned. The two arrays will always be the same size.
// The arrays returned are on the auto release pool. If |convertFilenames|
// is YES, then the function will attempt to convert filenames in the drag
// to file URLs.
- (void) getURLs:(NSArray**)outUrls
              andTitles:(NSArray**)outTitles
    convertingFilenames:(BOOL)convertFilenames
    convertingTextToURL:(BOOL)convertTextToURL
{
  NSArray* types = [self types];
  NSURL* urlFromNSURL = nil;  // Used below in getting an URL from the NSURLPboardType.
  if ([types containsObject:kWebURLsWithTitlesPboardType]) {
    NSArray* urlAndTitleContainer = [self propertyListForType:kWebURLsWithTitlesPboardType];
    if ([urlAndTitleContainer count] >= 2) {
      *outUrls = [urlAndTitleContainer objectAtIndex:0];
      *outTitles = [urlAndTitleContainer objectAtIndex:1];
      return;
    }
  }
  if ([types containsObject:NSFilenamesPboardType]) {
    NSArray *files = [self propertyListForType:NSFilenamesPboardType];
    *outUrls = [NSMutableArray arrayWithCapacity:[files count]];
    *outTitles = [NSMutableArray arrayWithCapacity:[files count]];
    for ( unsigned int i = 0; i < [files count]; ++i ) {
      NSString *file = [files objectAtIndex:i];
      NSString *ext = [[file pathExtension] lowercaseString];
      NSString *urlString = nil;
      NSString *title = @"";
      OSType fileType = NSHFSTypeCodeFromFileType(NSHFSTypeOfFile(file));
      
      // Check whether the file is a .webloc, a .ftploc, a .url, or some other kind of file.
      if ([ext isEqualToString:@"webloc"] || [ext isEqualToString:@"ftploc"] || fileType == 'ilht' || fileType == 'ilft') {
        NSURL* urlFromInetloc = [NSURL URLFromInetloc:file];
        if (urlFromInetloc) {
          urlString = [urlFromInetloc absoluteString];
          title     = [[file lastPathComponent] stringByDeletingPathExtension];
        }
      } else if ([ext isEqualToString:@"url"] || fileType == 'LINK') {
        NSURL* urlFromIEURLFile = [NSURL URLFromIEURLFile:file];
        if (urlFromIEURLFile) {
          urlString = [urlFromIEURLFile absoluteString];
          title     = [[file lastPathComponent] stringByDeletingPathExtension];
        }
      }
      
      if (!urlString) {
        if (!convertFilenames) {
          continue;
        }
        // Use the filename if not a .webloc or .url file, or if either of the
        // functions returns nil.
        urlString = [[NSURL fileURLWithPath:file] absoluteString];
        title     = [file lastPathComponent];
      }

      [(NSMutableArray*) *outUrls addObject:urlString];
      [(NSMutableArray*) *outTitles addObject:title];
    }
  } else if ([types containsObject:NSURLPboardType] && (urlFromNSURL = [NSURL URLFromPasteboard:self])) {
    *outUrls = [NSArray arrayWithObject:[urlFromNSURL absoluteString]];
    NSString* title = nil;
    if ([types containsObject:kCorePasteboardFlavorType_urld])
      title = [self stringForType:kCorePasteboardFlavorType_urld];
    if (!title && [types containsObject:kCorePasteboardFlavorType_urln])
      title = [self stringForType:kCorePasteboardFlavorType_urln];
    if (!title && [types containsObject:NSStringPboardType])
      title = [self stringForType:NSStringPboardType];
    *outTitles = [NSArray arrayWithObject:(title ? title : @"")];
  } else if (convertTextToURL && [types containsObject:NSStringPboardType]) {
    NSString* potentialURLString = [self cleanedStringWithPasteboardString:[self stringForType:NSStringPboardType]];
    if ([potentialURLString isValidURI]) {
      *outUrls = [NSArray arrayWithObject:potentialURLString];
      NSString* title = nil;
      if ([types containsObject:kCorePasteboardFlavorType_urld])
        title = [self stringForType:kCorePasteboardFlavorType_urld];
      if (!title && [types containsObject:kCorePasteboardFlavorType_urln])
        title = [self stringForType:kCorePasteboardFlavorType_urln];
      *outTitles = [NSArray arrayWithObject:(title ? title : @"")];
    } else {
      // The string doesn't look like a URL - return empty arrays
      *outUrls = [NSArray array];
      *outTitles = [NSArray array];
    }
  } else {
    // We don't recognise any of these formats - return empty arrays
    *outUrls = [NSArray array];
    *outTitles = [NSArray array];
  }
}

//
// Indicates if this pasteboard contains URL data that we understand
// Deals with all our URL formats. Only strings that are valid URLs count.
// If this returns YES it is safe to use getURLs:andTitles: to retrieve the data.
//
// NB: Does not consider our internal bookmark list format, because callers
// usually need to deal with this separately because it can include folders etc.
//
- (BOOL) containsURLDataConvertingTextToURL:(BOOL)convertTextToURL
{
  NSArray* types = [self types];
  if ([types containsObject:kWebURLsWithTitlesPboardType] ||
      [types containsObject:NSURLPboardType] ||
      [types containsObject:NSFilenamesPboardType])
    return YES;
  
  if (convertTextToURL && [types containsObject:NSStringPboardType]) {
    // Trim whitespace off the ends and newlines out of the middle so we don't reject otherwise-valid URLs;
    // we'll do another cleaning when we set the URLs and titles later, so this is safe.
    NSString* potentialURLString = [self cleanedStringWithPasteboardString:[self stringForType:NSStringPboardType]];
    return [potentialURLString isValidURI];
  }
  
  return NO;
}
@end
