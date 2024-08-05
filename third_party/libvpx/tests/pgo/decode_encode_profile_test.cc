// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a PGO test for the decoder and encoder

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libvpx/source/libvpx/test/codec_factory.h"
#include "third_party/libvpx/source/libvpx/test/i420_video_source.h"
#include "third_party/libvpx/source/libvpx/test/util.h"
#include "third_party/libvpx/source/libvpx/test/webm_video_source.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#include "third_party/libvpx/source/libvpx/vpx_ports/vpx_timer.h"
#include "third_party/libvpx/source/libvpx/vpx_util/vpx_pthread.h"

namespace {

using std::make_tuple;

constexpr int kMaxPsnr = 100;
constexpr double kUsecsInSec = 1000000.0;
constexpr vpx_rational kTimebase = {33333333, 1000000000};

/**
 * Test vectors:
 *   Parameter ID 0 - video file name
 *   Parameter ID 1 - number of threads
 */
using DecodePerfParam = std::tuple<const char*, unsigned>;

const DecodePerfParam kVP9DecodePerfVectors[] = {
    make_tuple("./vp90-2-bbb_426x240_tile_1x1_180kbps.webm", 4),
    make_tuple("./vp90-2-tos_1280x534_tile_1x4_1306kbps.webm", 2),
    make_tuple("./vp90-2-tos_1920x800_tile_1x4_fpm_2335kbps.webm", 2),
    make_tuple("./vp90-2-bbb_1280x720_tile_1x4_1310kbps.webm", 2),
    make_tuple("./vp90-2-tos_854x356_tile_1x2_fpm_546kbps.webm", 2),
    make_tuple("./vp90-2-sintel_1280x546_tile_1x4_1257kbps.webm", 2),
    make_tuple("./vp90-2-sintel_640x272_tile_1x2_318kbps.webm", 2),
    make_tuple("./vp90-2-tos_1280x534_tile_1x4_fpm_952kbps.webm", 2),
    make_tuple("./vp90-2-bbb_1920x1080_tile_1x1_2581kbps.webm", 2),
    make_tuple("./vp90-2-sintel_1920x818_tile_1x4_fpm_2279kbps.webm", 2),
    make_tuple("./vp90-2-tos_854x356_tile_1x2_656kbps.webm", 2),
    make_tuple("./vp90-2-bbb_640x360_tile_1x2_337kbps.webm", 2),
    make_tuple("./vp90-2-tos_426x178_tile_1x1_181kbps.webm", 2),
    make_tuple("./vp90-2-sintel_854x364_tile_1x2_621kbps.webm", 2),
    make_tuple("./vp90-2-bbb_1920x1080_tile_1x4_2586kbps.webm", 2),
    make_tuple("./vp90-2-sintel_426x182_tile_1x1_171kbps.webm", 2),
    make_tuple("./vp90-2-bbb_854x480_tile_1x2_651kbps.webm", 2),
    make_tuple("./vp90-2-tos_640x266_tile_1x2_336kbps.webm", 2),
    make_tuple("./vp90-2-bbb_1920x1080_tile_1x4_fpm_2304kbps.webm", 2),
};

/**
 * Class for obtaining video frames from memory.
 *
 * The class is used to pass frames from the decoder to the encoder.
 * Using an in-memory source allows to exercise the encoder on embedded
 * devices with limited storage space, where storing large YUV files
 * is not feasible.
 */
class InMemoryVideoSource : public ::libvpx_test::VideoSource {
 public:
  InMemoryVideoSource() : img_(nullptr) {
    pthread_mutex_init(&mutex_, nullptr);
    pthread_mutex_init(&producer_mutex_, nullptr);
    pthread_cond_init(&cond_, nullptr);
    pthread_cond_init(&cond_producer_, nullptr);
    end_of_input_ = false;
  }

  ~InMemoryVideoSource() override = default;

  void ReleaseImage() {
    pthread_mutex_lock(&mutex_);
    img_ = nullptr;
    pthread_mutex_unlock(&mutex_);
    pthread_cond_signal(&cond_producer_);
  }

