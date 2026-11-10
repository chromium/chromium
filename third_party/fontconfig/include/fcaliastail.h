#ifndef FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT
#define FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT __attribute((visibility("hidden")))
#endif
#if HAVE_GNUC_ATTRIBUTE
#ifdef __fccfg__
# undef FcBlanksCreate
extern __typeof (FcBlanksCreate) FcBlanksCreate __attribute((alias("IA__FcBlanksCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcBlanksCreate) chromium_FcBlanksCreate __attribute((alias("IA__FcBlanksCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcBlanksDestroy
extern __typeof (FcBlanksDestroy) FcBlanksDestroy __attribute((alias("IA__FcBlanksDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcBlanksDestroy) chromium_FcBlanksDestroy __attribute((alias("IA__FcBlanksDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcBlanksAdd
extern __typeof (FcBlanksAdd) FcBlanksAdd __attribute((alias("IA__FcBlanksAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcBlanksAdd) chromium_FcBlanksAdd __attribute((alias("IA__FcBlanksAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcBlanksIsMember
extern __typeof (FcBlanksIsMember) FcBlanksIsMember __attribute((alias("IA__FcBlanksIsMember"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcBlanksIsMember) chromium_FcBlanksIsMember __attribute((alias("IA__FcBlanksIsMember"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccfg__ */
#ifdef __fccache__
# undef FcCacheCopySet
extern __typeof (FcCacheCopySet) FcCacheCopySet __attribute((alias("IA__FcCacheCopySet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCacheCopySet) chromium_FcCacheCopySet __attribute((alias("IA__FcCacheCopySet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCacheNumSubdir
extern __typeof (FcCacheNumSubdir) FcCacheNumSubdir __attribute((alias("IA__FcCacheNumSubdir"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCacheNumSubdir) chromium_FcCacheNumSubdir __attribute((alias("IA__FcCacheNumSubdir"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCacheNumFont
extern __typeof (FcCacheNumFont) FcCacheNumFont __attribute((alias("IA__FcCacheNumFont"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCacheNumFont) chromium_FcCacheNumFont __attribute((alias("IA__FcCacheNumFont"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheUnlink
extern __typeof (FcDirCacheUnlink) FcDirCacheUnlink __attribute((alias("IA__FcDirCacheUnlink"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheUnlink) chromium_FcDirCacheUnlink __attribute((alias("IA__FcDirCacheUnlink"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheValid
extern __typeof (FcDirCacheValid) FcDirCacheValid __attribute((alias("IA__FcDirCacheValid"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheValid) chromium_FcDirCacheValid __attribute((alias("IA__FcDirCacheValid"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheClean
extern __typeof (FcDirCacheClean) FcDirCacheClean __attribute((alias("IA__FcDirCacheClean"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheClean) chromium_FcDirCacheClean __attribute((alias("IA__FcDirCacheClean"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCacheCreateTagFile
extern __typeof (FcCacheCreateTagFile) FcCacheCreateTagFile __attribute((alias("IA__FcCacheCreateTagFile"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCacheCreateTagFile) chromium_FcCacheCreateTagFile __attribute((alias("IA__FcCacheCreateTagFile"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheCreateUUID
extern __typeof (FcDirCacheCreateUUID) FcDirCacheCreateUUID __attribute((alias("IA__FcDirCacheCreateUUID"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheCreateUUID) chromium_FcDirCacheCreateUUID __attribute((alias("IA__FcDirCacheCreateUUID"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheDeleteUUID
extern __typeof (FcDirCacheDeleteUUID) FcDirCacheDeleteUUID __attribute((alias("IA__FcDirCacheDeleteUUID"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheDeleteUUID) chromium_FcDirCacheDeleteUUID __attribute((alias("IA__FcDirCacheDeleteUUID"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccache__ */
#ifdef __fccfg__
# undef FcConfigHome
extern __typeof (FcConfigHome) FcConfigHome __attribute((alias("IA__FcConfigHome"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigHome) chromium_FcConfigHome __attribute((alias("IA__FcConfigHome"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigEnableHome
extern __typeof (FcConfigEnableHome) FcConfigEnableHome __attribute((alias("IA__FcConfigEnableHome"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigEnableHome) chromium_FcConfigEnableHome __attribute((alias("IA__FcConfigEnableHome"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetFilename
extern __typeof (FcConfigGetFilename) FcConfigGetFilename __attribute((alias("IA__FcConfigGetFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetFilename) chromium_FcConfigGetFilename __attribute((alias("IA__FcConfigGetFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigFilename
extern __typeof (FcConfigFilename) FcConfigFilename __attribute((alias("IA__FcConfigFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigFilename) chromium_FcConfigFilename __attribute((alias("IA__FcConfigFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigCreate
extern __typeof (FcConfigCreate) FcConfigCreate __attribute((alias("IA__FcConfigCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigCreate) chromium_FcConfigCreate __attribute((alias("IA__FcConfigCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigReference
extern __typeof (FcConfigReference) FcConfigReference __attribute((alias("IA__FcConfigReference"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigReference) chromium_FcConfigReference __attribute((alias("IA__FcConfigReference"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigDestroy
extern __typeof (FcConfigDestroy) FcConfigDestroy __attribute((alias("IA__FcConfigDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigDestroy) chromium_FcConfigDestroy __attribute((alias("IA__FcConfigDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetCurrent
extern __typeof (FcConfigSetCurrent) FcConfigSetCurrent __attribute((alias("IA__FcConfigSetCurrent"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetCurrent) chromium_FcConfigSetCurrent __attribute((alias("IA__FcConfigSetCurrent"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetCurrent
extern __typeof (FcConfigGetCurrent) FcConfigGetCurrent __attribute((alias("IA__FcConfigGetCurrent"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetCurrent) chromium_FcConfigGetCurrent __attribute((alias("IA__FcConfigGetCurrent"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigUptoDate
extern __typeof (FcConfigUptoDate) FcConfigUptoDate __attribute((alias("IA__FcConfigUptoDate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigUptoDate) chromium_FcConfigUptoDate __attribute((alias("IA__FcConfigUptoDate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigBuildFonts
extern __typeof (FcConfigBuildFonts) FcConfigBuildFonts __attribute((alias("IA__FcConfigBuildFonts"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigBuildFonts) chromium_FcConfigBuildFonts __attribute((alias("IA__FcConfigBuildFonts"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetFontDirs
extern __typeof (FcConfigGetFontDirs) FcConfigGetFontDirs __attribute((alias("IA__FcConfigGetFontDirs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetFontDirs) chromium_FcConfigGetFontDirs __attribute((alias("IA__FcConfigGetFontDirs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetAppFonts
extern __typeof (FcConfigGetAppFonts) FcConfigGetAppFonts __attribute((alias("IA__FcConfigGetAppFonts"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetAppFonts) chromium_FcConfigGetAppFonts __attribute((alias("IA__FcConfigGetAppFonts"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetConfigDirs
extern __typeof (FcConfigGetConfigDirs) FcConfigGetConfigDirs __attribute((alias("IA__FcConfigGetConfigDirs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetConfigDirs) chromium_FcConfigGetConfigDirs __attribute((alias("IA__FcConfigGetConfigDirs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetConfigFiles
extern __typeof (FcConfigGetConfigFiles) FcConfigGetConfigFiles __attribute((alias("IA__FcConfigGetConfigFiles"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetConfigFiles) chromium_FcConfigGetConfigFiles __attribute((alias("IA__FcConfigGetConfigFiles"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetCache
extern __typeof (FcConfigGetCache) FcConfigGetCache __attribute((alias("IA__FcConfigGetCache"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetCache) chromium_FcConfigGetCache __attribute((alias("IA__FcConfigGetCache"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetBlanks
extern __typeof (FcConfigGetBlanks) FcConfigGetBlanks __attribute((alias("IA__FcConfigGetBlanks"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetBlanks) chromium_FcConfigGetBlanks __attribute((alias("IA__FcConfigGetBlanks"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetCacheDirs
extern __typeof (FcConfigGetCacheDirs) FcConfigGetCacheDirs __attribute((alias("IA__FcConfigGetCacheDirs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetCacheDirs) chromium_FcConfigGetCacheDirs __attribute((alias("IA__FcConfigGetCacheDirs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetRescanInterval
extern __typeof (FcConfigGetRescanInterval) FcConfigGetRescanInterval __attribute((alias("IA__FcConfigGetRescanInterval"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetRescanInterval) chromium_FcConfigGetRescanInterval __attribute((alias("IA__FcConfigGetRescanInterval"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetRescanInterval
extern __typeof (FcConfigSetRescanInterval) FcConfigSetRescanInterval __attribute((alias("IA__FcConfigSetRescanInterval"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetRescanInterval) chromium_FcConfigSetRescanInterval __attribute((alias("IA__FcConfigSetRescanInterval"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetFonts
extern __typeof (FcConfigGetFonts) FcConfigGetFonts __attribute((alias("IA__FcConfigGetFonts"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetFonts) chromium_FcConfigGetFonts __attribute((alias("IA__FcConfigGetFonts"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAcceptFont
extern __typeof (FcConfigAcceptFont) FcConfigAcceptFont __attribute((alias("IA__FcConfigAcceptFont"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAcceptFont) chromium_FcConfigAcceptFont __attribute((alias("IA__FcConfigAcceptFont"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAcceptFilename
extern __typeof (FcConfigAcceptFilename) FcConfigAcceptFilename __attribute((alias("IA__FcConfigAcceptFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAcceptFilename) chromium_FcConfigAcceptFilename __attribute((alias("IA__FcConfigAcceptFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAcceptFilter
extern __typeof (FcConfigAcceptFilter) FcConfigAcceptFilter __attribute((alias("IA__FcConfigAcceptFilter"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAcceptFilter) chromium_FcConfigAcceptFilter __attribute((alias("IA__FcConfigAcceptFilter"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAppFontAddFile
extern __typeof (FcConfigAppFontAddFile) FcConfigAppFontAddFile __attribute((alias("IA__FcConfigAppFontAddFile"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAppFontAddFile) chromium_FcConfigAppFontAddFile __attribute((alias("IA__FcConfigAppFontAddFile"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAppFontAddDir
extern __typeof (FcConfigAppFontAddDir) FcConfigAppFontAddDir __attribute((alias("IA__FcConfigAppFontAddDir"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAppFontAddDir) chromium_FcConfigAppFontAddDir __attribute((alias("IA__FcConfigAppFontAddDir"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigAppFontClear
extern __typeof (FcConfigAppFontClear) FcConfigAppFontClear __attribute((alias("IA__FcConfigAppFontClear"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigAppFontClear) chromium_FcConfigAppFontClear __attribute((alias("IA__FcConfigAppFontClear"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigPreferAppFont
extern __typeof (FcConfigPreferAppFont) FcConfigPreferAppFont __attribute((alias("IA__FcConfigPreferAppFont"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigPreferAppFont) chromium_FcConfigPreferAppFont __attribute((alias("IA__FcConfigPreferAppFont"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetWarningFlags
extern __typeof (FcConfigSetWarningFlags) FcConfigSetWarningFlags __attribute((alias("IA__FcConfigSetWarningFlags"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetWarningFlags) chromium_FcConfigSetWarningFlags __attribute((alias("IA__FcConfigSetWarningFlags"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetWarningFlags
extern __typeof (FcConfigGetWarningFlags) FcConfigGetWarningFlags __attribute((alias("IA__FcConfigGetWarningFlags"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetWarningFlags) chromium_FcConfigGetWarningFlags __attribute((alias("IA__FcConfigGetWarningFlags"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSubstituteWithPat
extern __typeof (FcConfigSubstituteWithPat) FcConfigSubstituteWithPat __attribute((alias("IA__FcConfigSubstituteWithPat"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSubstituteWithPat) chromium_FcConfigSubstituteWithPat __attribute((alias("IA__FcConfigSubstituteWithPat"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSubstitute
extern __typeof (FcConfigSubstitute) FcConfigSubstitute __attribute((alias("IA__FcConfigSubstitute"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSubstitute) chromium_FcConfigSubstitute __attribute((alias("IA__FcConfigSubstitute"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigGetSysRoot
extern __typeof (FcConfigGetSysRoot) FcConfigGetSysRoot __attribute((alias("IA__FcConfigGetSysRoot"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetSysRoot) chromium_FcConfigGetSysRoot __attribute((alias("IA__FcConfigGetSysRoot"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetSysRoot
extern __typeof (FcConfigSetSysRoot) FcConfigSetSysRoot __attribute((alias("IA__FcConfigSetSysRoot"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetSysRoot) chromium_FcConfigSetSysRoot __attribute((alias("IA__FcConfigSetSysRoot"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetFontSetFilter
extern __typeof (FcConfigSetFontSetFilter) FcConfigSetFontSetFilter __attribute((alias("IA__FcConfigSetFontSetFilter"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetFontSetFilter) chromium_FcConfigSetFontSetFilter __attribute((alias("IA__FcConfigSetFontSetFilter"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigFileInfoIterInit
extern __typeof (FcConfigFileInfoIterInit) FcConfigFileInfoIterInit __attribute((alias("IA__FcConfigFileInfoIterInit"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigFileInfoIterInit) chromium_FcConfigFileInfoIterInit __attribute((alias("IA__FcConfigFileInfoIterInit"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigFileInfoIterNext
extern __typeof (FcConfigFileInfoIterNext) FcConfigFileInfoIterNext __attribute((alias("IA__FcConfigFileInfoIterNext"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigFileInfoIterNext) chromium_FcConfigFileInfoIterNext __attribute((alias("IA__FcConfigFileInfoIterNext"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigFileInfoIterGet
extern __typeof (FcConfigFileInfoIterGet) FcConfigFileInfoIterGet __attribute((alias("IA__FcConfigFileInfoIterGet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigFileInfoIterGet) chromium_FcConfigFileInfoIterGet __attribute((alias("IA__FcConfigFileInfoIterGet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccfg__ */
#ifdef __fccharset__
# undef FcCharSetCreate
extern __typeof (FcCharSetCreate) FcCharSetCreate __attribute((alias("IA__FcCharSetCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetCreate) chromium_FcCharSetCreate __attribute((alias("IA__FcCharSetCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetNew
extern __typeof (FcCharSetNew) FcCharSetNew __attribute((alias("IA__FcCharSetNew"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetNew) chromium_FcCharSetNew __attribute((alias("IA__FcCharSetNew"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetDestroy
extern __typeof (FcCharSetDestroy) FcCharSetDestroy __attribute((alias("IA__FcCharSetDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetDestroy) chromium_FcCharSetDestroy __attribute((alias("IA__FcCharSetDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetAddChar
extern __typeof (FcCharSetAddChar) FcCharSetAddChar __attribute((alias("IA__FcCharSetAddChar"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetAddChar) chromium_FcCharSetAddChar __attribute((alias("IA__FcCharSetAddChar"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetDelChar
extern __typeof (FcCharSetDelChar) FcCharSetDelChar __attribute((alias("IA__FcCharSetDelChar"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetDelChar) chromium_FcCharSetDelChar __attribute((alias("IA__FcCharSetDelChar"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetCopy
extern __typeof (FcCharSetCopy) FcCharSetCopy __attribute((alias("IA__FcCharSetCopy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetCopy) chromium_FcCharSetCopy __attribute((alias("IA__FcCharSetCopy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetEqual
extern __typeof (FcCharSetEqual) FcCharSetEqual __attribute((alias("IA__FcCharSetEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetEqual) chromium_FcCharSetEqual __attribute((alias("IA__FcCharSetEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetIntersect
extern __typeof (FcCharSetIntersect) FcCharSetIntersect __attribute((alias("IA__FcCharSetIntersect"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetIntersect) chromium_FcCharSetIntersect __attribute((alias("IA__FcCharSetIntersect"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetUnion
extern __typeof (FcCharSetUnion) FcCharSetUnion __attribute((alias("IA__FcCharSetUnion"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetUnion) chromium_FcCharSetUnion __attribute((alias("IA__FcCharSetUnion"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetSubtract
extern __typeof (FcCharSetSubtract) FcCharSetSubtract __attribute((alias("IA__FcCharSetSubtract"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetSubtract) chromium_FcCharSetSubtract __attribute((alias("IA__FcCharSetSubtract"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetMerge
extern __typeof (FcCharSetMerge) FcCharSetMerge __attribute((alias("IA__FcCharSetMerge"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetMerge) chromium_FcCharSetMerge __attribute((alias("IA__FcCharSetMerge"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetHasChar
extern __typeof (FcCharSetHasChar) FcCharSetHasChar __attribute((alias("IA__FcCharSetHasChar"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetHasChar) chromium_FcCharSetHasChar __attribute((alias("IA__FcCharSetHasChar"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetCount
extern __typeof (FcCharSetCount) FcCharSetCount __attribute((alias("IA__FcCharSetCount"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetCount) chromium_FcCharSetCount __attribute((alias("IA__FcCharSetCount"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetIntersectCount
extern __typeof (FcCharSetIntersectCount) FcCharSetIntersectCount __attribute((alias("IA__FcCharSetIntersectCount"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetIntersectCount) chromium_FcCharSetIntersectCount __attribute((alias("IA__FcCharSetIntersectCount"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetSubtractCount
extern __typeof (FcCharSetSubtractCount) FcCharSetSubtractCount __attribute((alias("IA__FcCharSetSubtractCount"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetSubtractCount) chromium_FcCharSetSubtractCount __attribute((alias("IA__FcCharSetSubtractCount"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetIsSubset
extern __typeof (FcCharSetIsSubset) FcCharSetIsSubset __attribute((alias("IA__FcCharSetIsSubset"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetIsSubset) chromium_FcCharSetIsSubset __attribute((alias("IA__FcCharSetIsSubset"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetFirstPage
extern __typeof (FcCharSetFirstPage) FcCharSetFirstPage __attribute((alias("IA__FcCharSetFirstPage"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetFirstPage) chromium_FcCharSetFirstPage __attribute((alias("IA__FcCharSetFirstPage"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetNextPage
extern __typeof (FcCharSetNextPage) FcCharSetNextPage __attribute((alias("IA__FcCharSetNextPage"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetNextPage) chromium_FcCharSetNextPage __attribute((alias("IA__FcCharSetNextPage"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcCharSetCoverage
extern __typeof (FcCharSetCoverage) FcCharSetCoverage __attribute((alias("IA__FcCharSetCoverage"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcCharSetCoverage) chromium_FcCharSetCoverage __attribute((alias("IA__FcCharSetCoverage"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccharset__ */
#ifdef __fcdbg__
# undef FcValuePrint
extern __typeof (FcValuePrint) FcValuePrint __attribute((alias("IA__FcValuePrint"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcValuePrint) chromium_FcValuePrint __attribute((alias("IA__FcValuePrint"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternPrint
extern __typeof (FcPatternPrint) FcPatternPrint __attribute((alias("IA__FcPatternPrint"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternPrint) chromium_FcPatternPrint __attribute((alias("IA__FcPatternPrint"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetPrint
extern __typeof (FcFontSetPrint) FcFontSetPrint __attribute((alias("IA__FcFontSetPrint"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetPrint) chromium_FcFontSetPrint __attribute((alias("IA__FcFontSetPrint"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcdbg__ */
#ifdef __fcdefault__
# undef FcConfigGetDefaultLangs
extern __typeof (FcConfigGetDefaultLangs) FcConfigGetDefaultLangs __attribute((alias("IA__FcConfigGetDefaultLangs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetDefaultLangs) chromium_FcConfigGetDefaultLangs __attribute((alias("IA__FcConfigGetDefaultLangs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcGetDefaultLangs
extern __typeof (FcGetDefaultLangs) FcGetDefaultLangs __attribute((alias("IA__FcGetDefaultLangs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcGetDefaultLangs) chromium_FcGetDefaultLangs __attribute((alias("IA__FcGetDefaultLangs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetDefaultSubstitute
extern __typeof (FcConfigSetDefaultSubstitute) FcConfigSetDefaultSubstitute __attribute((alias("IA__FcConfigSetDefaultSubstitute"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetDefaultSubstitute) chromium_FcConfigSetDefaultSubstitute __attribute((alias("IA__FcConfigSetDefaultSubstitute"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDefaultSubstitute
extern __typeof (FcDefaultSubstitute) FcDefaultSubstitute __attribute((alias("IA__FcDefaultSubstitute"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDefaultSubstitute) chromium_FcDefaultSubstitute __attribute((alias("IA__FcDefaultSubstitute"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcdefault__ */
#ifdef __fcdir__
# undef FcFileIsDir
extern __typeof (FcFileIsDir) FcFileIsDir __attribute((alias("IA__FcFileIsDir"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFileIsDir) chromium_FcFileIsDir __attribute((alias("IA__FcFileIsDir"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFileScan
extern __typeof (FcFileScan) FcFileScan __attribute((alias("IA__FcFileScan"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFileScan) chromium_FcFileScan __attribute((alias("IA__FcFileScan"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirScan
extern __typeof (FcDirScan) FcDirScan __attribute((alias("IA__FcDirScan"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirScan) chromium_FcDirScan __attribute((alias("IA__FcDirScan"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirSave
extern __typeof (FcDirSave) FcDirSave __attribute((alias("IA__FcDirSave"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirSave) chromium_FcDirSave __attribute((alias("IA__FcDirSave"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcdir__ */
#ifdef __fccache__
# undef FcDirCacheLoad
extern __typeof (FcDirCacheLoad) FcDirCacheLoad __attribute((alias("IA__FcDirCacheLoad"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheLoad) chromium_FcDirCacheLoad __attribute((alias("IA__FcDirCacheLoad"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccache__ */
#ifdef __fcdir__
# undef FcDirCacheRescan
extern __typeof (FcDirCacheRescan) FcDirCacheRescan __attribute((alias("IA__FcDirCacheRescan"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheRescan) chromium_FcDirCacheRescan __attribute((alias("IA__FcDirCacheRescan"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheRead
extern __typeof (FcDirCacheRead) FcDirCacheRead __attribute((alias("IA__FcDirCacheRead"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheRead) chromium_FcDirCacheRead __attribute((alias("IA__FcDirCacheRead"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcdir__ */
#ifdef __fccache__
# undef FcDirCacheLoadFile
extern __typeof (FcDirCacheLoadFile) FcDirCacheLoadFile __attribute((alias("IA__FcDirCacheLoadFile"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheLoadFile) chromium_FcDirCacheLoadFile __attribute((alias("IA__FcDirCacheLoadFile"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcDirCacheUnload
extern __typeof (FcDirCacheUnload) FcDirCacheUnload __attribute((alias("IA__FcDirCacheUnload"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcDirCacheUnload) chromium_FcDirCacheUnload __attribute((alias("IA__FcDirCacheUnload"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccache__ */
#ifdef __fcfs__
# undef FcFontSetCreate
extern __typeof (FcFontSetCreate) FcFontSetCreate __attribute((alias("IA__FcFontSetCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetCreate) chromium_FcFontSetCreate __attribute((alias("IA__FcFontSetCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetDestroy
extern __typeof (FcFontSetDestroy) FcFontSetDestroy __attribute((alias("IA__FcFontSetDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetDestroy) chromium_FcFontSetDestroy __attribute((alias("IA__FcFontSetDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetAdd
extern __typeof (FcFontSetAdd) FcFontSetAdd __attribute((alias("IA__FcFontSetAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetAdd) chromium_FcFontSetAdd __attribute((alias("IA__FcFontSetAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcfs__ */
#ifdef __fcinit__
# undef FcInitLoadConfig
extern __typeof (FcInitLoadConfig) FcInitLoadConfig __attribute((alias("IA__FcInitLoadConfig"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcInitLoadConfig) chromium_FcInitLoadConfig __attribute((alias("IA__FcInitLoadConfig"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcInitLoadConfigAndFonts
extern __typeof (FcInitLoadConfigAndFonts) FcInitLoadConfigAndFonts __attribute((alias("IA__FcInitLoadConfigAndFonts"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcInitLoadConfigAndFonts) chromium_FcInitLoadConfigAndFonts __attribute((alias("IA__FcInitLoadConfigAndFonts"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcInit
extern __typeof (FcInit) FcInit __attribute((alias("IA__FcInit"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcInit) chromium_FcInit __attribute((alias("IA__FcInit"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFini
extern __typeof (FcFini) FcFini __attribute((alias("IA__FcFini"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFini) chromium_FcFini __attribute((alias("IA__FcFini"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcGetVersion
extern __typeof (FcGetVersion) FcGetVersion __attribute((alias("IA__FcGetVersion"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcGetVersion) chromium_FcGetVersion __attribute((alias("IA__FcGetVersion"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcInitReinitialize
extern __typeof (FcInitReinitialize) FcInitReinitialize __attribute((alias("IA__FcInitReinitialize"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcInitReinitialize) chromium_FcInitReinitialize __attribute((alias("IA__FcInitReinitialize"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcInitBringUptoDate
extern __typeof (FcInitBringUptoDate) FcInitBringUptoDate __attribute((alias("IA__FcInitBringUptoDate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcInitBringUptoDate) chromium_FcInitBringUptoDate __attribute((alias("IA__FcInitBringUptoDate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcinit__ */
#ifdef __fclang__
# undef FcGetLangs
extern __typeof (FcGetLangs) FcGetLangs __attribute((alias("IA__FcGetLangs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcGetLangs) chromium_FcGetLangs __attribute((alias("IA__FcGetLangs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangNormalize
extern __typeof (FcLangNormalize) FcLangNormalize __attribute((alias("IA__FcLangNormalize"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangNormalize) chromium_FcLangNormalize __attribute((alias("IA__FcLangNormalize"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangGetCharSet
extern __typeof (FcLangGetCharSet) FcLangGetCharSet __attribute((alias("IA__FcLangGetCharSet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangGetCharSet) chromium_FcLangGetCharSet __attribute((alias("IA__FcLangGetCharSet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetCreate
extern __typeof (FcLangSetCreate) FcLangSetCreate __attribute((alias("IA__FcLangSetCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetCreate) chromium_FcLangSetCreate __attribute((alias("IA__FcLangSetCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetDestroy
extern __typeof (FcLangSetDestroy) FcLangSetDestroy __attribute((alias("IA__FcLangSetDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetDestroy) chromium_FcLangSetDestroy __attribute((alias("IA__FcLangSetDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetCopy
extern __typeof (FcLangSetCopy) FcLangSetCopy __attribute((alias("IA__FcLangSetCopy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetCopy) chromium_FcLangSetCopy __attribute((alias("IA__FcLangSetCopy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetAdd
extern __typeof (FcLangSetAdd) FcLangSetAdd __attribute((alias("IA__FcLangSetAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetAdd) chromium_FcLangSetAdd __attribute((alias("IA__FcLangSetAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetDel
extern __typeof (FcLangSetDel) FcLangSetDel __attribute((alias("IA__FcLangSetDel"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetDel) chromium_FcLangSetDel __attribute((alias("IA__FcLangSetDel"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetHasLang
extern __typeof (FcLangSetHasLang) FcLangSetHasLang __attribute((alias("IA__FcLangSetHasLang"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetHasLang) chromium_FcLangSetHasLang __attribute((alias("IA__FcLangSetHasLang"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetCompare
extern __typeof (FcLangSetCompare) FcLangSetCompare __attribute((alias("IA__FcLangSetCompare"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetCompare) chromium_FcLangSetCompare __attribute((alias("IA__FcLangSetCompare"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetContains
extern __typeof (FcLangSetContains) FcLangSetContains __attribute((alias("IA__FcLangSetContains"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetContains) chromium_FcLangSetContains __attribute((alias("IA__FcLangSetContains"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetEqual
extern __typeof (FcLangSetEqual) FcLangSetEqual __attribute((alias("IA__FcLangSetEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetEqual) chromium_FcLangSetEqual __attribute((alias("IA__FcLangSetEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetHash
extern __typeof (FcLangSetHash) FcLangSetHash __attribute((alias("IA__FcLangSetHash"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetHash) chromium_FcLangSetHash __attribute((alias("IA__FcLangSetHash"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetGetLangs
extern __typeof (FcLangSetGetLangs) FcLangSetGetLangs __attribute((alias("IA__FcLangSetGetLangs"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetGetLangs) chromium_FcLangSetGetLangs __attribute((alias("IA__FcLangSetGetLangs"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetUnion
extern __typeof (FcLangSetUnion) FcLangSetUnion __attribute((alias("IA__FcLangSetUnion"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetUnion) chromium_FcLangSetUnion __attribute((alias("IA__FcLangSetUnion"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcLangSetSubtract
extern __typeof (FcLangSetSubtract) FcLangSetSubtract __attribute((alias("IA__FcLangSetSubtract"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcLangSetSubtract) chromium_FcLangSetSubtract __attribute((alias("IA__FcLangSetSubtract"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fclang__ */
#ifdef __fclist__
# undef FcObjectSetCreate
extern __typeof (FcObjectSetCreate) FcObjectSetCreate __attribute((alias("IA__FcObjectSetCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcObjectSetCreate) chromium_FcObjectSetCreate __attribute((alias("IA__FcObjectSetCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcObjectSetAdd
extern __typeof (FcObjectSetAdd) FcObjectSetAdd __attribute((alias("IA__FcObjectSetAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcObjectSetAdd) chromium_FcObjectSetAdd __attribute((alias("IA__FcObjectSetAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcObjectSetDestroy
extern __typeof (FcObjectSetDestroy) FcObjectSetDestroy __attribute((alias("IA__FcObjectSetDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcObjectSetDestroy) chromium_FcObjectSetDestroy __attribute((alias("IA__FcObjectSetDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcObjectSetVaBuild
extern __typeof (FcObjectSetVaBuild) FcObjectSetVaBuild __attribute((alias("IA__FcObjectSetVaBuild"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcObjectSetVaBuild) chromium_FcObjectSetVaBuild __attribute((alias("IA__FcObjectSetVaBuild"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcObjectSetBuild
extern __typeof (FcObjectSetBuild) FcObjectSetBuild __attribute((alias("IA__FcObjectSetBuild"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcObjectSetBuild) chromium_FcObjectSetBuild __attribute((alias("IA__FcObjectSetBuild"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetList
extern __typeof (FcFontSetList) FcFontSetList __attribute((alias("IA__FcFontSetList"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetList) chromium_FcFontSetList __attribute((alias("IA__FcFontSetList"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontList
extern __typeof (FcFontList) FcFontList __attribute((alias("IA__FcFontList"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontList) chromium_FcFontList __attribute((alias("IA__FcFontList"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fclist__ */
#ifdef __fcatomic__
# undef FcAtomicCreate
extern __typeof (FcAtomicCreate) FcAtomicCreate __attribute((alias("IA__FcAtomicCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicCreate) chromium_FcAtomicCreate __attribute((alias("IA__FcAtomicCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicLock
extern __typeof (FcAtomicLock) FcAtomicLock __attribute((alias("IA__FcAtomicLock"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicLock) chromium_FcAtomicLock __attribute((alias("IA__FcAtomicLock"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicNewFile
extern __typeof (FcAtomicNewFile) FcAtomicNewFile __attribute((alias("IA__FcAtomicNewFile"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicNewFile) chromium_FcAtomicNewFile __attribute((alias("IA__FcAtomicNewFile"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicOrigFile
extern __typeof (FcAtomicOrigFile) FcAtomicOrigFile __attribute((alias("IA__FcAtomicOrigFile"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicOrigFile) chromium_FcAtomicOrigFile __attribute((alias("IA__FcAtomicOrigFile"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicReplaceOrig
extern __typeof (FcAtomicReplaceOrig) FcAtomicReplaceOrig __attribute((alias("IA__FcAtomicReplaceOrig"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicReplaceOrig) chromium_FcAtomicReplaceOrig __attribute((alias("IA__FcAtomicReplaceOrig"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicDeleteNew
extern __typeof (FcAtomicDeleteNew) FcAtomicDeleteNew __attribute((alias("IA__FcAtomicDeleteNew"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicDeleteNew) chromium_FcAtomicDeleteNew __attribute((alias("IA__FcAtomicDeleteNew"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicUnlock
extern __typeof (FcAtomicUnlock) FcAtomicUnlock __attribute((alias("IA__FcAtomicUnlock"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicUnlock) chromium_FcAtomicUnlock __attribute((alias("IA__FcAtomicUnlock"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcAtomicDestroy
extern __typeof (FcAtomicDestroy) FcAtomicDestroy __attribute((alias("IA__FcAtomicDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcAtomicDestroy) chromium_FcAtomicDestroy __attribute((alias("IA__FcAtomicDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcatomic__ */
#ifdef __fcmatch__
# undef FcFontSetMatch
extern __typeof (FcFontSetMatch) FcFontSetMatch __attribute((alias("IA__FcFontSetMatch"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetMatch) chromium_FcFontSetMatch __attribute((alias("IA__FcFontSetMatch"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontMatch
extern __typeof (FcFontMatch) FcFontMatch __attribute((alias("IA__FcFontMatch"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontMatch) chromium_FcFontMatch __attribute((alias("IA__FcFontMatch"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontRenderPrepare
extern __typeof (FcFontRenderPrepare) FcFontRenderPrepare __attribute((alias("IA__FcFontRenderPrepare"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontRenderPrepare) chromium_FcFontRenderPrepare __attribute((alias("IA__FcFontRenderPrepare"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetSort
extern __typeof (FcFontSetSort) FcFontSetSort __attribute((alias("IA__FcFontSetSort"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetSort) chromium_FcFontSetSort __attribute((alias("IA__FcFontSetSort"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSort
extern __typeof (FcFontSort) FcFontSort __attribute((alias("IA__FcFontSort"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSort) chromium_FcFontSort __attribute((alias("IA__FcFontSort"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcFontSetSortDestroy
extern __typeof (FcFontSetSortDestroy) FcFontSetSortDestroy __attribute((alias("IA__FcFontSetSortDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcFontSetSortDestroy) chromium_FcFontSetSortDestroy __attribute((alias("IA__FcFontSetSortDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcmatch__ */
#ifdef __fcindex__
# undef FcIndexerQueryAll
extern __typeof (FcIndexerQueryAll) FcIndexerQueryAll __attribute((alias("IA__FcIndexerQueryAll"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcIndexerQueryAll) chromium_FcIndexerQueryAll __attribute((alias("IA__FcIndexerQueryAll"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcIndexerQuery
extern __typeof (FcIndexerQuery) FcIndexerQuery __attribute((alias("IA__FcIndexerQuery"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcIndexerQuery) chromium_FcIndexerQuery __attribute((alias("IA__FcIndexerQuery"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcindex__ */
#ifdef __fcmatrix__
# undef FcMatrixCopy
extern __typeof (FcMatrixCopy) FcMatrixCopy __attribute((alias("IA__FcMatrixCopy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixCopy) chromium_FcMatrixCopy __attribute((alias("IA__FcMatrixCopy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcMatrixEqual
extern __typeof (FcMatrixEqual) FcMatrixEqual __attribute((alias("IA__FcMatrixEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixEqual) chromium_FcMatrixEqual __attribute((alias("IA__FcMatrixEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcMatrixMultiply
extern __typeof (FcMatrixMultiply) FcMatrixMultiply __attribute((alias("IA__FcMatrixMultiply"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixMultiply) chromium_FcMatrixMultiply __attribute((alias("IA__FcMatrixMultiply"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcMatrixRotate
extern __typeof (FcMatrixRotate) FcMatrixRotate __attribute((alias("IA__FcMatrixRotate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixRotate) chromium_FcMatrixRotate __attribute((alias("IA__FcMatrixRotate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcMatrixScale
extern __typeof (FcMatrixScale) FcMatrixScale __attribute((alias("IA__FcMatrixScale"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixScale) chromium_FcMatrixScale __attribute((alias("IA__FcMatrixScale"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcMatrixShear
extern __typeof (FcMatrixShear) FcMatrixShear __attribute((alias("IA__FcMatrixShear"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcMatrixShear) chromium_FcMatrixShear __attribute((alias("IA__FcMatrixShear"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcmatrix__ */
#ifdef __fcname__
# undef FcNameRegisterObjectTypes
extern __typeof (FcNameRegisterObjectTypes) FcNameRegisterObjectTypes __attribute((alias("IA__FcNameRegisterObjectTypes"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameRegisterObjectTypes) chromium_FcNameRegisterObjectTypes __attribute((alias("IA__FcNameRegisterObjectTypes"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameUnregisterObjectTypes
extern __typeof (FcNameUnregisterObjectTypes) FcNameUnregisterObjectTypes __attribute((alias("IA__FcNameUnregisterObjectTypes"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameUnregisterObjectTypes) chromium_FcNameUnregisterObjectTypes __attribute((alias("IA__FcNameUnregisterObjectTypes"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameGetObjectType
extern __typeof (FcNameGetObjectType) FcNameGetObjectType __attribute((alias("IA__FcNameGetObjectType"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameGetObjectType) chromium_FcNameGetObjectType __attribute((alias("IA__FcNameGetObjectType"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameRegisterConstants
extern __typeof (FcNameRegisterConstants) FcNameRegisterConstants __attribute((alias("IA__FcNameRegisterConstants"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameRegisterConstants) chromium_FcNameRegisterConstants __attribute((alias("IA__FcNameRegisterConstants"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameUnregisterConstants
extern __typeof (FcNameUnregisterConstants) FcNameUnregisterConstants __attribute((alias("IA__FcNameUnregisterConstants"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameUnregisterConstants) chromium_FcNameUnregisterConstants __attribute((alias("IA__FcNameUnregisterConstants"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameGetConstant
extern __typeof (FcNameGetConstant) FcNameGetConstant __attribute((alias("IA__FcNameGetConstant"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameGetConstant) chromium_FcNameGetConstant __attribute((alias("IA__FcNameGetConstant"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameGetConstantFor
extern __typeof (FcNameGetConstantFor) FcNameGetConstantFor __attribute((alias("IA__FcNameGetConstantFor"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameGetConstantFor) chromium_FcNameGetConstantFor __attribute((alias("IA__FcNameGetConstantFor"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameConstant
extern __typeof (FcNameConstant) FcNameConstant __attribute((alias("IA__FcNameConstant"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameConstant) chromium_FcNameConstant __attribute((alias("IA__FcNameConstant"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameGetConstantNameFrom
extern __typeof (FcNameGetConstantNameFrom) FcNameGetConstantNameFrom __attribute((alias("IA__FcNameGetConstantNameFrom"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameGetConstantNameFrom) chromium_FcNameGetConstantNameFrom __attribute((alias("IA__FcNameGetConstantNameFrom"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameParse
extern __typeof (FcNameParse) FcNameParse __attribute((alias("IA__FcNameParse"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameParse) chromium_FcNameParse __attribute((alias("IA__FcNameParse"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcNameUnparse
extern __typeof (FcNameUnparse) FcNameUnparse __attribute((alias("IA__FcNameUnparse"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcNameUnparse) chromium_FcNameUnparse __attribute((alias("IA__FcNameUnparse"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcname__ */
#ifdef __fcpat__
# undef FcPatternCreate
extern __typeof (FcPatternCreate) FcPatternCreate __attribute((alias("IA__FcPatternCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternCreate) chromium_FcPatternCreate __attribute((alias("IA__FcPatternCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternDuplicate
extern __typeof (FcPatternDuplicate) FcPatternDuplicate __attribute((alias("IA__FcPatternDuplicate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternDuplicate) chromium_FcPatternDuplicate __attribute((alias("IA__FcPatternDuplicate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternReference
extern __typeof (FcPatternReference) FcPatternReference __attribute((alias("IA__FcPatternReference"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternReference) chromium_FcPatternReference __attribute((alias("IA__FcPatternReference"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternFilter
extern __typeof (FcPatternFilter) FcPatternFilter __attribute((alias("IA__FcPatternFilter"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternFilter) chromium_FcPatternFilter __attribute((alias("IA__FcPatternFilter"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcValueDestroy
extern __typeof (FcValueDestroy) FcValueDestroy __attribute((alias("IA__FcValueDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcValueDestroy) chromium_FcValueDestroy __attribute((alias("IA__FcValueDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcValueEqual
extern __typeof (FcValueEqual) FcValueEqual __attribute((alias("IA__FcValueEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcValueEqual) chromium_FcValueEqual __attribute((alias("IA__FcValueEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcValueSave
extern __typeof (FcValueSave) FcValueSave __attribute((alias("IA__FcValueSave"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcValueSave) chromium_FcValueSave __attribute((alias("IA__FcValueSave"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternDestroy
extern __typeof (FcPatternDestroy) FcPatternDestroy __attribute((alias("IA__FcPatternDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternDestroy) chromium_FcPatternDestroy __attribute((alias("IA__FcPatternDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternObjectCount
extern __typeof (FcPatternObjectCount) FcPatternObjectCount __attribute((alias("IA__FcPatternObjectCount"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternObjectCount) chromium_FcPatternObjectCount __attribute((alias("IA__FcPatternObjectCount"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternEqual
extern __typeof (FcPatternEqual) FcPatternEqual __attribute((alias("IA__FcPatternEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternEqual) chromium_FcPatternEqual __attribute((alias("IA__FcPatternEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternEqualSubset
extern __typeof (FcPatternEqualSubset) FcPatternEqualSubset __attribute((alias("IA__FcPatternEqualSubset"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternEqualSubset) chromium_FcPatternEqualSubset __attribute((alias("IA__FcPatternEqualSubset"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternHash
extern __typeof (FcPatternHash) FcPatternHash __attribute((alias("IA__FcPatternHash"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternHash) chromium_FcPatternHash __attribute((alias("IA__FcPatternHash"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAdd
extern __typeof (FcPatternAdd) FcPatternAdd __attribute((alias("IA__FcPatternAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAdd) chromium_FcPatternAdd __attribute((alias("IA__FcPatternAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddWeak
extern __typeof (FcPatternAddWeak) FcPatternAddWeak __attribute((alias("IA__FcPatternAddWeak"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddWeak) chromium_FcPatternAddWeak __attribute((alias("IA__FcPatternAddWeak"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGet
extern __typeof (FcPatternGet) FcPatternGet __attribute((alias("IA__FcPatternGet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGet) chromium_FcPatternGet __attribute((alias("IA__FcPatternGet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetWithBinding
extern __typeof (FcPatternGetWithBinding) FcPatternGetWithBinding __attribute((alias("IA__FcPatternGetWithBinding"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetWithBinding) chromium_FcPatternGetWithBinding __attribute((alias("IA__FcPatternGetWithBinding"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternDel
extern __typeof (FcPatternDel) FcPatternDel __attribute((alias("IA__FcPatternDel"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternDel) chromium_FcPatternDel __attribute((alias("IA__FcPatternDel"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternRemove
extern __typeof (FcPatternRemove) FcPatternRemove __attribute((alias("IA__FcPatternRemove"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternRemove) chromium_FcPatternRemove __attribute((alias("IA__FcPatternRemove"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddInteger
extern __typeof (FcPatternAddInteger) FcPatternAddInteger __attribute((alias("IA__FcPatternAddInteger"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddInteger) chromium_FcPatternAddInteger __attribute((alias("IA__FcPatternAddInteger"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddDouble
extern __typeof (FcPatternAddDouble) FcPatternAddDouble __attribute((alias("IA__FcPatternAddDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddDouble) chromium_FcPatternAddDouble __attribute((alias("IA__FcPatternAddDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddString
extern __typeof (FcPatternAddString) FcPatternAddString __attribute((alias("IA__FcPatternAddString"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddString) chromium_FcPatternAddString __attribute((alias("IA__FcPatternAddString"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddMatrix
extern __typeof (FcPatternAddMatrix) FcPatternAddMatrix __attribute((alias("IA__FcPatternAddMatrix"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddMatrix) chromium_FcPatternAddMatrix __attribute((alias("IA__FcPatternAddMatrix"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddCharSet
extern __typeof (FcPatternAddCharSet) FcPatternAddCharSet __attribute((alias("IA__FcPatternAddCharSet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddCharSet) chromium_FcPatternAddCharSet __attribute((alias("IA__FcPatternAddCharSet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddBool
extern __typeof (FcPatternAddBool) FcPatternAddBool __attribute((alias("IA__FcPatternAddBool"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddBool) chromium_FcPatternAddBool __attribute((alias("IA__FcPatternAddBool"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddLangSet
extern __typeof (FcPatternAddLangSet) FcPatternAddLangSet __attribute((alias("IA__FcPatternAddLangSet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddLangSet) chromium_FcPatternAddLangSet __attribute((alias("IA__FcPatternAddLangSet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternAddRange
extern __typeof (FcPatternAddRange) FcPatternAddRange __attribute((alias("IA__FcPatternAddRange"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternAddRange) chromium_FcPatternAddRange __attribute((alias("IA__FcPatternAddRange"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetInteger
extern __typeof (FcPatternGetInteger) FcPatternGetInteger __attribute((alias("IA__FcPatternGetInteger"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetInteger) chromium_FcPatternGetInteger __attribute((alias("IA__FcPatternGetInteger"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetDouble
extern __typeof (FcPatternGetDouble) FcPatternGetDouble __attribute((alias("IA__FcPatternGetDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetDouble) chromium_FcPatternGetDouble __attribute((alias("IA__FcPatternGetDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetString
extern __typeof (FcPatternGetString) FcPatternGetString __attribute((alias("IA__FcPatternGetString"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetString) chromium_FcPatternGetString __attribute((alias("IA__FcPatternGetString"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetMatrix
extern __typeof (FcPatternGetMatrix) FcPatternGetMatrix __attribute((alias("IA__FcPatternGetMatrix"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetMatrix) chromium_FcPatternGetMatrix __attribute((alias("IA__FcPatternGetMatrix"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetCharSet
extern __typeof (FcPatternGetCharSet) FcPatternGetCharSet __attribute((alias("IA__FcPatternGetCharSet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetCharSet) chromium_FcPatternGetCharSet __attribute((alias("IA__FcPatternGetCharSet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetBool
extern __typeof (FcPatternGetBool) FcPatternGetBool __attribute((alias("IA__FcPatternGetBool"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetBool) chromium_FcPatternGetBool __attribute((alias("IA__FcPatternGetBool"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetLangSet
extern __typeof (FcPatternGetLangSet) FcPatternGetLangSet __attribute((alias("IA__FcPatternGetLangSet"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetLangSet) chromium_FcPatternGetLangSet __attribute((alias("IA__FcPatternGetLangSet"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternGetRange
extern __typeof (FcPatternGetRange) FcPatternGetRange __attribute((alias("IA__FcPatternGetRange"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternGetRange) chromium_FcPatternGetRange __attribute((alias("IA__FcPatternGetRange"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternVaBuild
extern __typeof (FcPatternVaBuild) FcPatternVaBuild __attribute((alias("IA__FcPatternVaBuild"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternVaBuild) chromium_FcPatternVaBuild __attribute((alias("IA__FcPatternVaBuild"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternBuild
extern __typeof (FcPatternBuild) FcPatternBuild __attribute((alias("IA__FcPatternBuild"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternBuild) chromium_FcPatternBuild __attribute((alias("IA__FcPatternBuild"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcpat__ */
#ifdef __fcformat__
# undef FcPatternFormat
extern __typeof (FcPatternFormat) FcPatternFormat __attribute((alias("IA__FcPatternFormat"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternFormat) chromium_FcPatternFormat __attribute((alias("IA__FcPatternFormat"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcformat__ */
#ifdef __fcrange__
# undef FcRangeCreateDouble
extern __typeof (FcRangeCreateDouble) FcRangeCreateDouble __attribute((alias("IA__FcRangeCreateDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcRangeCreateDouble) chromium_FcRangeCreateDouble __attribute((alias("IA__FcRangeCreateDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcRangeCreateInteger
extern __typeof (FcRangeCreateInteger) FcRangeCreateInteger __attribute((alias("IA__FcRangeCreateInteger"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcRangeCreateInteger) chromium_FcRangeCreateInteger __attribute((alias("IA__FcRangeCreateInteger"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcRangeDestroy
extern __typeof (FcRangeDestroy) FcRangeDestroy __attribute((alias("IA__FcRangeDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcRangeDestroy) chromium_FcRangeDestroy __attribute((alias("IA__FcRangeDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcRangeCopy
extern __typeof (FcRangeCopy) FcRangeCopy __attribute((alias("IA__FcRangeCopy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcRangeCopy) chromium_FcRangeCopy __attribute((alias("IA__FcRangeCopy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcRangeGetDouble
extern __typeof (FcRangeGetDouble) FcRangeGetDouble __attribute((alias("IA__FcRangeGetDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcRangeGetDouble) chromium_FcRangeGetDouble __attribute((alias("IA__FcRangeGetDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcrange__ */
#ifdef __fcpat__
# undef FcPatternIterStart
extern __typeof (FcPatternIterStart) FcPatternIterStart __attribute((alias("IA__FcPatternIterStart"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterStart) chromium_FcPatternIterStart __attribute((alias("IA__FcPatternIterStart"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterNext
extern __typeof (FcPatternIterNext) FcPatternIterNext __attribute((alias("IA__FcPatternIterNext"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterNext) chromium_FcPatternIterNext __attribute((alias("IA__FcPatternIterNext"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterEqual
extern __typeof (FcPatternIterEqual) FcPatternIterEqual __attribute((alias("IA__FcPatternIterEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterEqual) chromium_FcPatternIterEqual __attribute((alias("IA__FcPatternIterEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternFindIter
extern __typeof (FcPatternFindIter) FcPatternFindIter __attribute((alias("IA__FcPatternFindIter"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternFindIter) chromium_FcPatternFindIter __attribute((alias("IA__FcPatternFindIter"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterIsValid
extern __typeof (FcPatternIterIsValid) FcPatternIterIsValid __attribute((alias("IA__FcPatternIterIsValid"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterIsValid) chromium_FcPatternIterIsValid __attribute((alias("IA__FcPatternIterIsValid"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterGetObject
extern __typeof (FcPatternIterGetObject) FcPatternIterGetObject __attribute((alias("IA__FcPatternIterGetObject"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterGetObject) chromium_FcPatternIterGetObject __attribute((alias("IA__FcPatternIterGetObject"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterValueCount
extern __typeof (FcPatternIterValueCount) FcPatternIterValueCount __attribute((alias("IA__FcPatternIterValueCount"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterValueCount) chromium_FcPatternIterValueCount __attribute((alias("IA__FcPatternIterValueCount"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcPatternIterGetValue
extern __typeof (FcPatternIterGetValue) FcPatternIterGetValue __attribute((alias("IA__FcPatternIterGetValue"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcPatternIterGetValue) chromium_FcPatternIterGetValue __attribute((alias("IA__FcPatternIterGetValue"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcpat__ */
#ifdef __fcweight__
# undef FcWeightFromOpenType
extern __typeof (FcWeightFromOpenType) FcWeightFromOpenType __attribute((alias("IA__FcWeightFromOpenType"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcWeightFromOpenType) chromium_FcWeightFromOpenType __attribute((alias("IA__FcWeightFromOpenType"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcWeightFromOpenTypeDouble
extern __typeof (FcWeightFromOpenTypeDouble) FcWeightFromOpenTypeDouble __attribute((alias("IA__FcWeightFromOpenTypeDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcWeightFromOpenTypeDouble) chromium_FcWeightFromOpenTypeDouble __attribute((alias("IA__FcWeightFromOpenTypeDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcWeightToOpenType
extern __typeof (FcWeightToOpenType) FcWeightToOpenType __attribute((alias("IA__FcWeightToOpenType"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcWeightToOpenType) chromium_FcWeightToOpenType __attribute((alias("IA__FcWeightToOpenType"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcWeightToOpenTypeDouble
extern __typeof (FcWeightToOpenTypeDouble) FcWeightToOpenTypeDouble __attribute((alias("IA__FcWeightToOpenTypeDouble"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcWeightToOpenTypeDouble) chromium_FcWeightToOpenTypeDouble __attribute((alias("IA__FcWeightToOpenTypeDouble"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcweight__ */
#ifdef __fcstr__
# undef FcStrCopy
extern __typeof (FcStrCopy) FcStrCopy __attribute((alias("IA__FcStrCopy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrCopy) chromium_FcStrCopy __attribute((alias("IA__FcStrCopy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrCopyFilename
extern __typeof (FcStrCopyFilename) FcStrCopyFilename __attribute((alias("IA__FcStrCopyFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrCopyFilename) chromium_FcStrCopyFilename __attribute((alias("IA__FcStrCopyFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrPlus
extern __typeof (FcStrPlus) FcStrPlus __attribute((alias("IA__FcStrPlus"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrPlus) chromium_FcStrPlus __attribute((alias("IA__FcStrPlus"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrFree
extern __typeof (FcStrFree) FcStrFree __attribute((alias("IA__FcStrFree"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrFree) chromium_FcStrFree __attribute((alias("IA__FcStrFree"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrDowncase
extern __typeof (FcStrDowncase) FcStrDowncase __attribute((alias("IA__FcStrDowncase"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrDowncase) chromium_FcStrDowncase __attribute((alias("IA__FcStrDowncase"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrCmpIgnoreCase
extern __typeof (FcStrCmpIgnoreCase) FcStrCmpIgnoreCase __attribute((alias("IA__FcStrCmpIgnoreCase"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrCmpIgnoreCase) chromium_FcStrCmpIgnoreCase __attribute((alias("IA__FcStrCmpIgnoreCase"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrCmp
extern __typeof (FcStrCmp) FcStrCmp __attribute((alias("IA__FcStrCmp"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrCmp) chromium_FcStrCmp __attribute((alias("IA__FcStrCmp"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrStrIgnoreCase
extern __typeof (FcStrStrIgnoreCase) FcStrStrIgnoreCase __attribute((alias("IA__FcStrStrIgnoreCase"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrStrIgnoreCase) chromium_FcStrStrIgnoreCase __attribute((alias("IA__FcStrStrIgnoreCase"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrStr
extern __typeof (FcStrStr) FcStrStr __attribute((alias("IA__FcStrStr"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrStr) chromium_FcStrStr __attribute((alias("IA__FcStrStr"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcUtf8ToUcs4
extern __typeof (FcUtf8ToUcs4) FcUtf8ToUcs4 __attribute((alias("IA__FcUtf8ToUcs4"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcUtf8ToUcs4) chromium_FcUtf8ToUcs4 __attribute((alias("IA__FcUtf8ToUcs4"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcUtf8Len
extern __typeof (FcUtf8Len) FcUtf8Len __attribute((alias("IA__FcUtf8Len"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcUtf8Len) chromium_FcUtf8Len __attribute((alias("IA__FcUtf8Len"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcUcs4ToUtf8
extern __typeof (FcUcs4ToUtf8) FcUcs4ToUtf8 __attribute((alias("IA__FcUcs4ToUtf8"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcUcs4ToUtf8) chromium_FcUcs4ToUtf8 __attribute((alias("IA__FcUcs4ToUtf8"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcUtf16ToUcs4
extern __typeof (FcUtf16ToUcs4) FcUtf16ToUcs4 __attribute((alias("IA__FcUtf16ToUcs4"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcUtf16ToUcs4) chromium_FcUtf16ToUcs4 __attribute((alias("IA__FcUtf16ToUcs4"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcUtf16Len
extern __typeof (FcUtf16Len) FcUtf16Len __attribute((alias("IA__FcUtf16Len"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcUtf16Len) chromium_FcUtf16Len __attribute((alias("IA__FcUtf16Len"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrBuildFilename
extern __typeof (FcStrBuildFilename) FcStrBuildFilename __attribute((alias("IA__FcStrBuildFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrBuildFilename) chromium_FcStrBuildFilename __attribute((alias("IA__FcStrBuildFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrDirname
extern __typeof (FcStrDirname) FcStrDirname __attribute((alias("IA__FcStrDirname"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrDirname) chromium_FcStrDirname __attribute((alias("IA__FcStrDirname"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrBasename
extern __typeof (FcStrBasename) FcStrBasename __attribute((alias("IA__FcStrBasename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrBasename) chromium_FcStrBasename __attribute((alias("IA__FcStrBasename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetCreate
extern __typeof (FcStrSetCreate) FcStrSetCreate __attribute((alias("IA__FcStrSetCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetCreate) chromium_FcStrSetCreate __attribute((alias("IA__FcStrSetCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetMember
extern __typeof (FcStrSetMember) FcStrSetMember __attribute((alias("IA__FcStrSetMember"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetMember) chromium_FcStrSetMember __attribute((alias("IA__FcStrSetMember"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetEqual
extern __typeof (FcStrSetEqual) FcStrSetEqual __attribute((alias("IA__FcStrSetEqual"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetEqual) chromium_FcStrSetEqual __attribute((alias("IA__FcStrSetEqual"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetAdd
extern __typeof (FcStrSetAdd) FcStrSetAdd __attribute((alias("IA__FcStrSetAdd"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetAdd) chromium_FcStrSetAdd __attribute((alias("IA__FcStrSetAdd"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetAddFilename
extern __typeof (FcStrSetAddFilename) FcStrSetAddFilename __attribute((alias("IA__FcStrSetAddFilename"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetAddFilename) chromium_FcStrSetAddFilename __attribute((alias("IA__FcStrSetAddFilename"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetDel
extern __typeof (FcStrSetDel) FcStrSetDel __attribute((alias("IA__FcStrSetDel"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetDel) chromium_FcStrSetDel __attribute((alias("IA__FcStrSetDel"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrSetDestroy
extern __typeof (FcStrSetDestroy) FcStrSetDestroy __attribute((alias("IA__FcStrSetDestroy"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrSetDestroy) chromium_FcStrSetDestroy __attribute((alias("IA__FcStrSetDestroy"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrListCreate
extern __typeof (FcStrListCreate) FcStrListCreate __attribute((alias("IA__FcStrListCreate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrListCreate) chromium_FcStrListCreate __attribute((alias("IA__FcStrListCreate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrListFirst
extern __typeof (FcStrListFirst) FcStrListFirst __attribute((alias("IA__FcStrListFirst"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrListFirst) chromium_FcStrListFirst __attribute((alias("IA__FcStrListFirst"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrListNext
extern __typeof (FcStrListNext) FcStrListNext __attribute((alias("IA__FcStrListNext"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrListNext) chromium_FcStrListNext __attribute((alias("IA__FcStrListNext"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcStrListDone
extern __typeof (FcStrListDone) FcStrListDone __attribute((alias("IA__FcStrListDone"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcStrListDone) chromium_FcStrListDone __attribute((alias("IA__FcStrListDone"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcstr__ */
#ifdef __fcxml__
# undef FcConfigParseAndLoad
extern __typeof (FcConfigParseAndLoad) FcConfigParseAndLoad __attribute((alias("IA__FcConfigParseAndLoad"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigParseAndLoad) chromium_FcConfigParseAndLoad __attribute((alias("IA__FcConfigParseAndLoad"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigParseAndLoadFromMemory
extern __typeof (FcConfigParseAndLoadFromMemory) FcConfigParseAndLoadFromMemory __attribute((alias("IA__FcConfigParseAndLoadFromMemory"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigParseAndLoadFromMemory) chromium_FcConfigParseAndLoadFromMemory __attribute((alias("IA__FcConfigParseAndLoadFromMemory"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcxml__ */
#ifdef __fcconffile__
# undef FcConfigFileGenerate
extern __typeof (FcConfigFileGenerate) FcConfigFileGenerate __attribute((alias("IA__FcConfigFileGenerate"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigFileGenerate) chromium_FcConfigFileGenerate __attribute((alias("IA__FcConfigFileGenerate"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fcconffile__ */
#ifdef __fccfg__
# undef FcConfigGetRescanInverval
extern __typeof (FcConfigGetRescanInverval) FcConfigGetRescanInverval __attribute((alias("IA__FcConfigGetRescanInverval"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigGetRescanInverval) chromium_FcConfigGetRescanInverval __attribute((alias("IA__FcConfigGetRescanInverval"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
# undef FcConfigSetRescanInverval
extern __typeof (FcConfigSetRescanInverval) FcConfigSetRescanInverval __attribute((alias("IA__FcConfigSetRescanInverval"))) FC_ATTRIBUTE_VISIBILITY_EXPORT;
extern __typeof (FcConfigSetRescanInverval) chromium_FcConfigSetRescanInverval __attribute((alias("IA__FcConfigSetRescanInverval"))) FC_CHROMIUM_ATTRIBUTE_VISIBILITY_EXPORT;
#endif /* __fccfg__ */
#endif /* HAVE_GNUC_ATTRIBUTE */
