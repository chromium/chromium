/*
 * Copyright 2014 - Present Rafael Winterhalter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package net.bytebuddy.android;

import androidx.annotation.RequiresApi;
import android.os.Build;
import com.android.dx.cf.direct.DirectClassFile;
import com.android.dx.cf.direct.StdAttributeFactory;
import com.android.dx.dex.DexOptions;
import com.android.dx.dex.cf.CfOptions;
import com.android.dx.dex.cf.CfTranslator;
import com.android.dx.dex.file.ClassDefItem;
import com.android.dx.dex.file.DexFile;
import dalvik.system.BaseDexClassLoader;
import dalvik.system.DexClassLoader;
import net.bytebuddy.description.type.TypeDescription;
import net.bytebuddy.dynamic.loading.ClassLoadingStrategy;
import net.bytebuddy.utility.RandomString;
import net.bytebuddy.utility.nullability.AlwaysNull;
import net.bytebuddy.utility.nullability.MaybeNull;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.Writer;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.logging.Logger;

/**
 * <p>
 * A class loading strategy that allows to load a dynamically created class at the runtime of an Android
 * application. For this, a {@link dalvik.system.DexClassLoader} is used under the covers.
 * </p>
 * <p>
 * This class loader requires to write files to the file system which are then processed by the Android VM. It is
 * <b>not</b> permitted by Android's security checks to store these files in a shared folder where they could be
 * manipulated by a third application what would break Android's sandbox model. An example for a forbidden storage
 * would therefore be the external storage. Instead, the class loading application must either supply a designated
 * directory, such as by creating a directory using {@code android.content.Context#getDir(String, int)} with specifying
 * {@code android.content.Context#MODE_PRIVATE} visibility for the created folder or by using the
 * {@code getCodeCacheDir} directory which is exposed for Android API versions 21 or higher.
 * </p>
 * <p>
 * By default, this Android {@link net.bytebuddy.dynamic.loading.ClassLoadingStrategy} uses the Android SDK's dex compiler in
 * <i>version 1.7</i> which requires the Java class files in version {@link net.bytebuddy.ClassFileVersion#JAVA_V6} as
 * its input. This version is slightly outdated but newer versions are not available in Maven Central which is why this
 * outdated version is included with this class loading strategy. Newer version can however be easily adapted by
 * implementing the methods of a {@link net.bytebuddy.android.AndroidClassLoadingStrategy.DexProcessor} to
 * appropriately delegate to the newer dex compiler. In case that the dex compiler's API was not altered, it would
 * even be sufficient to include the newer dex compiler to the Android application's build path while also excluding
 * the version that ships with this class loading strategy. While most parts of the Android SDK's components are
 * licensed under the <i>Apache 2.0 license</i>, please also note
 * <a href="https://developer.android.com/sdk/terms.html">their terms and conditions</a>.
 * </p>
 */
public abstract class AndroidClassLoadingStrategy implements ClassLoadingStrategy<ClassLoader> {

    /**
     * The name of the dex file that the {@link dalvik.system.DexClassLoader} expects to find inside of a jar file
     * that is handed to it as its argument.
     */
    private static final String DEX_CLASS_FILE = "classes.dex";

    /**
     * The file name extension of a jar file.
     */
    private static final String JAR_FILE_EXTENSION = ".jar";

    /**
     * A value for a {@link dalvik.system.DexClassLoader} to indicate that the library path is empty.
     */
    @AlwaysNull
    private static final String EMPTY_LIBRARY_PATH = null;

    /**
     * A processor for files before adding them to a dex file.
     */
    private static final FileProcessor FILE_PROCESSOR;

