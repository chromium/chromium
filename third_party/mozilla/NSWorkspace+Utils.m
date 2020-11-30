/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
 *   Josh Aas <josh@mozilla.com>
 *   Nick Kreeger <nick.kreeger@park.edu>
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

#import "NSWorkspace+Utils.h"

@implementation NSWorkspace(CaminoDefaultBrowserAdditions)

- (NSArray*)installedBrowserIdentifiers
{
  NSArray* apps = [(NSArray*)LSCopyAllHandlersForURLScheme(CFSTR("https")) autorelease];

  // add the default if it isn't there
  NSString* defaultHandler = [self defaultBrowserIdentifier];
  if (defaultHandler && ([apps indexOfObject:defaultHandler] == NSNotFound))
    apps = [apps arrayByAddingObject:defaultHandler];

  return apps;
}

- (NSArray*)installedFeedViewerIdentifiers
{
  NSArray* apps = [(NSArray*)LSCopyAllHandlersForURLScheme(CFSTR("feed")) autorelease];

  // add the default if it isn't there
  NSString* defaultHandler = [self defaultFeedViewerIdentifier];
  if (defaultHandler && ([apps indexOfObject:defaultHandler] == NSNotFound))
    apps = [apps arrayByAddingObject:defaultHandler];

  return apps;
}

- (NSString*)defaultBrowserIdentifier
{
  NSString* defaultBundleId = [(NSString*)LSCopyDefaultHandlerForURLScheme(CFSTR("http")) autorelease];
  // Sometimes LaunchServices likes to pretend there's no default browser.
  // If that happens, we'll assume it's probably Safari.
  if (!defaultBundleId)
    defaultBundleId = @"com.apple.safari";
  return defaultBundleId;
}

- (NSString*)defaultFeedViewerIdentifier
{
  return [(NSString*)LSCopyDefaultHandlerForURLScheme(CFSTR("feed")) autorelease];
}

- (void)setDefaultBrowserWithIdentifier:(NSString*)bundleID
{
  LSSetDefaultHandlerForURLScheme(CFSTR("http"), (CFStringRef)bundleID);
  LSSetDefaultHandlerForURLScheme(CFSTR("https"), (CFStringRef)bundleID);
  LSSetDefaultRoleHandlerForContentType(kUTTypeHTML, kLSRolesViewer, (CFStringRef)bundleID);
  LSSetDefaultRoleHandlerForContentType(kUTTypeURL, kLSRolesViewer, (CFStringRef)bundleID);
}

- (void)setDefaultFeedViewerWithIdentifier:(NSString*)bundleID
{
  LSSetDefaultHandlerForURLScheme(CFSTR("feed"), (CFStringRef)bundleID);
}

- (NSString*)identifierForBundle:(NSURL*)inBundleURL
{
  if (!inBundleURL) return nil;

  NSBundle* tmpBundle = [NSBundle bundleWithPath:[[inBundleURL path] stringByStandardizingPath]];
  if (tmpBundle)
  {
    NSString* tmpBundleID = [tmpBundle bundleIdentifier];
    if (tmpBundleID && ([tmpBundleID length] > 0)) {
      return tmpBundleID;
    }
  }
  return nil;
}

- (NSString*)displayNameForFile:(NSURL*)inFileURL
{
  NSString *name = nil;
  [inFileURL getResourceValue:&name forKey:NSURLLocalizedNameKey error:nil];
  return name;
}

//
// +osVersionString
//
// Returns the system version string from
// /System/Library/CoreServices/SystemVersion.plist
// (as recommended by Apple).
//
+ (NSString*)osVersionString
{
  NSDictionary* versionInfo = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
  return [versionInfo objectForKey:@"ProductVersion"];
}

// Begin Google Modified
#if 0
//
// +systemVersion
//
// Returns the host's OS version as returned by the 'sysv' gestalt selector,
// 10.x.y = 0x000010xy
//
+ (long)systemVersion
{
  static SInt32 sSystemVersion = 0;
  if (!sSystemVersion)
    Gestalt(gestaltSystemVersion, &sSystemVersion);
  return (long)sSystemVersion;
}

//
// +isLeopardOrHigher
//
// returns YES if we're on 10.5 or better
//
+ (BOOL)isLeopardOrHigher
{
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_4
  return YES;
#else
  return [self systemVersion] >= 0x1050;
#endif
}
#endif
// End Google Modified

@end
