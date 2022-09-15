// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc makeicns2.m -o makeicns2 -fobjc-arc -framework Foundation -framework
// CoreGraphics

// Note that this doesn't handle @2x icons. If those are ever needed, add an
// "int scale" parameter to parallel the "int size" one.

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <assert.h>
#include <libkern/OSByteOrder.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  FourCharCode type;
  uint32_t length;
} BlockHeader;

BlockHeader MakeBlockHeader(FourCharCode type, uint32_t length) {
  BlockHeader header = {type, length};
  header.type = OSSwapHostToBigInt32(header.type);
  header.length = OSSwapHostToBigInt32(header.length);

  return header;
}

typedef struct {
  FourCharCode image_type;
  FourCharCode mask_type;
} ImageAndMaskType;

ImageAndMaskType ImageAndMaskTypesFromSize(int size) {
  switch (size) {
    case 16: {
      ImageAndMaskType types = {'is32', 's8mk'};
      return types;
    }
    case 32: {
      ImageAndMaskType types = {'il32', 'l8mk'};
      return types;
    }
    case 48: {
      ImageAndMaskType types = {'ih32', 'h8mk'};
      return types;
    }
    case 128: {
      ImageAndMaskType types = {'it32', 't8mk'};
      return types;
    }
    default:
      assert(false);
  }
}

FourCharCode PNGIconTypeFromSize(int size) {
  switch (size) {
    case 16:
      return 'icp4';
    case 32:
      return 'icp5';
    case 64:
      return 'icp6';
    case 128:
      return 'ic07';
    case 256:
      return 'ic08';
    case 512:
      return 'ic09';
    default:
      assert(false);
  }
}

bool CGImageHasAlphaData(CGImageRef image) {
  // If the image specifies that it has no alpha, early-out.
  CGImageAlphaInfo alpha_info = CGImageGetAlphaInfo(image);
  if (alpha_info == kCGImageAlphaNone ||
      alpha_info == kCGImageAlphaNoneSkipFirst ||
      alpha_info == kCGImageAlphaNoneSkipLast) {
    return false;
  }

  // Otherwise, the image might specify that it is a full RGBA image but not
  // actually contain alpha data. Extract the alpha channel and examine it.

  size_t image_width = CGImageGetWidth(image);
  size_t image_height = CGImageGetHeight(image);

  CGContextRef context =
      CGBitmapContextCreate(/*data=*/NULL, image_width, image_height,
                            /*bitsPerComponent=*/8, /*bytesPerRow=*/image_width,
                            /*space=*/NULL, kCGImageAlphaOnly);
  CGContextDrawImage(context, CGRectMake(0, 0, image_width, image_height),
                     image);
  unsigned char* context_data = CGBitmapContextGetData(context);

  bool alpha_observed = false;
  for (size_t i = 0; i < image_width * image_height; ++i) {
    if (context_data[i] == 0) {
      alpha_observed = true;
      break;
    }
  }

  CGContextRelease(context);

  return alpha_observed;
}

