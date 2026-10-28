// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero.processor;

import com.sun.source.tree.ArrayTypeTree;
import com.sun.source.tree.CompilationUnitTree;
import com.sun.source.tree.ImportTree;
import com.sun.source.tree.MethodTree;
import com.sun.source.tree.ParameterizedTypeTree;
import com.sun.source.tree.Tree;
import com.sun.source.tree.VariableTree;
import com.sun.source.util.Trees;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;

import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.ProcessingEnvironment;
import javax.annotation.processing.RoundEnvironment;
import javax.annotation.processing.SupportedAnnotationTypes;
import javax.annotation.processing.SupportedOptions;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.DeclaredType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.util.Elements;
import javax.lang.model.util.Types;
import javax.tools.Diagnostic;

/**
 * Annotation processor that validates JNI signatures to catch type resolution limitations in JNI
 * Zero.
 *
 * <p>Specifically, JNI Zero cannot resolve unqualified references to nested types that are defined
 * in ancestor types (supertypes) because JNI Zero does not parse supertypes.
 *
 * <p>Example of failing case:
 *
 * <pre>
 * interface Foo {
 *   class Nested {}
 * }
 * class Bar implements Foo {
 *   &#64;CalledByNative Nested getNested() { ... } // Nested is unqualified and inherited
 * }
 * </pre>
 *
 * JNI Zero will incorrectly resolve `Nested` to `Bar$Nested` or `package.Nested`. This processor
 * flags such usages and requires qualifying them (e.g. `Foo.Nested`) or importing them explicitly.
 *
 * <h3>Turbine vs Non-Turbine Modes</h3>
 *
 * Turbine does not support the standard {@link com.sun.source.util.Trees} AST API.
 *
 * <ul>
 *   <li><b>Turbine Mode:</b> We rely on the `jni_zero.resolved_types_path` option passed from JNI
 *       Zero to verify that all resolved types actually exist in the classpath.
 *   <li><b>Non-Turbine Mode (mTrees is not null):</b> We use standard javac {@link Trees} AST APIs
 *       to inspect the AST and provide precise inline error messages for unqualified references.
 * </ul>
 */
@SupportedAnnotationTypes({
    "org.jni_zero.AccessedByNative",
    "org.jni_zero.CalledByNative",
    "org.jni_zero.CalledByNativeUnchecked",
    "org.jni_zero.CalledByNativeForTesting",
    "org.jni_zero.NativeMethods"
})
@SupportedSourceVersion(SourceVersion.RELEASE_17)
@SupportedOptions("jni_zero.resolved_types_path")
public class JniZeroValidatorProcessor extends AbstractProcessor {

    private boolean mResolvedTypesChecked = false;
    private JavacImpl mImpl;
    private Elements mElements;

    @Override
    public synchronized void init(ProcessingEnvironment processingEnv) {
        super.init(processingEnv);
        mElements = processingEnv.getElementUtils();
        try {
            // Trees.instance will throw IllegalArgumentException if not running under javac (e.g.
            // in Turbine).
            Trees trees = Trees.instance(processingEnv);
            mImpl = new JavacImpl(processingEnv, trees);
        } catch (IllegalArgumentException e) {
            mImpl = null;
        }
    }

    @Override
    public boolean process(Set<? extends TypeElement> annotations, RoundEnvironment roundEnv) {
        if (!mResolvedTypesChecked) {
            checkResolvedTypes();
            mResolvedTypesChecked = true;
        }
        if (mImpl == null) {
            return false;
        }
        for (TypeElement annotation : annotations) {
            if (annotation.getQualifiedName().toString().equals("org.jni_zero.NativeMethods")) {
                for (Element element : roundEnv.getElementsAnnotatedWith(annotation)) {
                    if (element.getKind() == ElementKind.INTERFACE) {
                        mImpl.checkNativeInterface((TypeElement) element);
                    }
                }
            } else {
                for (Element element : roundEnv.getElementsAnnotatedWith(annotation)) {
                    if (element.getKind() == ElementKind.METHOD) {
                        mImpl.checkCalledByNativeMethod((ExecutableElement) element);
                    } else if (element.getKind() == ElementKind.FIELD) {
                        mImpl.checkAccessedByNativeField((VariableElement) element);
                    }
                }
            }
        }
        return false;
    }

