/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.ImageLoaderTest');
goog.setTestOnly();

const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const GoogPromise = goog.require('goog.Promise');
const ImageLoader = goog.require('goog.net.ImageLoader');
const NetEventType = goog.require('goog.net.EventType');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const TEST_EVENT_TYPES = [
  EventType.LOAD,
  NetEventType.COMPLETE,
  NetEventType.ERROR,
];

/**
 * Mapping from test image file name to:
 * [expected width, expected height, expected event to be fired].
 */
const TEST_IMAGES = {
  'imageloader_testimg1.gif': [20, 20, EventType.LOAD],
  'imageloader_testimg2.gif': [20, 20, EventType.LOAD],
  'imageloader_testimg3.gif': [32, 32, EventType.LOAD],

  'this-is-not-image-1.gif': [0, 0, NetEventType.ERROR],
  'this-is-not-image-2.gif': [0, 0, NetEventType.ERROR],
};

let startTime;
let loader;

function assertImagesAreCorrect(results) {
  assertEquals(googObject.getCount(TEST_IMAGES), googObject.getCount(results));
  googObject.forEach(TEST_IMAGES, (value, key) => {
    // Check if fires the COMPLETE event.
    assertTrue('Image is not loaded completely.', key in results);

    const image = results[key];

    // Check image size.
    assertEquals('Image width is not correct', value[0], image[0]);
    assertEquals('Image length is not correct', value[1], image[1]);

    // Check if fired the correct event.
    assertEquals('Event *' + value[2] + '* must be fired', value[2], image[2]);
  });
}

/**
 * Overrides the loader's loadImage_ method so that it dispatches an image
 * loaded event immediately, causing any event listeners to receive them
 * synchronously.  This allows tests to assume synchronous execution.
 */
function makeLoaderSynchronous(loader) {
  const originalLoadImage = loader.loadImage_;
  loader.loadImage_ = function(request, id) {
    originalLoadImage.call(this, request, id);

    const event = new GoogEvent(EventType.LOAD);
    /** @suppress {globalThis} suppression added to enable type checking */
    event.currentTarget = this.imageIdToImageMap_[id];
    loader.onNetworkEvent_(event);
  };

  // Make listen() a no-op.
  loader.handler_.listen = goog.nullFunction;
}

