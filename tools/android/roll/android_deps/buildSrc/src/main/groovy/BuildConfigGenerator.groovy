// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import groovy.json.JsonOutput
import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction

import java.util.regex.Pattern

/**
 * Task to download dependencies specified in {@link ChromiumPlugin} and configure the
 * Chromium build to integrate them. Used by declaring a new task in a {@code build.gradle}
 * file:
 * <pre>
 * task myTaskName(type: BuildConfigGenerator) {
 *   repositoryPath 'build_files_and_repository_location/'
 * }
 * </pre>
 */
class BuildConfigGenerator extends DefaultTask {
    private static final BUILD_GN_TOKEN_START = "# === Generated Code Start ==="
    private static final BUILD_GN_TOKEN_END = "# === Generated Code End ==="
    private static final BUILD_GN_GEN_PATTERN = Pattern.compile(
            "${BUILD_GN_TOKEN_START}(.*)${BUILD_GN_TOKEN_END}",
            Pattern.DOTALL)
    private static final BUILD_GN_GEN_REMINDER = "# This is generated, do not edit. Update BuildConfigGenerator.groovy instead.\n"
    private static final DEPS_TOKEN_START = "# === ANDROID_DEPS Generated Code Start ==="
    private static final DEPS_TOKEN_END = "# === ANDROID_DEPS Generated Code End ==="
    private static final DEPS_GEN_PATTERN = Pattern.compile(
            "${DEPS_TOKEN_START}(.*)${DEPS_TOKEN_END}",
            Pattern.DOTALL)
    private static final DOWNLOAD_DIRECTORY_NAME = "libs"

    // Some libraries are hosted in Chromium's //third_party directory. This is a mapping between
    // them so they can be used instead of android_deps pulling in its own copy.
    private static final def EXISTING_LIBS = [
        'junit_junit': '//third_party/junit:junit',
        'org_hamcrest_hamcrest_core': '//third_party/hamcrest:hamcrest_core_java',
    ]


    /**
     * Directory where the artifacts will be downloaded and where files will be generated.
     * Note: this path is specified as relative to the chromium source root, and must be normalised
     * to an absolute path before being used, as Groovy would base relative path where the script
     * is being executed.
     */
    String repositoryPath

    /**
     * Relative path to the DEPS file where the cipd packages are specified.
     */
    String depsPath

    /**
     * Relative path to the Chromium source root from the build.gradle file.
     */
    String chromiumSourceRoot

    /**
     * Name of the cipd root package.
     */
    String cipdBucket

    /**
     * Prefix of path to strip before uploading to CIPD.
     */
    String stripFromCipdPath

    /**
     * Skips license file import.
     */
    boolean skipLicenses

    /**
     * Only pull play services targets into BUILD.gn file.
     * If the play services target depends on a non-play services target, it will use the target in
     * //third_party/android_deps/BUILD.gn.
     */
    boolean onlyPlayServices

    @TaskAction
    void main() {
        def graph = new ChromiumDepGraph(project: project, skipLicenses: skipLicenses)
        def normalisedRepoPath = normalisePath(repositoryPath)
        def rootDirPath = normalisePath(".")

        // 1. Parse the dependency data
        graph.collectDependencies()

        // 2. Import artifacts into the local repository
        def dependencyDirectories = []
        graph.dependencies.values().each { dependency ->
            if (excludeDependency(dependency, onlyPlayServices)) {
                return
            }
            logger.debug "Processing ${dependency.name}: \n${jsonDump(dependency)}"
            def depDir = "${DOWNLOAD_DIRECTORY_NAME}/${dependency.id}"
            def absoluteDepDir = "${normalisedRepoPath}/${depDir}"

            dependencyDirectories.add(depDir)

            if (new File("${absoluteDepDir}/${dependency.fileName}").exists()) {
                getLogger().quiet("${dependency.id} exists, skipping.")
                return
            }

            project.copy {
                from dependency.artifact.file
                into absoluteDepDir
            }

            new File("${absoluteDepDir}/README.chromium").write(makeReadme(dependency))
            new File("${absoluteDepDir}/cipd.yaml").write(makeCipdYaml(dependency, cipdBucket,
                                                                       stripFromCipdPath,
                                                                       repositoryPath))
            new File("${absoluteDepDir}/OWNERS").write(makeOwners())
            if (!skipLicenses) {
                if (!dependency.licensePath?.trim()?.isEmpty()) {
                    new File("${absoluteDepDir}/LICENSE").write(
                            new File("${normalisedRepoPath}/${dependency.licensePath}").text)
                } else if (!dependency.licenseUrl?.trim()?.isEmpty()) {
                    File destFile = new File("${absoluteDepDir}/LICENSE")
                    downloadFile(dependency.id, dependency.licenseUrl, destFile)
                    if (destFile.text.contains("<html")) {
                        throw new RuntimeException("Found HTML in LICENSE file. Please add an "
                                + "override to ChromiumDepGraph.groovy for ${dependency.name}.")
                    }
                }
            }
        }

        // 3. Generate the root level build files
        updateBuildTargetDeclaration(graph, "${normalisedRepoPath}/BUILD.gn", onlyPlayServices)
        updateDepsDeclaration(graph, cipdBucket, stripFromCipdPath, repositoryPath,
                              "${rootDirPath}/${depsPath}", onlyPlayServices)
        dependencyDirectories.sort { path1, path2 -> return path1.compareTo(path2) }
        updateReadmeReferenceFile(dependencyDirectories,
                                  "${normalisedRepoPath}/additional_readme_paths.json")
    }