    private void checkResolvedTypes() {
        String path = processingEnv.getOptions().get("jni_zero.resolved_types_path");
        if (path == null) {
            return;
        }
        try {
            Path filePath = Paths.get(path);
            if (Files.exists(filePath)) {
                List<String> lines = Files.readAllLines(filePath);
                for (String line : lines) {
                    line = line.trim();
                    if (line.isEmpty() || line.startsWith("#")) {
                        continue;
                    }
                    TypeElement typeElement = mElements.getTypeElement(line);
                    if (typeElement == null) {
                        processingEnv
                                .getMessager()
                                .printMessage(
                                        Diagnostic.Kind.ERROR,
                                        "JNI Zero validation failed: Resolved type \""
                                                + line
                                                + "\" does not exist in classpath. (From "
                                                + path
                                                + ")");
                    }
                }
            } else {
                processingEnv
                        .getMessager()
                        .printMessage(
                                Diagnostic.Kind.WARNING,
                                "JNI Zero validation: Resolved types file does not exist: " + path);
            }
        } catch (Exception e) {
            processingEnv
                    .getMessager()
                    .printMessage(
                            Diagnostic.Kind.ERROR,
                            "JNI Zero validation: Error reading resolved types file "
                                    + path
                                    + ": "
                                    + e.toString());
        }
    }

    // Does not work with Turbine due to missing Trees AST API.
    private static class JavacImpl {
        private final ProcessingEnvironment processingEnv;
        private final Types mTypes;
        private final Elements mElements;
        private final Trees mTrees;

        JavacImpl(ProcessingEnvironment processingEnv, Trees trees) {
            this.processingEnv = processingEnv;
            this.mTypes = processingEnv.getTypeUtils();
            this.mElements = processingEnv.getElementUtils();
            this.mTrees = trees;
        }

        void checkNativeInterface(TypeElement interfaceElement) {
            for (Element member : interfaceElement.getEnclosedElements()) {
                if (member.getKind() == ElementKind.METHOD) {
                    checkJniMethodTrees((ExecutableElement) member);
                }
            }
        }

        void checkCalledByNativeMethod(ExecutableElement methodElement) {
            checkJniMethodTrees(methodElement);
        }

        void checkAccessedByNativeField(VariableElement fieldElement) {
            VariableTree varTree = (VariableTree) mTrees.getTree(fieldElement);
            if (varTree != null) {
                checkTypeTrees(fieldElement.asType(), fieldElement, varTree.getType());
            }
        }

        private void checkJniMethodTrees(ExecutableElement methodElement) {
            checkTypeTrees(
                    methodElement.getReturnType(), methodElement, getReturnTypeTree(methodElement));

            List<? extends VariableElement> parameters = methodElement.getParameters();
            List<Tree> parameterTrees = getParameterTrees(methodElement);
            for (int i = 0; i < parameters.size(); i++) {
                if (i < parameterTrees.size()) {
                    checkTypeTrees(
                            parameters.get(i).asType(), methodElement, parameterTrees.get(i));
                }
            }
        }