testSuite({
  setUpPage() {
    // Increase the timeout to 5 seconds to allow more time for images to load.
    TestCase.getActiveTestCase().promiseTimeout = 5 * 1000;
  },

  setUp() {
    startTime = Date.now();

    loader = new ImageLoader();

    // Adds test images to the loader.
    let i = 0;
    for (const key in TEST_IMAGES) {
      const imageId = 'img_' + i++;
      loader.addImage(imageId, key);
    }
  },

  tearDown() {
    dispose(loader);
  },

  /** Tests loading image and disposing before loading completes. */
  testDisposeInTheMiddleOfLoadingWorks() {
    const resolver = GoogPromise.withResolver();

    events.listen(loader, TEST_EVENT_TYPES, (e) => {
      assertFalse(
          'Handler is still invoked after loader is disposed.',
          loader.isDisposed());

      switch (e.type) {
        case NetEventType.COMPLETE:
          resolver.reject('This test should never get COMPLETE event.');
          return;

        case EventType.LOAD:
        case NetEventType.ERROR:
          loader.dispose();
          break;
      }

      // Make sure that handler is never called again after disposal before
      // marking test as successful.
      Timer.callOnce(() => {
        resolver.resolve();
      }, 500);
    });

    loader.start();
    return resolver.promise;
  },

  /** Tests loading of images until completion. */
  testLoadingUntilCompletion() {
    const resolver = GoogPromise.withResolver();
    const results = {};
    events.listen(loader, TEST_EVENT_TYPES, (e) => {
      let image;
      switch (e.type) {
        case EventType.LOAD:
          image = e.target;
          results[image.src.substring(image.src.lastIndexOf('/') + 1)] =
              [image.naturalWidth, image.naturalHeight, e.type];
          return;

        case NetEventType.ERROR:
          image = e.target;
          results[image.src.substring(image.src.lastIndexOf('/') + 1)] =
              [image.naturalWidth, image.naturalHeight, e.type];
          return;

        case NetEventType.COMPLETE:
          try {
            assertImagesAreCorrect(results);
          } catch (e) {
            resolver.reject(e);
            return;
          }
          resolver.resolve();
          return;
      }
    });

    loader.start();
    return resolver.promise;
  },

  /**
   * Verifies that if an additional image is added after start() was called, but
   * before COMPLETE was dispatched, no COMPLETE event is sent.  Verifies
   * COMPLETE is finally sent when .start() is called again and all images have
   * now completed loading.
   */
  testImagesAddedAfterStart() {
    // Use synchronous image loading.
    makeLoaderSynchronous(loader);

    // Add another image once the first images finishes loading.
    events.listenOnce(loader, EventType.LOAD, () => {
      loader.addImage('extra_image', 'extra_image.gif');
    });

    // Keep track of the total # of image loads.
    const loadRecordFn = recordFunction();
    events.listen(loader, EventType.LOAD, loadRecordFn);

    // Keep track of how many times COMPLETE was dispatched.
    const completeRecordFn = recordFunction();
    events.listen(loader, NetEventType.COMPLETE, completeRecordFn);

    // Start testing.
    loader.start();
    assertEquals(
        'COMPLETE event should not have been dispatched yet: An image was ' +
            'added after the initial batch was started.',
        0, completeRecordFn.getCallCount());
    assertEquals(
        'Just the test images should have loaded',
        googObject.getCount(TEST_IMAGES), loadRecordFn.getCallCount());

    loader.start();
    assertEquals(
        'COMPLETE should have been dispatched once.', 1,
        completeRecordFn.getCallCount());
    assertEquals(
        'All images should have been loaded',
        googObject.getCount(TEST_IMAGES) + 1, loadRecordFn.getCallCount());
  },

  /**
   * Verifies that more images can be added after an upload starts, and start()
   * can be called for them, resulting in just one COMPLETE event once all the
   * images have completed.
   */
  testImagesAddedAndStartedAfterStart() {
    // Use synchronous image loading.
    makeLoaderSynchronous(loader);

    // Keep track of the total # of image loads.
    const loadRecordFn = recordFunction();
    events.listen(loader, EventType.LOAD, loadRecordFn);

    // Add more images once the first images finishes loading, and call start()
    // to get them going.
    events.listenOnce(loader, EventType.LOAD, (e) => {
      loader.addImage('extra_image', 'extra_image.gif');
      loader.addImage('extra_image2', 'extra_image2.gif');
      loader.start();
    });

    // Keep track of how many times COMPLETE was dispatched.
    const completeRecordFn = recordFunction();
    events.listen(loader, NetEventType.COMPLETE, completeRecordFn);

    // Start testing.  Make sure all 7 images loaded.
    loader.start();
    assertEquals(
        'COMPLETE should have been dispatched once.', 1,
        completeRecordFn.getCallCount());
    assertEquals(
        'All images should have been loaded',
        googObject.getCount(TEST_IMAGES) + 2, loadRecordFn.getCallCount());
  },

  /**
   * Verifies that if images are removed after loading has started, COMPLETE
   * is dispatched once the remaining images have finished.
   */
  testImagesRemovedAfterStart() {
    // Use synchronous image loading.
    makeLoaderSynchronous(loader);

    // Remove 2 images once the first image finishes loading.
    events.listenOnce(loader, EventType.LOAD, function(e) {
      loader.removeImage(
          googArray.peek(googObject.getKeys(this.imageIdToRequestMap_)));
      loader.removeImage(
          googArray.peek(googObject.getKeys(this.imageIdToRequestMap_)));
    });

    // Keep track of the total # of image loads.
    const loadRecordFn = recordFunction();
    events.listen(loader, EventType.LOAD, loadRecordFn);

    // Keep track of how many times COMPLETE was dispatched.
    const completeRecordFn = recordFunction();
    events.listen(loader, NetEventType.COMPLETE, completeRecordFn);

    // Start testing.  Make sure only the 3 images remaining loaded.
    loader.start();
    assertEquals(
        'COMPLETE should have been dispatched once.', 1,
        completeRecordFn.getCallCount());
    assertEquals(
        'All images should have been loaded',
        googObject.getCount(TEST_IMAGES) - 2, loadRecordFn.getCallCount());
  },

  /**
   * Verifies order of event dispatch when events are handled by a client of
   * {@link goog.net.ImageLoader}.
   */
  testImageLoaderClientEventDispatchOrder() {
    const clientLoader = new ImageLoader();
    makeLoaderSynchronous(clientLoader);

    // Creates a testing client that will dispose of the image loader on the
    // final propagated LOAD event.
    const testingClientImageLoader = new TestingClientImageLoader(clientLoader);

    let i = 0;
    for (const key in TEST_IMAGES) {
      const imageId = 'img_' + i++;
      testingClientImageLoader.addImage(imageId, key);
    }

    // Add more images once the first images finishes loading, and call start()
    // to get them going.
    events.listenOnce(testingClientImageLoader, EventType.LOAD, (e) => {
      testingClientImageLoader.addImage('extra_image', 'extra_image.gif');
      testingClientImageLoader.addImage('extra_image2', 'extra_image2.gif');
      testingClientImageLoader.start();
    });

    // Start testing.
    testingClientImageLoader.start();

    assertEquals(
        'All images should have dispatched a LOAD call before disposing.',
        googObject.getCount(TEST_IMAGES), testingClientImageLoader.loadCount);
    assertEquals(
        'COMPLETE should never be dispatched if we dispose the instance on image removal.',
        0, testingClientImageLoader.completeCount);
    assertEquals(
        'There should be no references to images in the image loader at time of dispose.',
        testingClientImageLoader.imageLoaderRemainingSize, 0);
  },

  /**
     Verifies that the correct image attribute is set when using CORS requests.
   */
  testSetsCorsAttribute() {
    // Use synchronous image loading.
    makeLoaderSynchronous(loader);

    // Verify the crossOrigin attribute of the requested images.
    events.listen(loader, EventType.LOAD, (e) => {
      const image = e.target;
      if (image.id == 'cors_request') {
        assertEquals(
            'CORS requested image should have a crossOrigin attribute set',
            'anonymous', image.crossOrigin);
      } else {
        assertTrue(
            'Non-CORS requested images should not have a crossOrigin attribute',
            googString.isEmptyOrWhitespace(
                googString.makeSafe(image.crossOrigin)));
      }
    });

    // Make a new request for one of the images, this time using CORS.
    const srcs = googObject.getKeys(TEST_IMAGES);
    loader.addImage(
        'cors_request', srcs[0], ImageLoader.CorsRequestType.ANONYMOUS);
    loader.start();
  },
});