    private static void updateBuildTargetDeclaration(ChromiumDepGraph depGraph, String path,
                                                     boolean onlyPlayServices) {
        File buildFile = new File(path)
        def sb = new StringBuilder()

        // Comparator to sort the dependency in alphabetical order, with the visible ones coming
        // before all the internal ones.
        def dependencyComparator = { dependency1, dependency2 ->
            def visibilityResult = Boolean.compare(dependency1.visible, dependency2.visible)
            if (visibilityResult != 0) return -visibilityResult

            return dependency1.id.compareTo(dependency2.id)
        }

        depGraph.dependencies.values().sort(dependencyComparator).each { dependency ->
            if (excludeDependency(dependency, onlyPlayServices)) {
                return
            }
            def depsStr = ""
            if (!dependency.children.isEmpty()) {
                dependency.children.each { childDep ->
                    def dep = depGraph.dependencies[childDep]
                    if (dep.exclude) {
                        return
                    }
                    // Special case: If a child dependency is an existing lib, rather than skipping
                    // it, replace the child dependency with the existing lib.
                    def existingLib = EXISTING_LIBS.get(dep.id)
                    def targetName = translateTargetName(dep.id) + "_java"
                    if (existingLib != null) {
                        depsStr += "\"${existingLib}\","
                    } else if (onlyPlayServices && !isPlayServicesTarget(dep.id)) {
                        depsStr += "\"//third_party/android_deps:${targetName}\","
                    } else {
                        depsStr += "\":${targetName}\","
                    }
                }
            }

            def libPath = "${DOWNLOAD_DIRECTORY_NAME}/${dependency.id}"
            def targetName = translateTargetName(dependency.id) + "_java"
            sb.append(BUILD_GN_GEN_REMINDER)
            if (dependency.extension == 'jar') {
                sb.append("""\
                java_prebuilt("${targetName}") {
                  jar_path = "${libPath}/${dependency.fileName}"
                  output_name = "${dependency.id}"
                """.stripIndent())
                if (dependency.supportsAndroid) {
                  sb.append("  supports_android = true\n")
                } else {
                  // No point in enabling asserts third-party prebuilts.
                  // Also required to break a dependency cycle for errorprone.
                  sb.append("  enable_bytecode_rewriter = false\n")
                }
            } else if (dependency.extension == 'aar') {
                sb.append("""\
                android_aar_prebuilt("${targetName}") {
                  aar_path = "${libPath}/${dependency.fileName}"
                  info_path = "${libPath}/${dependency.id}.info"
                """.stripIndent())
            } else {
                throw new IllegalStateException("Dependency type should be JAR or AAR")
            }

            if (!dependency.visible) {
              sb.append("  # To remove visibility constraint, add this dependency to\n")
              sb.append("  # //tools/android/roll/android_deps/build.gradle.\n")
              sb.append("  visibility = [ \":*\" ]\n")
            }
            if (dependency.testOnly) sb.append("  testonly = true\n")
            if (!depsStr.empty) sb.append("  deps = [${depsStr}]\n")
            addSpecialTreatment(sb, dependency.id)

            sb.append("}\n\n")
        }

        def matcher = BUILD_GN_GEN_PATTERN.matcher(buildFile.getText())
        if (!matcher.find()) throw new IllegalStateException("BUILD.gn insertion point not found.")
        buildFile.write(matcher.replaceFirst(
                "${BUILD_GN_TOKEN_START}\n${sb.toString()}\n${BUILD_GN_TOKEN_END}"))
    }