  void SetImage(const vpx_image_t* img, bool end_of_input = false) {
    pthread_mutex_lock(&mutex_);
    /**
     * It is not trivial to copy the vpx_image_t struct, since the raw
     * buffer size is not stored.
     * The buffer is locked by the encoder until the frame is encoded.
     */
    // Silence -Wcast-qual warnings
    img_ = const_cast<vpx_image_t*>(img);
    end_of_input_ = end_of_input;
    pthread_mutex_unlock(&mutex_);
    pthread_cond_signal(&cond_);
  }

  /**
   * Wait for the encoder to finish encoding of the current frame.
   */
  void WaitForEncoder() {
    pthread_mutex_lock(&producer_mutex_);

    while (img_ != nullptr) {
      pthread_cond_wait(&cond_producer_, &producer_mutex_);
    }

    pthread_mutex_unlock(&producer_mutex_);
  }

  void Begin() override {}

  void Next() override {
    // Wait for the next frame, img to be non-null. Use cond to signal
    pthread_mutex_lock(&mutex_);

    while (img_ == nullptr && !end_of_input_) {
      pthread_cond_wait(&cond_, &mutex_);
    }

    pthread_mutex_unlock(&mutex_);
  }

  vpx_image_t* img() const override { return img_; }

  vpx_codec_pts_t pts() const override { return 0; }

  unsigned long duration() const override { return 1; }

  vpx_rational_t timebase() const override { return {33333333, 1000000000}; }

  unsigned int frame() const override { return 0; }

  unsigned int limit() const override { return 0; }

 private:
  vpx_image_t* img_;
  pthread_mutex_t mutex_;
  pthread_mutex_t producer_mutex_;
  pthread_cond_t cond_;
  pthread_cond_t cond_producer_;
  bool end_of_input_;
};

THREADFN EncoderThreadAdapter(void* obj);

class VP9M2MEncoder : public ::libvpx_test::EncoderTest {
 public:
  VP9M2MEncoder(unsigned int threads)
      : EncoderTest(&libvpx_test::kVP9),
        initialized_(false),
        min_psnr_(kMaxPsnr),
        encoding_mode_(::libvpx_test::kRealTime),
        speed_(0),
        threads_(threads) {
    InitializeConfig();
    SetMode(encoding_mode_);

    cfg_.g_timebase = kTimebase;
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_resize_allowed = 0;
    cfg_.rc_end_usage = VPX_CBR;
    cfg_.g_error_resilient = 1;
    cfg_.g_threads = threads_;
  }

  ~VP9M2MEncoder() override = default;

  void Shutdown() {
    SetImage(nullptr, true);
    abort_ = true;
    pthread_join(thread_, nullptr);
  }

  void RunLoopEncode() { RunLoop(&in_mem_source_); }

  void WaitForEncoder() { in_mem_source_.WaitForEncoder(); }

  bool SetImage(const vpx_image_t* img, bool end_of_input = false) {
    in_mem_source_.SetImage(img, end_of_input);

    if (!initialized_) {
      Init(img);
    }

    return initialized_;
  }

 protected:
  void ReleaseImage() { in_mem_source_.ReleaseImage(); }

  void PostEncodeFrameHook(libvpx_test::Encoder* /*encoder*/) override {
    // Call ReleaseImage to set img_ to null.
    // This signals that the frame has been encoded.
    ReleaseImage();
  }