/**
 * Represents an example client that uses {@link goog.net.ImageLoader} and
 * consumes the events it emits.
 */
class TestingClientImageLoader extends GoogEventTarget {
  constructor(imageLoader) {
    super();

    /** @private @const {!ImageLoader} */
    this.imageLoader_ = imageLoader;
    this.eventHandler_ = new EventHandler(this);

    this.imagesRemaining = 0;
    this.imageLoaderRemainingSize = 0;
    this.loadCount = 0;
    this.completeCount = 0;

    // Handles events dispatched from ImageLoad
    this.eventHandler_.listen(
        this.imageLoader_, [EventType.LOAD, NetEventType.COMPLETE],
        this.handleImageLoaderEvent_);
  }

  addImage(id, url) {
    this.imagesRemaining += 1;
    this.imageLoader_.addImage(id, url);
  }

  start() {
    this.imageLoader_.start();
  }

  /**
   * Disposes the image loader when handling the final image, prior to
   * dispatching the COMPLETE event. This allows verification that the state of
   * the ImageLoader is clean before the original event is dispatched to any
   * clients.
   * @private
   */
  handleImageLoaderEvent_(e) {
    switch (e.type) {
      case NetEventType.COMPLETE:
        this.completeCount += 1;

      case EventType.LOAD:
        this.loadCount += 1;
        this.imagesRemaining -= 1;
    }

    /** @suppress {visibility} suppression added to enable type checking */
    this.imageLoaderRemainingSize =
        googObject.getKeys(this.imageLoader_.imageIdToRequestMap_).length;

    if (this.imagesRemaining == 0) {
      this.imageLoader_.disposeInternal();
    }
  }
}