        private void checkTypeTrees(TypeMirror typeMirror, Element originElement, Tree typeTree) {
            if (typeTree == null) {
                return;
            }

            if (typeMirror.getKind() == TypeKind.ARRAY) {
                TypeMirror componentType = ((ArrayType) typeMirror).getComponentType();
                Tree componentTree = getComponentTree(typeTree);
                checkTypeTrees(componentType, originElement, componentTree);
                return;
            }

            if (typeMirror.getKind() != TypeKind.DECLARED) {
                return;
            }

            DeclaredType declaredType = (DeclaredType) typeMirror;
            TypeElement typeElement = (TypeElement) declaredType.asElement();

            if (typeElement.getEnclosingElement().getKind() == ElementKind.CLASS
                    || typeElement.getEnclosingElement().getKind() == ElementKind.INTERFACE) {

                TypeElement enclosingClass = getJniClass(originElement);
                if (enclosingClass != null) {
                    String refStr = typeTree.toString();
                    if (!isValidReference(
                            refStr,
                            typeElement,
                            enclosingClass,
                            fqn -> isFqnImportedTrees(fqn, originElement))) {
                        TypeElement outerElement = (TypeElement) typeElement.getEnclosingElement();
                        printErrorTrees(
                                originElement,
                                typeTree,
                                "JNI Zero limitation: Unqualified reference to nested type \""
                                        + typeElement.getSimpleName()
                                        + "\" inherited from supertype or sibling \""
                                        + outerElement.getQualifiedName()
                                        + "\". JNI Zero cannot resolve this. "
                                        + "Please qualify it (e.g. \""
                                        + outerElement.getSimpleName()
                                        + "."
                                        + typeElement.getSimpleName()
                                        + "\") or import it explicitly.");
                    }
                }
            }

            // Check type arguments
            List<? extends TypeMirror> typeArguments = declaredType.getTypeArguments();
            List<Tree> typeArgTrees = getTypeArgumentTrees(typeTree);
            for (int i = 0; i < typeArguments.size(); i++) {
                if (i < typeArgTrees.size()) {
                    checkTypeTrees(typeArguments.get(i), originElement, typeArgTrees.get(i));
                }
            }
        }

        private TypeElement getEnclosingClass(Element element) {
            while (element != null
                    && element.getKind() != ElementKind.CLASS
                    && element.getKind() != ElementKind.INTERFACE) {
                element = element.getEnclosingElement();
            }
            return (TypeElement) element;
        }

        private TypeElement getJniClass(Element element) {
            TypeElement enclosing = getEnclosingClass(element);
            if (enclosing == null) {
                return null;
            }
            if (enclosing.getKind() == ElementKind.INTERFACE) {
                if (hasAnnotation(enclosing, "org.jni_zero.NativeMethods")) {
                    Element outer = enclosing.getEnclosingElement();
                    if (outer != null
                            && (outer.getKind() == ElementKind.CLASS
                                    || outer.getKind() == ElementKind.INTERFACE)) {
                        return (TypeElement) outer;
                    }
                }
            }
            return enclosing;
        }

        private boolean hasAnnotation(Element element, String annotationFqn) {
            for (AnnotationMirror anno : element.getAnnotationMirrors()) {
                String fqn =
                        ((TypeElement) anno.getAnnotationType().asElement())
                                .getQualifiedName()
                                .toString();
                if (fqn.equals(annotationFqn)) {
                    return true;
                }
            }
            return false;
        }

        private boolean isSupertype(TypeElement superCandidate, TypeElement subElement) {
            return mTypes.isSubtype(subElement.asType(), mTypes.erasure(superCandidate.asType()));
        }

        private boolean isCalledByNative(Element element) {
            for (AnnotationMirror anno : element.getAnnotationMirrors()) {
                String annoFqn =
                        ((TypeElement) anno.getAnnotationType().asElement())
                                .getQualifiedName()
                                .toString();
                if (annoFqn.startsWith("org.jni_zero.CalledByNative")) {
                    return true;
                }
            }
            return false;
        }

