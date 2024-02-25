/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ==============================================================================*/
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/task/vision/sources/TFLImageSearcher.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

NS_ASSUME_NONNULL_BEGIN

static NSString *const kExpectedTaskErrorDomain = @"org.tensorflow.lite.tasks";

static NSString *const kSearcherModelName = @"mobilenet_v3_small_100_224_searcher";
static NSString *const kEmbedderModelName = @"mobilenet_v3_small_100_224_embedder";
static NSString *const kMobileNetIndexName = @"searcher_index";

#define ValidateError(error, expectedDomain, expectedCode, expectedLocalizedDescription) \
  XCTAssertNotNil(error);                                                                \
  XCTAssertEqualObjects(error.domain, expectedDomain);                                   \
  XCTAssertEqual(error.code, expectedCode);                                              \
  XCTAssertNotEqual(                                                                     \
      [error.localizedDescription rangeOfString:expectedLocalizedDescription].location,  \
      NSNotFound)

#define ValidateSearchResultCount(searchResult, expectedNearestNeighborsCount) \
  XCTAssertNotNil(searchResult);                                               \
  XCTAssertEqual(searchResult.nearestNeighbors.count, expectedNearestNeighborsCount);

#define ValidateNearestNeighbor(nearestNeighbor, expectedMetadata, expectedDistance) \
  XCTAssertEqualObjects(nearestNeighbor.metadata, expectedMetadata);                 \
  XCTAssertEqualWithAccuracy(nearestNeighbor.distance, expectedDistance, 1e-6);

@interface TFLImageSearcherTests : XCTestCase
@end

@implementation TFLImageSearcherTests

- (void)setUp {
  [super setUp];
}

- (NSString *)filePathWithName:(NSString *)fileName extension:(NSString *)extension {
  NSString *filePath = [[NSBundle bundleForClass:self.class] pathForResource:fileName
                                                                      ofType:extension];
  XCTAssertNotNil(filePath);

  return filePath;
}

- (TFLImageSearcherOptions *)imageSearcherOptionsWithModelName:(NSString *)modelName {
  NSString *modelPath = [self filePathWithName:modelName extension:@"tflite"];
  TFLImageSearcherOptions *imageSearcherOptions =
      [[TFLImageSearcherOptions alloc] initWithModelPath:modelPath];

  return imageSearcherOptions;
}

- (TFLImageSearcher *)defaultImageSearcherWithModelName:(NSString *)modelName
                                          indexFileName:(nullable NSString *)indexFileName {
  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:modelName];

  if ([indexFileName length] > 0) {
    NSString *indexFilePath = [self filePathWithName:indexFileName extension:@"ldb"];
    imageSearcherOptions.searchOptions.indexFile.filePath = indexFilePath;
  }

  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:nil];
  XCTAssertNotNil(imageSearcher);

  return imageSearcher;
}

- (void)validateSearchResult:(TFLSearchResult *)searchResult {
  ValidateSearchResultCount(searchResult,
                            5  // expectedNearestNeighborsCount
  );

  ValidateNearestNeighbor(searchResult.nearestNeighbors[0],
                          @"burger",  // expectedMetadata
                          198.456329  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[1],
                          @"car",     // expectedMetadata
                          226.022186  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[2],
                          @"bird",    // expectedMetadata
                          227.297668  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[3],
                          @"dog",     // expectedMetadata
                          229.133789  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[4],
                          @"cat",     // expectedMetadata
                          229.718948  // expectedDistance
  );
}

- (void)validateSearchResultForRegionOfInterest:(TFLSearchResult *)searchResult {
  ValidateSearchResultCount(searchResult,
                            5  // expectedNearestNeighborsCount
  );

  ValidateNearestNeighbor(searchResult.nearestNeighbors[0],
                          @"burger",     // expectedMetadata
                          179.349853516  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[1],
                          @"car",        // expectedMetadata
                          203.803939819  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[2],
                          @"bird",       // expectedMetadata
                          205.671005249  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[3],
                          @"dog",        // expectedMetadata
                          207.130584717  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[4],
                          @"cat",        // expectedMetadata
                          207.447616577  // expectedDistance
  );
}