    /*
     * Resolves the file processor.
     */
    static {
        FileProcessor fileProcessor;
        try {
            fileProcessor = new FileProcessor.ForReadOnlyClassFile(
                    Class.forName("java.nio.file.Files").getMethod("setPosixFilePermissions",
                            Class.forName("java.nio.file.Path"),
                            Set.class),
                    File.class.getMethod("toPath"),
                    Collections.singleton(Class.forName("java.nio.file.attribute.PosixFilePermission")
                            .getMethod("valueOf", String.class)
                            .invoke(null, "OWNER_READ")));
        } catch (Throwable ignored) {
            fileProcessor = FileProcessor.Disabled.INSTANCE;
        }
        FILE_PROCESSOR = fileProcessor;
    }

    /**
     * The dex creator to be used by this Android class loading strategy.
     */
    private final DexProcessor dexProcessor;

    /**
     * A directory that is <b>not shared with other applications</b> to be used for storing generated classes and
     * their processed forms.
     */
    protected final File privateDirectory;

    /**
     * A generator for random string values.
     */
    protected final RandomString randomString;

    /**
     * Creates a new Android class loading strategy that uses the given folder for storing classes. The directory is not cleared
     * by Byte Buddy after the application terminates. This remains the responsibility of the user.
     *
     * @param privateDirectory A directory that is <b>not shared with other applications</b> to be used for storing
     *                         generated classes and their processed forms.
     * @param dexProcessor     The dex processor to be used for creating a dex file out of Java files.
     */
    protected AndroidClassLoadingStrategy(File privateDirectory, DexProcessor dexProcessor) {
        if (!privateDirectory.isDirectory()) {
            throw new IllegalArgumentException("Not a directory " + privateDirectory);
        }
        this.privateDirectory = privateDirectory;
        this.dexProcessor = dexProcessor;
        randomString = new RandomString();
    }

    /**
     * {@inheritDoc}
     */
    public Map<TypeDescription, Class<?>> load(@MaybeNull ClassLoader classLoader, Map<TypeDescription, byte[]> types) {
        DexProcessor.Conversion conversion = dexProcessor.create();
        for (Map.Entry<TypeDescription, byte[]> entry : types.entrySet()) {
            conversion.register(entry.getKey().getName(), entry.getValue());
        }
        File jar = new File(privateDirectory, randomString.nextString() + JAR_FILE_EXTENSION);
        try {
            if (!jar.createNewFile()) {
                throw new IllegalStateException("Cannot create " + jar);
            }
            JarOutputStream outputStream = new JarOutputStream(new FileOutputStream(jar));
            try {
                outputStream.putNextEntry(new JarEntry(DEX_CLASS_FILE));
                conversion.drainTo(outputStream);
                outputStream.closeEntry();
            } finally {
                outputStream.close();
            }
            FILE_PROCESSOR.accept(jar);
            return doLoad(classLoader, types.keySet(), jar);
        } catch (IOException exception) {
            throw new IllegalStateException("Cannot write to zip file " + jar, exception);
        } finally {
            if (!jar.delete()) {
                Logger.getLogger("net.bytebuddy").warning("Could not delete " + jar);
            }
        }
    }

    /**
     * Applies the actual class loading.
     *
     * @param classLoader      The target class loader.
     * @param typeDescriptions Descriptions of the loaded types.
     * @param jar              A jar file containing the supplied types as dex files.
     * @return A mapping of all type descriptions to their loaded types.
     * @throws IOException If an I/O exception occurs.
     */
    protected abstract Map<TypeDescription, Class<?>> doLoad(@MaybeNull ClassLoader classLoader, Set<TypeDescription> typeDescriptions, File jar) throws IOException;

    /**
     * A processor for files that are added to a dex file.
     */
    protected interface FileProcessor {

        /**
         * Accepts a file for processing.
         *
         * @param file The file to process.
         */
        void accept(File file);

        /**
         * A non-operational file processor.
         */
        enum Disabled implements FileProcessor {

            /**
             * The singleton instance.
             */
            INSTANCE;

            /**
             * {@inheritDoc}
             */
            public void accept(File file) {
                /* do nothing */
            }
        }