void CreateDataOrImageFromPNG(NSString* iconset,
                              int size,
                              NSData** out_png_data,
                              CGImageRef* out_image_ref) {
  NSString* png_pathname =
      [NSString stringWithFormat:@"%@/%d.png", iconset, size];
  NSData* png_data = [NSData dataWithContentsOfFile:png_pathname];
  if (!png_data) {
    fprintf(stderr, "Error: Failed to read %s\n", png_pathname.UTF8String);
    exit(EXIT_FAILURE);
  }

  if (out_png_data)
    *out_png_data = png_data;

  // Even if the image isn't requested by the caller, still proceed to create
  // the image in order to check its attributes and the general fact that it
  // correctly parses as a PNG file.

  CGDataProviderRef data_provider =
      CGDataProviderCreateWithCFData((CFDataRef)png_data);
  if (!data_provider) {
    fprintf(stderr, "Error: Failed to create a data provider from %s\n",
            png_pathname.UTF8String);
    exit(EXIT_FAILURE);
  }

  CGImageRef image = CGImageCreateWithPNGDataProvider(
      data_provider, NULL, false, kCGRenderingIntentDefault);
  if (!image) {
    fprintf(stderr, "Error: Failed to create image from %s\n",
            png_pathname.UTF8String);
    exit(EXIT_FAILURE);
  }

  CGDataProviderRelease(data_provider);

  size_t image_width = CGImageGetWidth(image);
  size_t image_height = CGImageGetHeight(image);
  if (image_width != size || image_height != size) {
    fprintf(stderr,
            "Error: Expected %s to be of size %dx%d; found it to be of size "
            "%zdx%zd\n",
            png_pathname.UTF8String, size, size, image_width, image_height);
    exit(EXIT_FAILURE);
  }

  if (!CGImageHasAlphaData(image)) {
    fprintf(stderr, "Error: Expected %s to have alpha data but it does not\n",
            png_pathname.UTF8String);
    exit(EXIT_FAILURE);
  }

  if (out_image_ref)
    *out_image_ref = image;
  else
    CGImageRelease(image);
}

NSArray<NSMutableData*>* ARGBDataFromImage(CGImageRef image) {
  size_t image_width = CGImageGetWidth(image);
  size_t image_height = CGImageGetHeight(image);
  CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGContextRef context = CGBitmapContextCreate(
      NULL, image_width, image_height, 8 /* bitsPerComponent */,
      4 * image_width /* bytesPerRow */, color_space,
      kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Big /* ARGB */);
  assert(context);
  CGColorSpaceRelease(color_space);

  CGContextDrawImage(context, CGRectMake(0, 0, image_width, image_height),
                     image);

  NSMutableData* alpha_data =
      [NSMutableData dataWithCapacity:image_width * image_height];
  NSMutableData* red_data =
      [NSMutableData dataWithCapacity:image_width * image_height];
  NSMutableData* green_data =
      [NSMutableData dataWithCapacity:image_width * image_height];
  NSMutableData* blue_data =
      [NSMutableData dataWithCapacity:image_width * image_height];

  unsigned char* context_data = CGBitmapContextGetData(context);
  for (unsigned char* pixel = context_data;
       pixel < context_data + 4 * image_width * image_height; pixel += 4) {
    // Unfortunately, as noted in
    // https://developer.apple.com/library/archive/qa/qa1037/_index.html and
    // verified as still being true on 10.14, CGBitmapContextCreate() cannot
    // create unpremultiplied contexts, and therefore messed up the edges of the
    // PNG file. Therefore, unpremultiply all the image data.
    //
    // There is no visible degradation due to precision loss. This only has an
    // effect at the edges of the icon, anyway, as the opaque parts of the icon
    // have an alpha of 0xFF, whose inverse is 1, and the unpremultiplying ends
    // up not changing the value of the pixel.
    unsigned char* alpha = pixel;
    unsigned char* red = pixel + 1;
    unsigned char* green = pixel + 2;
    unsigned char* blue = pixel + 3;

    if (*alpha) {
      float inverse = 255.0 / *alpha;
      *red = MIN(255, (int)(inverse * *red));
      *green = MIN(255, (int)(inverse * *green));
      *blue = MIN(255, (int)(inverse * *blue));
    } else {
      *red = 0;
      *green = 0;
      *blue = 0;
    }

    [alpha_data appendBytes:alpha length:1];
    [red_data appendBytes:red length:1];
    [green_data appendBytes:green length:1];
    [blue_data appendBytes:blue length:1];
  }

  CGContextRelease(context);

  return @[ alpha_data, red_data, green_data, blue_data ];
}