        private boolean isValidReference(
                String refStr,
                TypeElement typeElement,
                TypeElement enclosingClass,
                Predicate<String> isImported) {
            if (typeElement.getEnclosingElement().getKind() != ElementKind.CLASS
                    && typeElement.getEnclosingElement().getKind() != ElementKind.INTERFACE) {
                return true;
            }

            String pkgName = mElements.getPackageOf(enclosingClass).getQualifiedName().toString();
            TypeElement topLevelClass = getTopLevelClass(typeElement);
            String topLevelFqn = topLevelClass.getQualifiedName().toString();

            refStr = refStr.replaceAll("\\s*\\.\\s*", ".");

            if (refStr.equals(topLevelFqn) || refStr.startsWith(topLevelFqn + ".")) {
                return true;
            }

            List<TypeElement> chain = getEnclosingChain(typeElement);
            for (TypeElement ancestor : chain) {
                String simpleName = ancestor.getSimpleName().toString();
                if (refStr.equals(simpleName) || refStr.startsWith(simpleName + ".")) {
                    boolean isTopLevel =
                            ancestor.getEnclosingElement().getKind() == ElementKind.PACKAGE;
                    boolean isTarget = ancestor.equals(typeElement);

                    if (isTopLevel) {
                        String ancestorFqn = ancestor.getQualifiedName().toString();
                        String ancestorPkg =
                                mElements.getPackageOf(ancestor).getQualifiedName().toString();
                        if (ancestorPkg.equals(pkgName) || isImported.test(ancestorFqn)) {
                            return true;
                        }
                    } else {
                        String ancestorFqn = ancestor.getQualifiedName().toString();
                        if (isImported.test(ancestorFqn)) {
                            return true;
                        } else if (isTarget
                                && ancestor.getEnclosingElement().equals(topLevelClass)) {
                            return true;
                        }
                    }
                    break;
                }
            }
            return false;
        }

        private List<TypeElement> getEnclosingChain(TypeElement element) {
            List<TypeElement> chain = new ArrayList<>();
            Element current = element;
            while (current != null
                    && (current.getKind() == ElementKind.CLASS
                            || current.getKind() == ElementKind.INTERFACE)) {
                chain.add((TypeElement) current);
                current = current.getEnclosingElement();
            }
            return chain;
        }

        private TypeElement getTopLevelClass(TypeElement element) {
            Element enclosing = element.getEnclosingElement();
            while (enclosing != null && enclosing.getKind() != ElementKind.PACKAGE) {
                element = (TypeElement) enclosing;
                enclosing = element.getEnclosingElement();
            }
            return element;
        }

        private boolean isFqnImportedTrees(String fqn, Element originElement) {
            CompilationUnitTree cut = mTrees.getPath(originElement).getCompilationUnit();
            if (cut == null) {
                return false;
            }
            for (ImportTree importTree : cut.getImports()) {
                if (importTree.isStatic()) {
                    continue;
                }
                String importStr = importTree.getQualifiedIdentifier().toString();
                if (importStr.endsWith(".*")) {
                    String importPackage = importStr.substring(0, importStr.length() - 2);
                    int lastDot = fqn.lastIndexOf('.');
                    if (lastDot != -1) {
                        String parentFqn = fqn.substring(0, lastDot);
                        if (importPackage.equals(parentFqn)) {
                            return true;
                        }
                    }
                } else {
                    if (importStr.equals(fqn)) {
                        return true;
                    }
                }
            }
            return false;
        }

        private void printErrorTrees(Element element, Tree tree, String message) {
            mTrees.printMessage(
                    Diagnostic.Kind.ERROR,
                    message,
                    tree,
                    mTrees.getPath(element).getCompilationUnit());
        }

        private Tree getReturnTypeTree(ExecutableElement methodElement) {
            MethodTree methodTree = mTrees.getTree(methodElement);
            if (methodTree != null) {
                return methodTree.getReturnType();
            }
            return null;
        }

        private List<Tree> getParameterTrees(ExecutableElement methodElement) {
            List<Tree> typeTrees = new ArrayList<>();
            MethodTree methodTree = mTrees.getTree(methodElement);
            if (methodTree != null) {
                for (VariableTree varTree : methodTree.getParameters()) {
                    typeTrees.add(varTree.getType());
                }
            }
            return typeTrees;
        }

        private List<Tree> getTypeArgumentTrees(Tree typeTree) {
            List<Tree> result = new ArrayList<>();
            if (typeTree != null && typeTree.getKind() == Tree.Kind.PARAMETERIZED_TYPE) {
                ParameterizedTypeTree paramTree = (ParameterizedTypeTree) typeTree;
                result.addAll(paramTree.getTypeArguments());
            }
            return result;
        }

        private Tree getComponentTree(Tree typeTree) {
            if (typeTree != null && typeTree.getKind() == Tree.Kind.ARRAY_TYPE) {
                return ((ArrayTypeTree) typeTree).getType();
            }
            return null;
        }
    }
}