        /**
         * A file processor that marks a file as read-only.
         */
        class ForReadOnlyClassFile implements FileProcessor {

            /**
             * The {@code java.nio.file.Files#setPosixFilePermissions} method.
             */
            private final Method setPosixFilePermissions;

            /**
             * The {@code java.io.File#toPath} method.
             */
            private final Method toPath;

            /**
             * The set of permissions to set.
             */
            private final Set<?> permissions;

            /**
             * Creates a new file processor for a read only file.
             *
             * @param setPosixFilePermissions The {@code java.nio.file.Files#setPosixFilePermissions} method.
             * @param toPath                  The {@code java.io.File#toPath} method.
             * @param permissions             The set of {java.nio.file.attribute.FilePermission} to set.
             */
            protected ForReadOnlyClassFile(Method setPosixFilePermissions, Method toPath, Set<?> permissions) {
                this.setPosixFilePermissions = setPosixFilePermissions;
                this.toPath = toPath;
                this.permissions = permissions;
            }

            /**
             * {@inheritDoc}
             */
            public void accept(File file) {
                try {
                    setPosixFilePermissions.invoke(null, toPath.invoke(file), permissions);
                } catch (IllegalAccessException exception) {
                    throw new IllegalStateException("Cannot access file system permissions", exception);
                } catch (InvocationTargetException exception) {
                    if (!(exception.getTargetException() instanceof UnsupportedOperationException)) {
                        throw new IllegalStateException("Cannot invoke file system permissions method", exception.getTargetException());
                    }
                }
            }
        }
    }

    /**
     * A dex processor is responsible for converting a collection of Java class files into a Android dex file.
     */
    public interface DexProcessor {

        /**
         * Creates a new conversion process which allows to store several Java class files in the created dex
         * file before writing this dex file to a specified {@link java.io.OutputStream}.
         *
         * @return A mutable conversion process.
         */
        Conversion create();

        /**
         * Represents an ongoing conversion of several Java class files into an Android dex file.
         */
        interface Conversion {

            /**
             * Adds a Java class to the generated dex file.
             *
             * @param name                 The binary name of the Java class.
             * @param binaryRepresentation The binary representation of this class.
             */
            void register(String name, byte[] binaryRepresentation);

            /**
             * Writes an Android dex file containing all registered Java classes to the provided output stream.
             *
             * @param outputStream The output stream to write the generated dex file to.
             * @throws IOException If an error occurs while writing the file.
             */
            void drainTo(OutputStream outputStream) throws IOException;
        }

        /**
         * An implementation of a dex processor based on the Android SDK's <i>dx.jar</i> with an API that is
         * compatible to version 1.7.
         */
        class ForSdkCompiler implements DexProcessor {

            /**
             * An API version for a DEX file that ensures compatibility to the underlying compiler.
             */
            private static final int DEX_COMPATIBLE_API_VERSION = 13;

            /**
             * The dispatcher for translating a dex file.
             */
            private static final Dispatcher DISPATCHER;

            /*
             * Resolves the dispatcher for class file translations.
             */
            static {
                Dispatcher dispatcher;
                try {
                    Class<?> dxContextType = Class.forName("com.android.dx.command.dexer.DxContext");
                    dispatcher = new Dispatcher.ForApi26LevelCompatibleVm(CfTranslator.class.getMethod("translate",
                            dxContextType,
                            DirectClassFile.class,
                            byte[].class,
                            CfOptions.class,
                            DexOptions.class,
                            DexFile.class), dxContextType.getConstructor());
                } catch (Throwable ignored) {
                    try {
                        dispatcher = new Dispatcher.ForLegacyVm(CfTranslator.class.getMethod("translate",
                                DirectClassFile.class,
                                byte[].class,
                                CfOptions.class,
                                DexOptions.class,
                                DexFile.class), DexOptions.class.getField("minSdkVersion"));
                    } catch (Throwable suppressed) {
                        try {
                            dispatcher = new Dispatcher.ForLegacyVm(CfTranslator.class.getMethod("translate",
                                    DirectClassFile.class,
                                    byte[].class,
                                    CfOptions.class,
                                    DexOptions.class,
                                    DexFile.class), DexOptions.class.getField("targetApiLevel"));
                        } catch (Throwable throwable) {
                            dispatcher = new Dispatcher.Unavailable(throwable.getMessage());
                        }
                    }
                }
                DISPATCHER = dispatcher;
            }

