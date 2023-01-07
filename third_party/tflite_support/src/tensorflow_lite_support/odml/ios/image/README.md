# MLImage for iOS

MLImage is a lightweight image library for iOS developers. It provides a
common image format to increase interoperability among various on-device
machine learning frameworks.

Build the `MLImage` Objective-C library target:

```shell
bazel build -c opt --apple_platform_type=ios tensorflow_lite_support/odml/ios/image:MLImage
```

Build the `tests` target:

```shell
bazel test -c opt --apple_platform_type=ios tensorflow_lite_support/odml/ios/image:tests
```

#### Generate the Xcode project using Tulsi

Open the `//tensorflow_lite_support/odml/ios/image/MLImage.tulsiproj` using
the [TulsiApp](https://github.com/bazelbuild/tulsi) or by running the
[`generate_xcodeproj.sh`](https://github.com/bazelbuild/tulsi/blob/master/src/tools/generate_xcodeproj.sh):

```shell
generate_xcodeproj.sh --genconfig tensorflow_lite_support/odml/ios/image/MLImage.tulsiproj:MLImage --outputfolder ~/path/to/generated/MLImage.xcodeproj
```