void AppendRLEImageData(NSData* data, NSMutableData* rle_data) {
  // The packing loop is done with two offsets:
  //   - unpacked_offset: this is the offset of the start of the bytes that have
  //                      not yet been written to the block
  //   - current_offset: this is the offset used to search for byte runs
  //
  // |unpacked_offset| lags behind. The code scours through the data, looking
  // for runs of length greater than 3 (since only runs of 3 or longer can be
  // compressed). As soon as a run is found, all the data from |unpacked_offset|
  // is dumped as literal data, then the run is dumped, and then the search
  // continues.

  const char* data_bytes = data.bytes;
  const size_t data_length = data.length;
  size_t unpacked_offset = 0;
  size_t current_offset = 0;

  // Search for runs through the block of data, byte by byte.
  while (current_offset < data_length) {
    char current_byte = data_bytes[current_offset];
    size_t run_length = 1;
    while (current_offset + run_length < data_length && run_length < 130 &&
           data_bytes[current_offset + run_length] == current_byte) {
      ++run_length;
    }
    if (run_length >= 3) {
      // A long-enough run was found. First, dump all the data before the run
      // into the output block.
      while (unpacked_offset < current_offset) {
        // Because uncompressed data runs max out at 128 bytes of data, cap the
        // uncompressed run at 128 bytes.
        size_t uncompressed_length = MIN(current_offset - unpacked_offset, 128);
        // Key byte values of 0..127 mean 1..128 bytes of uncompressed data.
        unsigned char key_byte = uncompressed_length - 1;
        [rle_data appendBytes:&key_byte length:1];
        [rle_data appendBytes:data_bytes + unpacked_offset
                       length:uncompressed_length];
        unpacked_offset += uncompressed_length;
      }
      // Now that the output block is caught up, put the run that was just found
      // into it. Key byte values of 128..255 mean 3..130 copies of the
      // following byte, thus the addition of 125 to the run length.
      unsigned char key_byte = run_length + 125;
      [rle_data appendBytes:&key_byte length:1];
      [rle_data appendBytes:&current_byte length:1];
      current_offset += run_length;
      unpacked_offset = current_offset;
    } else {
      // The run is too small, so keep looking.
      current_offset += run_length;
    }
  }
  // At this point, there are no more runs, so pack the rest of the data into
  // the output block.
  while (unpacked_offset < current_offset) {
    // Because uncompressed data runs max out at 128 bytes of data, cap the
    // uncompressed run at 128 bytes.
    size_t uncompressed_length = MIN(current_offset - unpacked_offset, 128);
    // Key byte values of 0..127 mean 1..128 bytes of uncompressed data.
    unsigned char key_byte = uncompressed_length - 1;
    [rle_data appendBytes:&key_byte length:1];
    [rle_data appendBytes:data_bytes + unpacked_offset
                   length:uncompressed_length];
    unpacked_offset += uncompressed_length;
  }
}

NSArray<NSData*>* ImageAndMaskIconBlocksForIconOfSize(NSString* iconset,
                                                      int size) {
  CGImageRef image;
  CreateDataOrImageFromPNG(iconset, size, nil, &image);
  NSArray<NSMutableData*>* argb_data = ARGBDataFromImage(image);
  CGImageRelease(image);

  // Notes:
  //   - *Only* the image is RLE-encoded. The mask, no matter how compressible
  //     it is (and boy, is it!), is not RLE-encoded.
  //   - The red, green, and blue data must be *separately* RLE-encoded, and
  //     then combined to make the image block. If they are combined *before*
  //     encoding, then it's likely there will be an RLE run that spans colors
  //     and that will cause the image to be decoded incorrectly.

  ImageAndMaskType types = ImageAndMaskTypesFromSize(size);

  NSMutableData* mask_block = argb_data.firstObject;

  NSMutableData* image_block = [NSMutableData data];
  if (types.image_type == 'it32') {
    // For whatever unknown reason, the 'it32' data starts with four unused
    // bytes. Not a single reference guide has the slightest clue as to why this
    // is.
    [image_block appendBytes:"\0\0\0\0" length:4];
  }

  for (int i = 1; i < argb_data.count; ++i)
    AppendRLEImageData(argb_data[i], image_block);

  BlockHeader image_header = MakeBlockHeader(
      types.image_type, image_block.length + sizeof(BlockHeader));
  [image_block replaceBytesInRange:NSMakeRange(0, 0)
                         withBytes:&image_header
                            length:sizeof(image_header)];

  BlockHeader mask_header =
      MakeBlockHeader(types.mask_type, mask_block.length + sizeof(BlockHeader));
  [mask_block replaceBytesInRange:NSMakeRange(0, 0)
                        withBytes:&mask_header
                           length:sizeof(mask_header)];

  return @[ image_block, mask_block ];
}