    public static String translateTargetName(String targetName) {
        if (isPlayServicesTarget(targetName)) {
            return targetName.replaceFirst("com_", "").replaceFirst("android_gms_", "")
        }
        return targetName
    }

    public static boolean isPlayServicesTarget(String dependencyId) {
        // Firebase has historically been treated as a part of play services, so it counts here for
        // backwards compatibility.
        return Pattern.matches(".*google.*(play_services|firebase).*", dependencyId)
    }

    private static void addSpecialTreatment(StringBuilder sb, String dependencyId) {
        if (isPlayServicesTarget(dependencyId)) {
            if (Pattern.matches(".*cast_framework.*", dependencyId)) {
                sb.append('  # Removing all resources from cast framework as they are unused bloat.\n')
                sb.append('  # Can only safely remove them when R8 will strip the path that accesses them.\n')
                sb.append('  strip_resources = !is_java_debug\n')
            } else {
                sb.append('  # Removing drawables from GMS .aars as they are unused bloat.\n')
                sb.append('  strip_drawables = true\n')
            }
        }
        switch(dependencyId) {
            case 'androidx_annotation_annotation':
                sb.append('  # https://crbug.com/989505\n')
                sb.append('  jar_excluded_patterns = ["META-INF/proguard/*"]\n')
                break
            case 'androidx_core_core':
            case 'androidx_media_media':
            case 'androidx_versionedparcelable_versionedparcelable':
            case 'com_android_support_support_compat':
            case 'com_android_support_support_media_compat':
            case 'com_android_support_versionedparcelable':
                // Target has AIDL, but we don't support it yet: http://crbug.com/644439
                sb.append('  ignore_aidl = true\n')
                break
            case 'androidx_test_uiautomator_uiautomator':
	        sb.append('  deps = [":androidx_test_runner_java"]\n')
                break
            case 'com_android_support_mediarouter_v7':
                sb.append('  # https://crbug.com/1000382\n')
                sb.append('  proguard_configs = ["support_mediarouter.flags"]\n')
                break
            case 'androidx_mediarouter_mediarouter':
                sb.append('  # https://crbug.com/1000382\n')
                sb.append('  proguard_configs = ["androidx_mediarouter.flags"]\n')
                break
            case 'androidx_transition_transition':
                // Not specified in the POM, compileOnly dependency not supposed to be used unless
                // the library is present: b/70887421
                sb.append('  deps += [":androidx_fragment_fragment_java"]\n')
                break
            case 'androidx_vectordrawable_vectordrawable':
            case 'com_android_support_support_vector_drawable':
                // Target has AIDL, but we don't support it yet: http://crbug.com/644439
                sb.append('  create_srcjar = false\n')
                break
            case 'android_arch_lifecycle_runtime':
            case 'android_arch_lifecycle_viewmodel':
                sb.append('  # https://crbug.com/887942#c1\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_android_support_coordinatorlayout':
                sb.append('  # https:crbug.com/954584\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_android_support_design':
                // Reduce binary size. https:crbug.com/954584
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_android_support_support_annotations':
                sb.append('  # https://crbug.com/989505\n')
                sb.append('  jar_excluded_patterns = ["META-INF/proguard/*"]\n')
                break
            case 'com_android_support_transition':
                // Not specified in the POM, compileOnly dependency not supposed to be used unless
                // the library is present: b/70887421
                sb.append('  deps += [":com_android_support_support_fragment_java"]\n')
                break
            case 'com_google_android_gms_play_services_basement':
                // Deprecated deps jar but still needed by play services basement.
                sb.append('  input_jars_paths=["\\$android_sdk/optional/org.apache.http.legacy.jar"]\n')
                break
            case 'com_google_ar_core':
                // Target .aar file contains .so libraries that need to be extracted,
                // and android_aar_prebuilt template will fail if it's not set explictly.
                sb.append('  extract_native_libraries = true\n')
                break
            case 'com_google_guava_guava':
                // Need to exclude class and replace it with class library as
                // com_google_guava_listenablefuture has support_androids=true.
                sb.append('  deps += [":com_google_guava_listenablefuture_java"]\n')
                sb.append('  jar_excluded_patterns = ["*/ListenableFuture.class"]\n')
                break
            case 'com_google_guava_listenablefuture_java':
                // Needed to break dependency cycle for errorprone_plugin_java.
                sb.append('  no_build_hooks = true\n')
                break
            case 'androidx_test_rules':
                // Target needs Android SDK deps which exist in third_party/android_sdk.
                sb.append("""\
                |  deps += [
                |    "//third_party/android_sdk:android_test_base_java",
                |    "//third_party/android_sdk:android_test_mock_java",
                |    "//third_party/android_sdk:android_test_runner_java",
                |  ]
                |
                |""".stripMargin())
                break
            case 'net_sf_kxml_kxml2':
                sb.append('  # Target needs to exclude *xmlpull* files as already included in Android SDK.\n')
                sb.append('  jar_excluded_patterns = [ "*xmlpull*" ]\n')
                break
            case 'com_android_support_preference_v7':
                // Replace broad library -keep rules with a more limited set in
                // chrome/android/java/proguard.flags instead.
                sb.append('  ignore_proguard_configs = true\n')
                break
        }
    }

    private static void updateDepsDeclaration(ChromiumDepGraph depGraph, String cipdBucket,
                                              String stripFromCipdPath, String repoPath,
                                              String depsFilePath, boolean onlyPlayServices) {
        File depsFile = new File(depsFilePath)
        def sb = new StringBuilder()
        // Note: The string we're inserting is nested 1 level, hence the 2 leading spaces. Same
        // applies to the multiline package declaration string below.
        sb.append("  # Generated by //tools/android/roll/android_deps/fetch_all.py")

        // Comparator to sort the dependencies in alphabetical order.
        def dependencyComparator = { dependency1, dependency2 ->
            return dependency1.id.compareTo(dependency2.id)
        }

        depGraph.dependencies.values().sort(dependencyComparator).each { dependency ->
            if (excludeDependency(dependency, onlyPlayServices)) {
                return
            }
            def depPath = "${DOWNLOAD_DIRECTORY_NAME}/${dependency.id}"
            def cipdPath = "${cipdBucket}/"
            if (stripFromCipdPath) {
                assert repoPath.startsWith(stripFromCipdPath)
                cipdPath += repoPath.substring(stripFromCipdPath.length() + 1)
            } else {
                cipdPath += repoPath
            }
            // CIPD does not allow uppercase in names.
            cipdPath += "/${depPath}".toLowerCase()
            sb.append("""\
            |
            |  'src/${repoPath}/${depPath}': {
            |      'packages': [
            |          {
            |              'package': '${cipdPath}',
            |              'version': 'version:${dependency.version}-${dependency.cipdSuffix}',
            |          },
            |      ],
            |      'condition': 'checkout_android',
            |      'dep_type': 'cipd',
            |  },
            |""".stripMargin())
        }

        def matcher = DEPS_GEN_PATTERN.matcher(depsFile.getText())
        if (!matcher.find()) throw new IllegalStateException("DEPS insertion point not found.")
        depsFile.write(matcher.replaceFirst("${DEPS_TOKEN_START}\n${sb}\n  ${DEPS_TOKEN_END}"))
    }

    private static void updateReadmeReferenceFile(List<String> directories, String readmePath) {
        File refFile = new File(readmePath)
        refFile.write(JsonOutput.prettyPrint(JsonOutput.toJson(directories)) + "\n")
    }

    public static boolean excludeDependency(ChromiumDepGraph.DependencyDescription dependency,
                                             boolean onlyPlayServices) {
        return dependency.exclude || EXISTING_LIBS.get(dependency.id) != null ||
                (onlyPlayServices && !isPlayServicesTarget(dependency.id))
    }

    private String normalisePath(String pathRelativeToChromiumRoot) {
        return project.file("${chromiumSourceRoot}/${pathRelativeToChromiumRoot}").absolutePath
    }

    static String makeOwners() {
        // Make it easier to upgrade existing dependencies without full third_party review.
        return "file://third_party/android_deps/OWNERS"
    }

    static String makeReadme(ChromiumDepGraph.DependencyDescription dependency) {
        def licenseString
        // Replace license names with ones that are whitelisted, see third_party/PRESUBMIT.py
        switch (dependency.licenseName) {
            case "The Apache Software License, Version 2.0":
                licenseString = "Apache Version 2.0"
                break
            default:
                licenseString = dependency.licenseName
        }

        def securityCritical = dependency.supportsAndroid && dependency.isShipped
        def licenseFile = dependency.isShipped? "LICENSE" : "NOT_SHIPPED"

        return """\
        Name: ${dependency.displayName}
        Short Name: ${dependency.name}
        URL: ${dependency.url}
        Version: ${dependency.version}
        License: ${licenseString}
        License File: ${licenseFile}
        Security Critical: ${securityCritical? "yes" : "no"}
        ${dependency.licenseAndroidCompatible? "License Android Compatible: yes" : ""}
        Description:
        ${dependency.description}

        Local Modifications:
        No modifications.
        """.stripIndent()
    }

    static String makeCipdYaml(ChromiumDepGraph.DependencyDescription dependency, String cipdBucket,
                               String stripFromCipdPath, String repoPath) {
        if (!stripFromCipdPath) {
            stripFromCipdPath = ''
        }
        def cipdVersion = "${dependency.version}-${dependency.cipdSuffix}"
        def cipdPath = "${cipdBucket}/"
        if (stripFromCipdPath) {
            assert repoPath.startsWith(stripFromCipdPath)
            cipdPath += repoPath.substring(stripFromCipdPath.length() + 1)
        } else {
            cipdPath += repoPath
        }
        // CIPD does not allow uppercase in names.
        cipdPath += "/${DOWNLOAD_DIRECTORY_NAME}/" + dependency.id.toLowerCase()

        // NOTE: the fetch_all.py script relies on the format of this file!
        // See fetch_all.py:GetCipdPackageInfo().
        // NOTE: keep the copyright year 2018 until this generated code is
        //       updated, avoiding annual churn of all cipd.yaml files.
        def str = """\
        # Copyright 2018 The Chromium Authors. All rights reserved.
        # Use of this source code is governed by a BSD-style license that can be
        # found in the LICENSE file.

        # To create CIPD package run the following command.
        # cipd create --pkg-def cipd.yaml -tag version:${cipdVersion}
        package: ${cipdPath}
        description: "${dependency.displayName}"
        data:
        - file: ${dependency.fileName}
        """.stripIndent()

        return str
    }

    static String jsonDump(obj) {
        return JsonOutput.prettyPrint(JsonOutput.toJson(obj))
    }

    static void printDump(obj) {
        getLogger().warn(jsonDump(obj))
    }

    static HttpURLConnection connectAndFollowRedirects(String id, String sourceUrl) {
        URL urlObj = new URL(sourceUrl)
        HttpURLConnection connection
        for (int i = 0; i < 10; ++i) {
            // Several deps use this URL for their license, but it just points to license
            // *template*. Generally the actual license can be found in the source code.
            if (sourceUrl.contains("://opensource.org/licenses")) {
                throw new RuntimeException("Found templated license URL for dependency "
                    + id + ": " + sourceUrl
                    + ". You will need to edit FALLBACK_PROPERTIES for this dep.")
            }
            connection = urlObj.openConnection()
            switch (connection.getResponseCode()) {
                case HttpURLConnection.HTTP_MOVED_PERM:
                case HttpURLConnection.HTTP_MOVED_TEMP:
                    String location = connection.getHeaderField("Location");
                    urlObj = new URL(urlObj, location);
                    continue
                case HttpURLConnection.HTTP_OK:
                    return connection
                default:
                    throw new RuntimeException(
                        "Url had statusCode=" + connection.getResponseCode() + ": " + sourceUrl)
            }
        }
        throw new RuntimeException("Url in redirect loop: " + sourceUrl)
    }

    static void downloadFile(String id, String sourceUrl, File destinationFile) {
        destinationFile.withOutputStream { out ->
            out << connectAndFollowRedirects(id, sourceUrl).getInputStream()
        }
    }

}