            /**
             * Creates a default dex processor that ensures API version compatibility.
             *
             * @return A dex processor using an SDK compiler that ensures compatibility.
             */
            protected static DexProcessor makeDefault() {
                DexOptions dexOptions = new DexOptions();
                DISPATCHER.setTargetApi(dexOptions, DEX_COMPATIBLE_API_VERSION);
                return new ForSdkCompiler(dexOptions, new CfOptions());
            }

            /**
             * The file name extension of a Java class file.
             */
            private static final String CLASS_FILE_EXTENSION = ".class";

            /**
             * Indicates that a dex file should be written without providing a human readable output.
             */
            @AlwaysNull
            private static final Writer NO_PRINT_OUTPUT = null;

            /**
             * Indicates that the dex file creation should not be verbose.
             */
            private static final boolean NOT_VERBOSE = false;

            /**
             * The dex file options to be applied when converting a Java class file.
             */
            private final DexOptions dexFileOptions;

            /**
             * The dex compiler options to be applied when converting a Java class file.
             */
            private final CfOptions dexCompilerOptions;

            /**
             * Creates a new Android SDK dex compiler-based dex processor.
             *
             * @param dexFileOptions     The dex file options to apply.
             * @param dexCompilerOptions The dex compiler options to apply.
             */
            public ForSdkCompiler(DexOptions dexFileOptions, CfOptions dexCompilerOptions) {
                this.dexFileOptions = dexFileOptions;
                this.dexCompilerOptions = dexCompilerOptions;
            }

            /**
             * {@inheritDoc}
             */
            public DexProcessor.Conversion create() {
                return new Conversion(new DexFile(dexFileOptions));
            }

            /**
             * Represents a to-dex-file-conversion of a
             * {@link net.bytebuddy.android.AndroidClassLoadingStrategy.DexProcessor.ForSdkCompiler}.
             */
            protected class Conversion implements DexProcessor.Conversion {

                /**
                 * Indicates non-strict parsing of a class file.
                 */
                private static final boolean NON_STRICT = false;

                /**
                 * The dex file that is created by this conversion.
                 */
                private final DexFile dexFile;

                /**
                 * Creates a new ongoing to-dex-file conversion.
                 *
                 * @param dexFile The dex file that is created by this conversion.
                 */
                protected Conversion(DexFile dexFile) {
                    this.dexFile = dexFile;
                }

                /**
                 * {@inheritDoc}
                 */
                public void register(String name, byte[] binaryRepresentation) {
                    DirectClassFile directClassFile = new DirectClassFile(binaryRepresentation,
                            name.replace('.', '/') + CLASS_FILE_EXTENSION,
                            NON_STRICT);
                    directClassFile.setAttributeFactory(new StdAttributeFactory());
                    dexFile.add(DISPATCHER.translate(directClassFile,
                            binaryRepresentation,
                            dexCompilerOptions,
                            dexFileOptions,
                            new DexFile(dexFileOptions)));
                }

                /**
                 * {@inheritDoc}
                 */
                public void drainTo(OutputStream outputStream) throws IOException {
                    dexFile.writeTo(outputStream, NO_PRINT_OUTPUT, NOT_VERBOSE);
                }
            }

            /**
             * A dispatcher for translating a direct class file.
             */
            protected interface Dispatcher {