NSData* PNGIconBlockForIconOfSize(NSString* iconset, int size) {
  NSData* png_data;
  CreateDataOrImageFromPNG(iconset, size, &png_data, NULL);

  NSUInteger icon_block_length = png_data.length + sizeof(BlockHeader);
  NSMutableData* icon_block =
      [NSMutableData dataWithCapacity:icon_block_length];

  BlockHeader block_header =
      MakeBlockHeader(PNGIconTypeFromSize(size), icon_block_length);
  [icon_block appendBytes:&block_header length:sizeof(block_header)];
  [icon_block appendData:png_data];

  return icon_block;
}

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    NSString* iconset = @(argv[i]);
    fprintf(stdout, "Processing iconset %s\n", iconset.UTF8String);

    if (![iconset hasSuffix:@".iconset"]) {
      fprintf(stderr, "Error: %s does not have the '.iconset' suffix\n",
              iconset.UTF8String);
      exit(EXIT_FAILURE);
    }

    NSMutableArray<NSData*>* blocks = [NSMutableArray array];

    // Add the standard Chromium icon sizes.
    [blocks
        addObjectsFromArray:ImageAndMaskIconBlocksForIconOfSize(iconset, 16)];
    [blocks
        addObjectsFromArray:ImageAndMaskIconBlocksForIconOfSize(iconset, 32)];
    [blocks
        addObjectsFromArray:ImageAndMaskIconBlocksForIconOfSize(iconset, 128)];
    [blocks addObject:PNGIconBlockForIconOfSize(iconset, 256)];
    [blocks addObject:PNGIconBlockForIconOfSize(iconset, 512)];

    // Add the TOC. It contains every block header, and starts with one of its
    // own.
    NSUInteger toc_block_length = (blocks.count + 1) * sizeof(BlockHeader);
    NSMutableData* toc_block =
        [NSMutableData dataWithCapacity:toc_block_length];

    BlockHeader toc_header = MakeBlockHeader('TOC ', toc_block_length);
    [toc_block appendBytes:&toc_header length:sizeof(toc_header)];
    for (NSData* block in blocks)
      [toc_block appendBytes:block.bytes length:sizeof(BlockHeader)];
    [blocks insertObject:toc_block atIndex:0];

    // Build the .icns data.
    NSMutableData* icns_data = [NSMutableData data];
    for (NSData* block in blocks)
      [icns_data appendData:block];

    BlockHeader file_header =
        MakeBlockHeader('icns', icns_data.length + sizeof(BlockHeader));
    [icns_data replaceBytesInRange:NSMakeRange(0, 0)
                         withBytes:&file_header
                            length:sizeof(BlockHeader)];

    // Write the .icns file.
    NSMutableString* icns_name = [NSMutableString stringWithString:iconset];
    [icns_name deleteCharactersInRange:NSMakeRange(iconset.length - 8,
                                                   8)];  // Strip ".iconset".
    [icns_name appendString:@".icns"];

    BOOL result = [icns_data writeToFile:icns_name atomically:NO];
    if (!result) {
      fprintf(stderr, "Failed to write icns file %s\n", icns_name.UTF8String);
      exit(EXIT_FAILURE);
    }
  }

  return EXIT_SUCCESS;
}