- (void)validateSearchResultsWithNormalization:(TFLSearchResult *)searchResult {
  ValidateSearchResultCount(searchResult,
                            5  // expectedNearestNeighborsCount
  );

  ValidateNearestNeighbor(searchResult.nearestNeighbors[0],
                          @"burger",        // expectedMetadata
                          0.00766587257385  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[1],
                          @"car",       // expectedMetadata
                          1.8352342844  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[2],
                          @"bird",       // expectedMetadata
                          1.91979730129  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[3],
                          @"dog",        // expectedMetadata
                          2.04152798653  // expectedDistance
  );
  ValidateNearestNeighbor(searchResult.nearestNeighbors[4],
                          @"cat",        // expectedMetadata
                          2.08032441139  // expectedDistance
  );
}

- (void)testInferenceWithSearchModelOnMLImageWithUIImage {
  TFLImageSearcher *imageSearcher = [self defaultImageSearcherWithModelName:kSearcherModelName
                                                              indexFileName:nil];
  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLSearchResult *searchResult = [imageSearcher searchWithGMLImage:gmlImage error:nil];
  [self validateSearchResult:searchResult];
}

- (void)testInferenceWithEmbedderModelAndIndexFileOnMLImageWithUIImage {
  TFLImageSearcher *imageSearcher = [self defaultImageSearcherWithModelName:kEmbedderModelName
                                                              indexFileName:kMobileNetIndexName];
  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLSearchResult *searchResult = [imageSearcher searchWithGMLImage:gmlImage error:nil];
  [self validateSearchResult:searchResult];
}

- (void)testSearchWithNormalizationSucceeds {
  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:kSearcherModelName];
  imageSearcherOptions.embeddingOptions.l2Normalize = YES;

  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:nil];

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLSearchResult *searchResult = [imageSearcher searchWithGMLImage:gmlImage error:nil];
  [self validateSearchResultsWithNormalization:searchResult];
}

- (void)testSearchWithMaxResultsSucceeds {
  const NSInteger searchResultCount = 2;

  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:kSearcherModelName];
  imageSearcherOptions.searchOptions.maxResults = searchResultCount;

  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:nil];

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLSearchResult *searchResult = [imageSearcher searchWithGMLImage:gmlImage error:nil];
  ValidateSearchResultCount(searchResult,
                            searchResultCount  // expectedNearestNeighborsCount
  );
}

- (void)testSearchWithRegionOfInterestSucceeds {
  TFLImageSearcher *imageSearcher = [self defaultImageSearcherWithModelName:kEmbedderModelName
                                                              indexFileName:kMobileNetIndexName];
  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  CGRect roi = CGRectMake(0,    // x
                          0,    // y
                          400,  // width
                          325   // height
  );
  TFLSearchResult *searchResult = [imageSearcher searchWithGMLImage:gmlImage
                                                   regionOfInterest:roi
                                                              error:nil];
  [self validateSearchResultForRegionOfInterest:searchResult];
}

- (void)testCreateImageSearcherWithQuantizeOptionFails {
  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:kSearcherModelName];
  imageSearcherOptions.embeddingOptions.quantize = YES;

  NSError *error = nil;
  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:&error];

  XCTAssertNil(imageSearcher);
  ValidateError(error,
                kExpectedTaskErrorDomain,                 // expectedErrorDomain
                TFLSupportErrorCodeInvalidArgumentError,  // expectedErrorCode
                @"Setting EmbeddingOptions.quantize = true "
                @"is not allowed"  // expectedErrorMessage
  );
}

- (void)testCreateImageSearcherWithInvalidMaxResultsFails {
  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:kSearcherModelName];
  imageSearcherOptions.searchOptions.maxResults = -1;

  NSError *error = nil;
  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:&error];

  XCTAssertNil(imageSearcher);
  ValidateError(error,
                kExpectedTaskErrorDomain,                           // expectedErrorDomain
                TFLSupportErrorCodeInvalidArgumentError,            // expectedErrorCode
                @"SearchOptions.max_results must be > 0, found -1"  // expectedErrorMessage
  );
}

- (void)testImageSearcherWithEmbedderModelAndInvalidIndexFileFails {
  TFLImageSearcherOptions *imageSearcherOptions =
      [self imageSearcherOptionsWithModelName:kEmbedderModelName];

  NSError *error = nil;
  TFLImageSearcher *imageSearcher = [TFLImageSearcher imageSearcherWithOptions:imageSearcherOptions
                                                                         error:&error];

  XCTAssertNil(imageSearcher);
  ValidateError(error,
                kExpectedTaskErrorDomain,                                // expectedErrorDomain
                TFLSupportErrorCodeMetadataAssociatedFileNotFoundError,  // expectedErrorCode
                @"SearchOptions.index_file is not set"                   // expectedErrorMessage
  );
}

@end

NS_ASSUME_NONNULL_END