  void PreEncodeFrameHook(::libvpx_test::VideoSource* video,
                          ::libvpx_test::Encoder* encoder) override {
    if (video->frame() == 0) {
      const int log2_tile_columns = 3;
      encoder->Control(VP8E_SET_CPUUSED, speed_);
      encoder->Control(VP9E_SET_TILE_COLUMNS, log2_tile_columns);
      encoder->Control(VP9E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(VP8E_SET_ENABLEAUTOALTREF, 0);
    }
  }

  void Run(void* arg) {
    pthread_create(&thread_, nullptr, EncoderThreadAdapter, arg);
  }

  void BeginPassHook(unsigned int /*pass*/) override { min_psnr_ = kMaxPsnr; }

  void PSNRPktHook(const vpx_codec_cx_pkt_t* pkt) override {
    if (pkt->data.psnr.psnr[0] < min_psnr_) {
      min_psnr_ = pkt->data.psnr.psnr[0];
    }
  }

  bool DoDecode() const override { return false; }

  bool Init(const vpx_image_t* img) {
    cfg_.rc_target_bitrate = img->bps;
    init_flags_ = VPX_CODEC_USE_PSNR;
    cfg_.g_w = img->d_w;
    cfg_.g_h = img->d_h;
    cfg_.g_threads = threads_;
    initialized_ = true;
    speed_ = 9;

    Run(this);
    return initialized_;
  }

 private:
  bool initialized_;
  double min_psnr_;
  libvpx_test::TestMode encoding_mode_;
  int speed_;
  unsigned int threads_;
  pthread_t thread_;
  InMemoryVideoSource in_mem_source_;
};

THREADFN EncoderThreadAdapter(void* obj) {
  VP9M2MEncoder* p = reinterpret_cast<VP9M2MEncoder*>(obj);
  p->RunLoopEncode();
  return THREAD_EXIT_SUCCESS;
}

class DecodeEncodePerfTest : public ::testing::TestWithParam<DecodePerfParam> {
 public:
  DecodeEncodePerfTest()
      : video_name_(GET_PARAM(0)), thread_count_(GET_PARAM(1)) {}

  const char* GetParamVideoName() const { return video_name_; }

  unsigned int GetParamThreadCount() const { return thread_count_; }

 private:
  const char* video_name_;
  unsigned int thread_count_;
};

// In-memory decode and encode test for Profile Guided Optimizations
//
//  This performance test is designed to exercise the decoder and encoder
//  and generate a compilation profile for the decoder and encoder.
//  Contrary to the name we don't really care to measure the actual test case
//  performance, but rather to hit as many code paths as possible.
//  The decoder and encoder run on separate threads, but the frames are
//  synchronized instead of using a buffer queue.
//  TODO(crbug.com/chromium/325103518): introduce noise to the decoded frames.

TEST_P(DecodeEncodePerfTest, PerfTest) {
  VP9M2MEncoder encoder(GetParamThreadCount());

  libvpx_test::WebMVideoSource video(GetParamVideoName());

  video.Init();

  vpx_codec_dec_cfg_t cfg = vpx_codec_dec_cfg_t();
  cfg.threads = GetParamThreadCount();
  libvpx_test::VP9Decoder decoder(cfg, 0);
  libvpx_test::DxDataIterator dec_iter = decoder.GetDxData();

  vpx_usec_timer t;
  vpx_usec_timer_start(&t);

  for (video.Begin(); video.cxdata() != nullptr; video.Next()) {
    const vpx_image_t* d_img;
    decoder.DecodeFrame(video.cxdata(), video.frame_size());
    d_img = dec_iter.Next();
    ASSERT_TRUE(encoder.SetImage(d_img));
    encoder.WaitForEncoder();
  }

  printf("Exiting\n");
  encoder.Shutdown();

  vpx_usec_timer_mark(&t);
  const double elapsed_secs_ =
      static_cast<double>(vpx_usec_timer_elapsed(&t)) / kUsecsInSec;
  const unsigned frames = video.frame_number();
  const double fps = static_cast<double>(frames) / elapsed_secs_;

  printf("{\n");
  printf("\t\"type\" : \"decode_perf_test\",\n");
  printf("\t\"version\" : \"%s\",\n", vpx_codec_version_str());
  printf("\t\"videoName\" : \"%s\",\n", GetParamVideoName());
  printf("\t\"threadCount\" : %u,\n", GetParamThreadCount());
  printf("\t\"decodeTimeSecs\" : %f,\n", elapsed_secs_);
  printf("\t\"totalFrames\" : %u,\n", frames);
  printf("\t\"framesPerSecond\" : %f\n", fps);
  printf("}\n");
}

INSTANTIATE_TEST_SUITE_P(VP9,
                         DecodeEncodePerfTest,
                         ::testing::ValuesIn(kVP9DecodePerfVectors));
}  // namespace