                /**
                 * Creates a new class file definition.
                 *
                 * @param directClassFile      The direct class file to translate.
                 * @param binaryRepresentation The file's binary representation.
                 * @param dexCompilerOptions   The dex compiler options.
                 * @param dexFileOptions       The dex file options.
                 * @param dexFile              The dex file.
                 * @return The translated class file definition.
                 */
                ClassDefItem translate(DirectClassFile directClassFile,
                                       byte[] binaryRepresentation,
                                       CfOptions dexCompilerOptions,
                                       DexOptions dexFileOptions,
                                       DexFile dexFile);

                /**
                 * Sets the target API level if available.
                 *
                 * @param dexOptions     The dex options to set the api version for
                 * @param targetApiLevel The target API level.
                 */
                void setTargetApi(DexOptions dexOptions, int targetApiLevel);

                /**
                 * An unavailable dispatcher.
                 */
                class Unavailable implements Dispatcher {

                    /**
                     * A message explaining why the dispatcher is unavailable.
                     */
                    private final String message;

                    /**
                     * Creates a new unavailable dispatcher.
                     *
                     * @param message A message explaining why the dispatcher is unavailable.
                     */
                    protected Unavailable(String message) {
                        this.message = message;
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public ClassDefItem translate(DirectClassFile directClassFile,
                                                  byte[] binaryRepresentation,
                                                  CfOptions dexCompilerOptions,
                                                  DexOptions dexFileOptions,
                                                  DexFile dexFile) {
                        throw new IllegalStateException("Could not resolve dispatcher: " + message);
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public void setTargetApi(DexOptions dexOptions, int targetApiLevel) {
                        throw new IllegalStateException("Could not resolve dispatcher: " + message);
                    }
                }

                /**
                 * A dispatcher for a lagacy Android VM.
                 */
                class ForLegacyVm implements Dispatcher {

                    /**
                     * The {@code CfTranslator#translate(DirectClassFile, byte[], CfOptions, DexOptions, DexFile)} method.
                     */
                    private final Method translate;

                    /**
                     * The {@code DexOptions#targetApiLevel} field.
                     */
                    private final Field targetApi;

                    /**
                     * Creates a new dispatcher.
                     *
                     * @param translate The {@code CfTranslator#translate(DirectClassFile, byte[], CfOptions, DexOptions, DexFile)} method.
                     * @param targetApi The {@code DexOptions#targetApiLevel} field.
                     */
                    protected ForLegacyVm(Method translate, Field targetApi) {
                        this.translate = translate;
                        this.targetApi = targetApi;
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public ClassDefItem translate(DirectClassFile directClassFile,
                                                  byte[] binaryRepresentation,
                                                  CfOptions dexCompilerOptions,
                                                  DexOptions dexFileOptions,
                                                  DexFile dexFile) {
                        try {
                            return (ClassDefItem) translate.invoke(null,
                                    directClassFile,
                                    binaryRepresentation,
                                    dexCompilerOptions,
                                    dexFileOptions,
                                    dexFile);
                        } catch (IllegalAccessException exception) {
                            throw new IllegalStateException("Cannot access an Android dex file translation method", exception);
                        } catch (InvocationTargetException exception) {
                            throw new IllegalStateException("Cannot invoke Android dex file translation method", exception.getTargetException());
                        }
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public void setTargetApi(DexOptions dexOptions, int targetApiLevel) {
                        try {
                            targetApi.set(dexOptions, targetApiLevel);
                        } catch (IllegalAccessException exception) {
                            throw new IllegalStateException("Cannot access an Android dex file translation method", exception);
                        }
                    }
                }

                /**
                 * A dispatcher for an Android VM with API level 26 or higher.
                 */
                class ForApi26LevelCompatibleVm implements Dispatcher {

                    /**
                     * The {@code CfTranslator#translate(DxContext, DirectClassFile, byte[], CfOptions, DexOptions, DexFile)} method.
                     */
                    private final Method translate;

                    /**
                     * The {@code com.android.dx.command.dexer.DxContext#DxContext()} constructor.
                     */
                    private final Constructor<?> dxContext;

                    /**
                     * Creates a new dispatcher.
                     *
                     * @param translate The {@code CfTranslator#translate(DxContext, DirectClassFile, byte[], CfOptions, DexOptions, DexFile)} method.
                     * @param dxContext The {@code com.android.dx.command.dexer.DxContext#DxContext()} constructor.
                     */
                    protected ForApi26LevelCompatibleVm(Method translate, Constructor<?> dxContext) {
                        this.translate = translate;
                        this.dxContext = dxContext;
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public ClassDefItem translate(DirectClassFile directClassFile,
                                                  byte[] binaryRepresentation,
                                                  CfOptions dexCompilerOptions,
                                                  DexOptions dexFileOptions,
                                                  DexFile dexFile) {
                        try {
                            return (ClassDefItem) translate.invoke(null,
                                    dxContext.newInstance(),
                                    directClassFile,
                                    binaryRepresentation,
                                    dexCompilerOptions,
                                    dexFileOptions,
                                    dexFile);
                        } catch (IllegalAccessException exception) {
                            throw new IllegalStateException("Cannot access an Android dex file translation method", exception);
                        } catch (InstantiationException exception) {
                            throw new IllegalStateException("Cannot instantiate dex context", exception);
                        } catch (InvocationTargetException exception) {
                            throw new IllegalStateException("Cannot invoke Android dex file translation method", exception.getTargetException());
                        }
                    }

                    /**
                     * {@inheritDoc}
                     */
                    public void setTargetApi(DexOptions dexOptions, int targetApiLevel) {
                        /* do nothing */
                    }
                }
            }
        }
    }

    /**
     * An Android class loading strategy that creates a wrapper class loader that loads any type.
     */
    @RequiresApi(Build.VERSION_CODES.CUPCAKE)
    public static class Wrapping extends AndroidClassLoadingStrategy {

        /**
         * Creates a new wrapping class loading strategy for Android that uses the default SDK-compiler based dex processor.
         *
         * @param privateDirectory A directory that is <b>not shared with other applications</b> to be used for storing
         *                         generated classes and their processed forms.
         */
        public Wrapping(File privateDirectory) {
            this(privateDirectory, DexProcessor.ForSdkCompiler.makeDefault());
        }

        /**
         * Creates a new wrapping class loading strategy for Android.
         *
         * @param privateDirectory A directory that is <b>not shared with other applications</b> to be used for storing
         *                         generated classes and their processed forms.
         * @param dexProcessor     The dex processor to be used for creating a dex file out of Java files.
         */
        public Wrapping(File privateDirectory, DexProcessor dexProcessor) {
            super(privateDirectory, dexProcessor);
        }

        /**
         * {@inheritDoc}
         */
        protected Map<TypeDescription, Class<?>> doLoad(@MaybeNull ClassLoader classLoader, Set<TypeDescription> typeDescriptions, File jar) {
            ClassLoader dexClassLoader = new DexClassLoader(jar.getAbsolutePath(), privateDirectory.getAbsolutePath(), EMPTY_LIBRARY_PATH, classLoader);
            Map<TypeDescription, Class<?>> loadedTypes = new HashMap<TypeDescription, Class<?>>();
            for (TypeDescription typeDescription : typeDescriptions) {
                try {
                    loadedTypes.put(typeDescription, Class.forName(typeDescription.getName(), false, dexClassLoader));
                } catch (ClassNotFoundException exception) {
                    throw new IllegalStateException("Cannot load " + typeDescription, exception);
                }
            }
            return loadedTypes;
        }
    }

    /**
     * An Android class loading strategy that injects types into the target class loader.
     */
    @RequiresApi(Build.VERSION_CODES.CUPCAKE)
    public static class Injecting extends AndroidClassLoadingStrategy {

        /**
         * The dispatcher to use for loading a dex file.
         */
        private static final Dispatcher DISPATCHER;

        /*
         * Creates a dispatcher to use for loading a dex file.
         */
        static {
            Dispatcher dispatcher;
            try {
                dispatcher = new Dispatcher.ForAndroidPVm(BaseDexClassLoader.class.getMethod("addDexPath", String.class, boolean.class));
            } catch (Throwable ignored) {
                dispatcher = Dispatcher.ForLegacyVm.INSTANCE;
            }
            DISPATCHER = dispatcher;
        }

        /**
         * Creates a new injecting class loading strategy for Android that uses the default SDK-compiler based dex processor.
         *
         * @param privateDirectory A directory that is <b>not shared with other applications</b> to be used for storing
         *                         generated classes and their processed forms.
         */
        public Injecting(File privateDirectory) {
            this(privateDirectory, DexProcessor.ForSdkCompiler.makeDefault());
        }

        /**
         * Creates a new injecting class loading strategy for Android.
         *
         * @param privateDirectory A directory that is <b>not shared with other applications</b> to be used for storing
         *                         generated classes and their processed forms.
         * @param dexProcessor     The dex processor to be used for creating a dex file out of Java files.
         */
        public Injecting(File privateDirectory, DexProcessor dexProcessor) {
            super(privateDirectory, dexProcessor);
        }

        /**
         * {@inheritDoc}
         */
        public Map<TypeDescription, Class<?>> load(@MaybeNull ClassLoader classLoader, Map<TypeDescription, byte[]> types) {
            if (classLoader == null) {
                throw new IllegalArgumentException("Cannot inject classes into the bootstrap class loader on Android");
            }
            return super.load(classLoader, types);
        }

        /**
         * {@inheritDoc}
         */
        protected Map<TypeDescription, Class<?>> doLoad(@MaybeNull ClassLoader classLoader, Set<TypeDescription> typeDescriptions, File jar) throws IOException {
            dalvik.system.DexFile dexFile = DISPATCHER.loadDex(privateDirectory, jar, classLoader, randomString);
            try {
                Map<TypeDescription, Class<?>> loadedTypes = new HashMap<TypeDescription, Class<?>>();
                for (TypeDescription typeDescription : typeDescriptions) {
                    synchronized (classLoader) { // Guaranteed to be non-null by check in 'load' method.
                        Class<?> type = DISPATCHER.loadClass(dexFile, classLoader, typeDescription);
                        if (type == null) {
                            throw new IllegalStateException("Could not load " + typeDescription);
                        }
                        loadedTypes.put(typeDescription, type);
                    }
                }
                return loadedTypes;
            } finally {
                if (dexFile != null) {
                    dexFile.close();
                }
            }
        }

        /**
         * A dispatcher for loading a dex file.
         */
        protected interface Dispatcher {

            /**
             * Loads a dex file.
             *
             * @param privateDirectory The private directory to use if required.
             * @param jar              The jar to load.
             * @param classLoader      The class loader to inject into.
             * @param randomString     The random string to use.
             * @return The created {@link dalvik.system.DexFile} or {@code null} if no such file is created.
             * @throws IOException If an I/O exception is thrown.
             */
            @MaybeNull
            dalvik.system.DexFile loadDex(File privateDirectory, File jar, @MaybeNull ClassLoader classLoader, RandomString randomString) throws IOException;

            /**
             * Loads a class.
             *
             * @param dexFile         The dex file to process if any was created.
             * @param classLoader     The class loader to load the class from.
             * @param typeDescription The type to load.
             * @return The loaded class.
             */
            @MaybeNull
            Class<?> loadClass(dalvik.system.DexFile dexFile, @MaybeNull ClassLoader classLoader, TypeDescription typeDescription);

            /**
             * A dispatcher for legacy VMs that allow {@link dalvik.system.DexFile#loadDex(String, String, int)}.
             */
            enum ForLegacyVm implements Dispatcher {

                /**
                 * The singleton instance.
                 */
                INSTANCE;

                /**
                 * A constant indicating the use of no flags.
                 */
                private static final int NO_FLAGS = 0;

                /**
                 * A file extension used for holding Android's optimized data.
                 */
                private static final String EXTENSION = ".data";

                /**
                 * {@inheritDoc}
                 */
                @MaybeNull
                public dalvik.system.DexFile loadDex(File privateDirectory,
                                                     File jar,
                                                     @MaybeNull ClassLoader classLoader,
                                                     RandomString randomString) throws IOException {
                    return dalvik.system.DexFile.loadDex(jar.getAbsolutePath(),
                            new File(privateDirectory.getAbsolutePath(), randomString.nextString() + EXTENSION).getAbsolutePath(),
                            NO_FLAGS);
                }

                /**
                 * {@inheritDoc}
                 */
                @MaybeNull
                public Class<?> loadClass(dalvik.system.DexFile dexFile, @MaybeNull ClassLoader classLoader, TypeDescription typeDescription) {
                    return dexFile.loadClass(typeDescription.getName(), classLoader);
                }
            }

            /**
             * A dispatcher for an Android P VM that uses the reflection-only method {@code addDexPath} of {@link DexClassLoader}.
             */
            class ForAndroidPVm implements Dispatcher {

                /**
                 * Indicates that this dispatcher does not return a {@link dalvik.system.DexFile} instance.
                 */
                @AlwaysNull
                private static final dalvik.system.DexFile NO_RETURN_VALUE = null;

                /**
                 * The {@code BaseDexClassLoader#addDexPath(String, boolean)} method.
                 */
                private final Method addDexPath;

                /**
                 * Creates a new Android P-compatible dispatcher for loading a dex file.
                 *
                 * @param addDexPath The {@code BaseDexClassLoader#addDexPath(String, boolean)} method.
                 */
                protected ForAndroidPVm(Method addDexPath) {
                    this.addDexPath = addDexPath;
                }

                /**
                 * {@inheritDoc}
                 */
                @MaybeNull
                public dalvik.system.DexFile loadDex(File privateDirectory,
                                                     File jar,
                                                     @MaybeNull ClassLoader classLoader,
                                                     RandomString randomString) throws IOException {
                    if (!(classLoader instanceof BaseDexClassLoader)) {
                        throw new IllegalArgumentException("On Android P, a class injection can only be applied to BaseDexClassLoader: " + classLoader);
                    }
                    try {
                        // START CHROMIUM LOCAL MODIFICATION
                        addDexPath.invoke(classLoader, jar.getAbsolutePath(), false);
                        // END CHROMIUM LOCAL MODIFICATION
                        return NO_RETURN_VALUE;
                    } catch (IllegalAccessException exception) {
                        throw new IllegalStateException("Cannot access BaseDexClassLoader#addDexPath(String, boolean)", exception);
                    } catch (InvocationTargetException exception) {
                        Throwable cause = exception.getTargetException();
                        if (cause instanceof IOException) {
                            throw (IOException) cause;
                        } else {
                            throw new IllegalStateException("Cannot invoke BaseDexClassLoader#addDexPath(String, boolean)", cause);
                        }
                    }
                }

                /**
                 * {@inheritDoc}
                 */
                public Class<?> loadClass(@MaybeNull dalvik.system.DexFile dexFile, @MaybeNull ClassLoader classLoader, TypeDescription typeDescription) {
                    try {
                        return Class.forName(typeDescription.getName(), false, classLoader);
                    } catch (ClassNotFoundException exception) {
                        throw new IllegalStateException("Could not locate " + typeDescription, exception);
                    }
                }
            }
        }
    }
}