// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/single_keyframe_effect_animation.h"
#include "cc/layers/layer.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/skia_util.h"

using cc::MatchesPNGFile;
using cc::WritePNGFile;

namespace ui {

namespace {

// There are three test classes in here that configure the Compositor and
// Layer's slightly differently:
// - LayerWithNullDelegateTest uses NullLayerDelegate as the LayerDelegate. This
//   is typically the base class you want to use.
// - LayerWithDelegateTest uses LayerDelegate on the delegates.
// - LayerWithRealCompositorTest when a real compositor is required for testing.
//    - Slow because they bring up a window and run the real compositor. This
//      is typically not what you want.

class ColoredLayer : public Layer, public LayerDelegate {
 public:
  explicit ColoredLayer(SkColor color)
      : Layer(LAYER_TEXTURED),
        color_(color) {
    set_delegate(this);
  }

  ~ColoredLayer() override {}

  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, size());
    recorder.canvas()->DrawColor(color_);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  SkColor color_;
};

// Layer delegate for painting text with fade effect on canvas.
class DrawFadedStringLayerDelegate : public LayerDelegate {
 public:
  DrawFadedStringLayerDelegate(SkColor back_color, const gfx::Size& layer_size)
      : background_color_(back_color), layer_size_(layer_size) {}

  ~DrawFadedStringLayerDelegate() override {}

  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer_size_);
    gfx::Rect bounds(layer_size_);
    recorder.canvas()->DrawColor(background_color_);
    const base::string16 text = base::ASCIIToUTF16("Tests!");
    recorder.canvas()->DrawFadedString(text, font_list_, SK_ColorRED, bounds,
                                       0);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  const SkColor background_color_;
  const gfx::FontList font_list_;
  const gfx::Size layer_size_;

  DISALLOW_COPY_AND_ASSIGN(DrawFadedStringLayerDelegate);
};

class LayerWithRealCompositorTest : public testing::Test {
 public:
  LayerWithRealCompositorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    if (base::PathService::Get(gfx::DIR_TEST_DATA, &test_data_directory_)) {
      test_data_directory_ = test_data_directory_.AppendASCII("compositor");
    } else {
      LOG(ERROR) << "Could not open test data directory.";
    }
    gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
  }
  ~LayerWithRealCompositorTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    bool enable_pixel_output = true;
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                     &context_factory_private);

    const gfx::Rect host_bounds(10, 10, 500, 500);
    compositor_host_.reset(TestCompositorHost::Create(
        host_bounds, context_factory, context_factory_private));
    compositor_host_->Show();
  }

  void TearDown() override {
    ResetCompositor();
    TerminateContextFactoryForTests();
  }

  Compositor* GetCompositor() { return compositor_host_->GetCompositor(); }

  void ResetCompositor() {
    compositor_host_.reset();
  }

  Layer* CreateLayer(LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

  std::unique_ptr<Layer> CreateDrawFadedStringLayerDelegate(
      const gfx::Rect& bounds,
      DrawFadedStringLayerDelegate* delegate) {
    std::unique_ptr<Layer> layer(new Layer(LAYER_TEXTURED));
    layer->SetBounds(bounds);
    layer->set_delegate(delegate);
    return layer;
  }

  void DrawTree(Layer* root) {
    GetCompositor()->SetRootLayer(root);
    GetCompositor()->ScheduleDraw();
    WaitForSwap();
  }

  void ReadPixels(SkBitmap* bitmap) {
    ReadPixels(bitmap, gfx::Rect(GetCompositor()->size()));
  }

  void ReadPixels(SkBitmap* bitmap, gfx::Rect source_rect) {
    scoped_refptr<ReadbackHolder> holder(new ReadbackHolder);
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
            base::BindOnce(&ReadbackHolder::OutputRequestCallback, holder));
    request->set_area(source_rect);

    GetCompositor()->root_layer()->RequestCopyOfOutput(std::move(request));

    // Wait for copy response.  This needs to wait as the compositor could
    // be in the middle of a draw right now, and the commit with the
    // copy output request may not be done on the first draw.
    for (int i = 0; i < 2; i++) {
      GetCompositor()->ScheduleFullRedraw();
      WaitForDraw();
    }

    // Waits for the callback to finish run and return result.
    holder->WaitForReadback();

    *bitmap = holder->result();
  }

  void WaitForDraw() {
    ui::DrawWaiterForTest::WaitForCompositingStarted(GetCompositor());
  }

  void WaitForSwap() {
    ui::DrawWaiterForTest::WaitForCompositingEnded(GetCompositor());
  }

  void WaitForCommit() {
    ui::DrawWaiterForTest::WaitForCommit(GetCompositor());
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  const base::FilePath& test_data_directory() const {
    return test_data_directory_;
  }

 private:
  class ReadbackHolder : public base::RefCountedThreadSafe<ReadbackHolder> {
   public:
    ReadbackHolder() : run_loop_(new base::RunLoop) {}

    void OutputRequestCallback(std::unique_ptr<viz::CopyOutputResult> result) {
      if (result->IsEmpty())
        result_.reset();
      else
        result_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
      run_loop_->Quit();
    }

    void WaitForReadback() { run_loop_->Run(); }

    const SkBitmap& result() const { return *result_; }

   private:
    friend class base::RefCountedThreadSafe<ReadbackHolder>;

    virtual ~ReadbackHolder() {}

    std::unique_ptr<SkBitmap> result_;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<TestCompositorHost> compositor_host_;

  // The root directory for test files.
  base::FilePath test_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithRealCompositorTest);
};

// LayerDelegate that paints colors to the layer.
class TestLayerDelegate : public LayerDelegate {
 public:
  TestLayerDelegate() { reset(); }
  ~TestLayerDelegate() override {}

  void AddColor(SkColor color) {
    colors_.push_back(color);
  }

  int color_index() const { return color_index_; }

  float device_scale_factor() const {
    return device_scale_factor_;
  }

  void set_layer_bounds(const gfx::Rect& layer_bounds) {
    layer_bounds_ = layer_bounds;
  }

  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer_bounds_.size());
    recorder.canvas()->DrawColor(colors_[color_index_]);
    color_index_ = (color_index_ + 1) % static_cast<int>(colors_.size());
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    device_scale_factor_ = new_device_scale_factor;
  }

  MOCK_METHOD2(OnLayerBoundsChanged,
               void(const gfx::Rect&, PropertyChangeReason));
  MOCK_METHOD2(OnLayerTransformed,
               void(const gfx::Transform&, PropertyChangeReason));
  MOCK_METHOD1(OnLayerOpacityChanged, void(PropertyChangeReason));
  MOCK_METHOD0(OnLayerAlphaShapeChanged, void());

  void reset() {
    color_index_ = 0;
    device_scale_factor_ = 0.0f;
  }

 private:
  std::vector<SkColor> colors_;
  int color_index_;
  float device_scale_factor_;
  gfx::Rect layer_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

// LayerDelegate that verifies that a layer was asked to update its canvas.
class DrawTreeLayerDelegate : public LayerDelegate {
 public:
  DrawTreeLayerDelegate(const gfx::Rect& layer_bounds)
      : painted_(false), layer_bounds_(layer_bounds) {}
  ~DrawTreeLayerDelegate() override {}

  void Reset() {
    painted_ = false;
  }

  bool painted() const { return painted_; }

 private:
  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    painted_ = true;
    ui::PaintRecorder recorder(context, layer_bounds_.size());
    recorder.canvas()->DrawColor(SK_ColorWHITE);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  bool painted_;
  const gfx::Rect layer_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DrawTreeLayerDelegate);
};

// The simplest possible layer delegate. Does nothing.
class NullLayerDelegate : public LayerDelegate {
 public:
  NullLayerDelegate() {}
  ~NullLayerDelegate() override {}

  gfx::Rect invalidation() const { return invalidation_; }

 private:
  gfx::Rect invalidation_;

  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    invalidation_ = context.InvalidationForTesting();
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  DISALLOW_COPY_AND_ASSIGN(NullLayerDelegate);
};

// Remembers if it has been notified.
class TestCompositorObserver : public CompositorObserver {
 public:
  TestCompositorObserver() = default;

  bool committed() const { return committed_; }
  bool notified() const { return started_ && ended_; }

  void Reset() {
    committed_ = false;
    started_ = false;
    ended_ = false;
  }

 private:
  void OnCompositingDidCommit(Compositor* compositor) override {
    committed_ = true;
  }

  void OnCompositingStarted(Compositor* compositor,
                            base::TimeTicks start_time) override {
    started_ = true;
  }

  void OnCompositingEnded(Compositor* compositor) override { ended_ = true; }

  void OnCompositingChildResizing(Compositor* compositor) override {}

  void OnCompositingShuttingDown(Compositor* compositor) override {}

  bool committed_ = false;
  bool started_ = false;
  bool ended_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorObserver);
};

class TestCompositorAnimationObserver : public CompositorAnimationObserver {
 public:
  explicit TestCompositorAnimationObserver(ui::Compositor* compositor)
      : compositor_(compositor),
        animation_step_count_(0),
        shutdown_(false) {
    DCHECK(compositor_);
    compositor_->AddAnimationObserver(this);
  }

  ~TestCompositorAnimationObserver() override {
    if (compositor_)
      compositor_->RemoveAnimationObserver(this);
  }

  size_t animation_step_count() const { return animation_step_count_; }
  bool shutdown() const { return shutdown_; }

 private:
  void OnAnimationStep(base::TimeTicks timestamp) override {
    ++animation_step_count_;
  }

  void OnCompositingShuttingDown(Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
    shutdown_ = true;
  }

  ui::Compositor* compositor_;
  size_t animation_step_count_;
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorAnimationObserver);
};

}  // namespace

TEST_F(LayerWithRealCompositorTest, Draw) {
  std::unique_ptr<Layer> layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 50, 50)));
  DrawTree(layer.get());
}

// Create this hierarchy:
// L1 - red
// +-- L2 - blue
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, Hierarchy) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));
  std::unique_ptr<Layer> l3(
      CreateColorLayer(SK_ColorYELLOW, gfx::Rect(5, 5, 25, 25)));
  std::unique_ptr<Layer> l4(
      CreateColorLayer(SK_ColorMAGENTA, gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  DrawTree(l1.get());
}

class LayerWithDelegateTest : public testing::Test {
 public:
  LayerWithDelegateTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~LayerWithDelegateTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;

    InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                     &context_factory_private);

    const gfx::Rect host_bounds(1000, 1000);
    compositor_host_.reset(TestCompositorHost::Create(
        host_bounds, context_factory, context_factory_private));
    compositor_host_->Show();
  }

  void TearDown() override {
    compositor_host_.reset();
    TerminateContextFactoryForTests();
  }

  Compositor* compositor() { return compositor_host_->GetCompositor(); }

  virtual Layer* CreateLayer(LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  virtual Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

  void DrawTree(Layer* root) {
    compositor()->SetRootLayer(root);
    Draw();
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  // Invokes DrawTree on the compositor.
  void Draw() {
    compositor()->ScheduleDraw();
    WaitForDraw();
  }

  void WaitForDraw() {
    DrawWaiterForTest::WaitForCompositingStarted(compositor());
  }

  void WaitForCommit() {
    DrawWaiterForTest::WaitForCommit(compositor());
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<TestCompositorHost> compositor_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithDelegateTest);
};

void ReturnMailbox(bool* run, const gpu::SyncToken& sync_token, bool is_lost) {
  *run = true;
}

TEST(LayerStandaloneTest, ReleaseMailboxOnDestruction) {
  std::unique_ptr<Layer> layer(new Layer(LAYER_TEXTURED));
  bool callback_run = false;
  auto resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());
  layer->SetTransferableResource(
      resource,
      viz::SingleReleaseCallback::Create(
          base::BindOnce(ReturnMailbox, &callback_run)),
      gfx::Size(10, 10));
  EXPECT_FALSE(callback_run);
  layer.reset();
  EXPECT_TRUE(callback_run);
}

// L1
//  +-- L2
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Simple) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  DrawTree(l1.get());

  gfx::PointF point1_in_l2_coords(5, 5);
  Layer::ConvertPointToLayer(l2.get(), l1.get(), &point1_in_l2_coords);
  gfx::PointF point1_in_l1_coords(15, 15);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l2_coords);

  gfx::PointF point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l2.get(), &point2_in_l1_coords);
  gfx::PointF point2_in_l2_coords(-5, -5);
  EXPECT_EQ(point2_in_l2_coords, point2_in_l1_coords);
}

// L1
//  +-- L2
//       +-- L3
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Medium) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));
  std::unique_ptr<Layer> l3(
      CreateColorLayer(SK_ColorYELLOW, gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());
  DrawTree(l1.get());

  gfx::PointF point1_in_l3_coords(5, 5);
  Layer::ConvertPointToLayer(l3.get(), l1.get(), &point1_in_l3_coords);
  gfx::PointF point1_in_l1_coords(25, 25);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l3_coords);

  gfx::PointF point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l3.get(), &point2_in_l1_coords);
  gfx::PointF point2_in_l3_coords(-15, -15);
  EXPECT_EQ(point2_in_l3_coords, point2_in_l1_coords);
}

TEST_F(LayerWithRealCompositorTest, Delegate) {
  // This test makes sure that whenever paint happens at a layer, its layer
  // delegate gets the paint, which in this test update its color and
  // |color_index|.
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorBLACK, gfx::Rect(20, 20, 400, 400)));
  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  TestLayerDelegate delegate;
  l1->set_delegate(&delegate);
  delegate.set_layer_bounds(l1->bounds());
  delegate.AddColor(SK_ColorWHITE);
  delegate.AddColor(SK_ColorYELLOW);
  delegate.AddColor(SK_ColorGREEN);

  l1->SchedulePaint(gfx::Rect(0, 0, 400, 400));
  WaitForDraw();
  // Test that paint happened at layer delegate.
  EXPECT_EQ(1, delegate.color_index());

  l1->SchedulePaint(gfx::Rect(10, 10, 200, 200));
  WaitForDraw();
  // Test that paint happened at layer delegate.
  EXPECT_EQ(2, delegate.color_index());

  l1->SchedulePaint(gfx::Rect(5, 5, 50, 50));
  WaitForDraw();
  // Test that paint happened at layer delegate.
  EXPECT_EQ(0, delegate.color_index());
}

TEST_F(LayerWithRealCompositorTest, DrawTree) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));
  std::unique_ptr<Layer> l3(
      CreateColorLayer(SK_ColorYELLOW, gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  DrawTreeLayerDelegate d1(l1->bounds());
  l1->set_delegate(&d1);
  DrawTreeLayerDelegate d2(l2->bounds());
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3(l3->bounds());
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  WaitForDraw();
  EXPECT_FALSE(d1.painted());
  EXPECT_TRUE(d2.painted());
  EXPECT_FALSE(d3.painted());
}

// Tests that scheduling paint on a layer with a mask updates the mask.
TEST_F(LayerWithRealCompositorTest, SchedulePaintUpdatesMask) {
  std::unique_ptr<Layer> layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> mask_layer(CreateLayer(ui::LAYER_TEXTURED));
  mask_layer->SetBounds(gfx::Rect(layer->GetTargetBounds().size()));
  layer->SetMaskLayer(mask_layer.get());

  GetCompositor()->SetRootLayer(layer.get());
  WaitForDraw();

  DrawTreeLayerDelegate d1(layer->bounds());
  layer->set_delegate(&d1);
  DrawTreeLayerDelegate d2(mask_layer->bounds());
  mask_layer->set_delegate(&d2);

  layer->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  WaitForDraw();
  EXPECT_TRUE(d1.painted());
  EXPECT_TRUE(d2.painted());
}

// Tests no-texture Layers.
// Create this hierarchy:
// L1 - red
// +-- L2 - NO TEXTURE
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, HierarchyNoTexture) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(CreateNoTextureLayer(gfx::Rect(10, 10, 350, 350)));
  std::unique_ptr<Layer> l3(
      CreateColorLayer(SK_ColorYELLOW, gfx::Rect(5, 5, 25, 25)));
  std::unique_ptr<Layer> l4(
      CreateColorLayer(SK_ColorMAGENTA, gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  DrawTreeLayerDelegate d2(l2->bounds());
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3(l3->bounds());
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  l3->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  WaitForDraw();

  // |d2| should not have received a paint notification since it has no texture.
  EXPECT_FALSE(d2.painted());
  // |d3| should have received a paint notification.
  EXPECT_TRUE(d3.painted());
}

TEST_F(LayerWithDelegateTest, Cloning) {
  std::unique_ptr<Layer> layer(CreateLayer(LAYER_SOLID_COLOR));

  gfx::Transform transform;
  transform.Scale(2, 1);
  transform.Translate(10, 5);

  layer->SetTransform(transform);
  layer->SetColor(SK_ColorRED);
  layer->SetLayerInverted(true);
  layer->AddCacheRenderSurfaceRequest();
  layer->AddTrilinearFilteringRequest();

  auto clone = layer->Clone();

  // Cloning preserves layer state.
  EXPECT_EQ(transform, clone->GetTargetTransform());
  EXPECT_EQ(SK_ColorRED, clone->background_color());
  EXPECT_EQ(SK_ColorRED, clone->GetTargetColor());
  EXPECT_TRUE(clone->layer_inverted());
  // Cloning should not preserve cache_render_surface flag.
  EXPECT_NE(layer->cc_layer_for_testing()->cache_render_surface(),
            clone->cc_layer_for_testing()->cache_render_surface());
  // Cloning should not preserve trilinear_filtering flag.
  EXPECT_NE(layer->cc_layer_for_testing()->trilinear_filtering(),
            clone->cc_layer_for_testing()->trilinear_filtering());

  layer->SetTransform(gfx::Transform());
  layer->SetColor(SK_ColorGREEN);
  layer->SetLayerInverted(false);

  // The clone is an independent copy, so state changes do not propagate.
  EXPECT_EQ(transform, clone->GetTargetTransform());
  EXPECT_EQ(SK_ColorRED, clone->background_color());
  EXPECT_EQ(SK_ColorRED, clone->GetTargetColor());
  EXPECT_TRUE(clone->layer_inverted());

  constexpr SkColor kTransparent = SK_ColorTRANSPARENT;
  layer->SetColor(kTransparent);
  layer->SetFillsBoundsOpaquely(false);
  // Color and opaqueness targets should be preserved during cloning, even after
  // switching away from solid color content.
  layer->SwitchCCLayerForTest();

  clone = layer->Clone();

  // The clone is a copy of the latest state.
  EXPECT_TRUE(clone->GetTargetTransform().IsIdentity());
  EXPECT_EQ(kTransparent, clone->background_color());
  EXPECT_EQ(kTransparent, clone->GetTargetColor());
  EXPECT_FALSE(clone->layer_inverted());
  EXPECT_FALSE(clone->fills_bounds_opaquely());

  // A solid color layer with transparent color can be marked as opaque. The
  // clone should retain this state.
  layer.reset(CreateLayer(LAYER_SOLID_COLOR));
  layer->SetColor(kTransparent);
  layer->SetFillsBoundsOpaquely(true);

  clone = layer->Clone();
  EXPECT_TRUE(clone->GetTargetTransform().IsIdentity());
  EXPECT_EQ(kTransparent, clone->background_color());
  EXPECT_EQ(kTransparent, clone->GetTargetColor());
  EXPECT_FALSE(clone->layer_inverted());
  EXPECT_TRUE(clone->fills_bounds_opaquely());

  layer.reset(CreateLayer(LAYER_SOLID_COLOR));
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);
  layer->SetColor(SK_ColorRED);

  ScopedLayerAnimationSettings settings(layer->GetAnimator());
  layer->SetVisible(false);
  layer->SetOpacity(0.0f);
  layer->SetColor(SK_ColorGREEN);

  EXPECT_TRUE(layer->visible());
  EXPECT_EQ(1.0f, layer->opacity());
  EXPECT_EQ(SK_ColorRED, layer->background_color());

  clone = layer->Clone();

  // Cloning copies animation targets.
  EXPECT_FALSE(clone->visible());
  EXPECT_EQ(0.0f, clone->opacity());
  EXPECT_EQ(SK_ColorGREEN, clone->background_color());
}

TEST_F(LayerWithDelegateTest, Mirroring) {
  std::unique_ptr<Layer> root(CreateNoTextureLayer(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));

  const gfx::Rect bounds(0, 0, 50, 50);
  child->SetBounds(bounds);
  child->SetVisible(true);

  DrawTreeLayerDelegate delegate(child->bounds());
  child->set_delegate(&delegate);

  const auto mirror = child->Mirror();

  // Bounds and visibility are preserved.
  EXPECT_EQ(bounds, mirror->bounds());
  EXPECT_TRUE(mirror->visible());

  root->Add(child.get());
  root->Add(mirror.get());

  DrawTree(root.get());
  EXPECT_TRUE(delegate.painted());
  delegate.Reset();

  // Both layers should be clean.
  EXPECT_TRUE(child->damaged_region_for_testing().IsEmpty());
  EXPECT_TRUE(mirror->damaged_region_for_testing().IsEmpty());

  const gfx::Rect damaged_rect(10, 10, 20, 20);
  EXPECT_TRUE(child->SchedulePaint(damaged_rect));
  EXPECT_EQ(damaged_rect, child->damaged_region_for_testing().bounds());

  DrawTree(root.get());
  EXPECT_TRUE(delegate.painted());
  delegate.Reset();

  // Damage should be propagated to the mirror.
  EXPECT_EQ(damaged_rect, mirror->damaged_region_for_testing().bounds());
  EXPECT_TRUE(child->damaged_region_for_testing().IsEmpty());

  DrawTree(root.get());
  EXPECT_TRUE(delegate.painted());

  // Mirror should be clean.
  EXPECT_TRUE(mirror->damaged_region_for_testing().IsEmpty());

  // Bounds are not synchronized by default.
  const gfx::Rect new_bounds(10, 10, 10, 10);
  child->SetBounds(new_bounds);
  EXPECT_EQ(bounds, mirror->bounds());
  child->SetBounds(bounds);

  // Bounds should be synchronized if requested.
  child->set_sync_bounds(true);
  child->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, mirror->bounds());
}

// Tests for SurfaceLayer cloning and mirroring. This tests certain properties
// are preserved.
TEST_F(LayerWithDelegateTest, SurfaceLayerCloneAndMirror) {
  const viz::FrameSinkId arbitrary_frame_sink(1, 1);
  viz::ParentLocalSurfaceIdAllocator allocator;
  std::unique_ptr<Layer> layer(CreateLayer(LAYER_SOLID_COLOR));

  viz::LocalSurfaceId local_surface_id = allocator.GenerateId();
  viz::SurfaceId surface_id_one(arbitrary_frame_sink, local_surface_id);
  layer->SetShowSurface(surface_id_one, gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  EXPECT_FALSE(layer->StretchContentToFillBounds());

  auto clone = layer->Clone();
  EXPECT_FALSE(clone->StretchContentToFillBounds());
  auto mirror = layer->Mirror();
  EXPECT_FALSE(mirror->StretchContentToFillBounds());

  local_surface_id = allocator.GenerateId();
  viz::SurfaceId surface_id_two(arbitrary_frame_sink, local_surface_id);
  layer->SetShowSurface(surface_id_two, gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), true);
  EXPECT_TRUE(layer->StretchContentToFillBounds());

  clone = layer->Clone();
  EXPECT_TRUE(clone->StretchContentToFillBounds());
  mirror = layer->Mirror();
  EXPECT_TRUE(mirror->StretchContentToFillBounds());
}

class LayerWithNullDelegateTest : public LayerWithDelegateTest {
 public:
  LayerWithNullDelegateTest() {}
  ~LayerWithNullDelegateTest() override {}

  void SetUp() override {
    LayerWithDelegateTest::SetUp();
    default_layer_delegate_.reset(new NullLayerDelegate());
  }

  Layer* CreateLayer(LayerType type) override {
    Layer* layer = new Layer(type);
    layer->set_delegate(default_layer_delegate_.get());
    return layer;
  }

  Layer* CreateTextureRootLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateTextureLayer(bounds);
    compositor()->SetRootLayer(layer);
    return layer;
  }

  Layer* CreateTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_TEXTURED);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) override {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

  gfx::Rect LastInvalidation() const {
    return default_layer_delegate_->invalidation();
  }

 private:
  std::unique_ptr<NullLayerDelegate> default_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithNullDelegateTest);
};

TEST_F(LayerWithNullDelegateTest, EscapedDebugNames) {
  std::unique_ptr<Layer> layer(CreateLayer(LAYER_NOT_DRAWN));
  std::string name = "\"\'\\/\b\f\n\r\t\n";
  layer->set_name(name);
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info(
      layer->TakeDebugInfo(layer->cc_layer_for_testing()));
  EXPECT_TRUE(debug_info.get());
  std::string json;
  debug_info->AppendAsTraceFormat(&json);
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> debug_info_value(json_reader.ReadToValue(json));
  EXPECT_TRUE(debug_info_value);
  EXPECT_TRUE(debug_info_value->is_dict());
  base::DictionaryValue* dictionary = 0;
  EXPECT_TRUE(debug_info_value->GetAsDictionary(&dictionary));
  std::string roundtrip;
  EXPECT_TRUE(dictionary->GetString("layer_name", &roundtrip));
  EXPECT_EQ(name, roundtrip);
}

TEST_F(LayerWithNullDelegateTest, SwitchLayerPreservesCCLayerState) {
  std::unique_ptr<Layer> l1(CreateLayer(LAYER_SOLID_COLOR));
  l1->SetFillsBoundsOpaquely(true);
  l1->SetVisible(false);
  l1->SetBounds(gfx::Rect(4, 5));

  EXPECT_EQ(gfx::Point3F(), l1->cc_layer_for_testing()->transform_origin());
  EXPECT_TRUE(l1->cc_layer_for_testing()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer_for_testing()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_EQ(gfx::Size(4, 5), l1->cc_layer_for_testing()->bounds());

  cc::Layer* before_layer = l1->cc_layer_for_testing();

  bool callback1_run = false;
  auto resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());
  l1->SetTransferableResource(resource,
                              viz::SingleReleaseCallback::Create(base::BindOnce(
                                  ReturnMailbox, &callback1_run)),
                              gfx::Size(10, 10));

  EXPECT_NE(before_layer, l1->cc_layer_for_testing());

  EXPECT_EQ(gfx::Point3F(), l1->cc_layer_for_testing()->transform_origin());
  EXPECT_TRUE(l1->cc_layer_for_testing()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer_for_testing()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_EQ(gfx::Size(4, 5), l1->cc_layer_for_testing()->bounds());
  EXPECT_FALSE(callback1_run);

  bool callback2_run = false;
  resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());
  l1->SetTransferableResource(resource,
                              viz::SingleReleaseCallback::Create(base::BindOnce(
                                  ReturnMailbox, &callback2_run)),
                              gfx::Size(10, 10));
  EXPECT_TRUE(callback1_run);
  EXPECT_FALSE(callback2_run);

  // Show solid color instead.
  l1->SetShowSolidColorContent();
  EXPECT_EQ(gfx::Point3F(), l1->cc_layer_for_testing()->transform_origin());
  EXPECT_TRUE(l1->cc_layer_for_testing()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer_for_testing()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_EQ(gfx::Size(4, 5), l1->cc_layer_for_testing()->bounds());
  EXPECT_TRUE(callback2_run);

  before_layer = l1->cc_layer_for_testing();

  // Back to a texture, without changing the bounds of the layer or the texture.
  bool callback3_run = false;
  resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());
  l1->SetTransferableResource(resource,
                              viz::SingleReleaseCallback::Create(base::BindOnce(
                                  ReturnMailbox, &callback3_run)),
                              gfx::Size(10, 10));

  EXPECT_NE(before_layer, l1->cc_layer_for_testing());

  EXPECT_EQ(gfx::Point3F(), l1->cc_layer_for_testing()->transform_origin());
  EXPECT_TRUE(l1->cc_layer_for_testing()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer_for_testing()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_EQ(gfx::Size(4, 5), l1->cc_layer_for_testing()->bounds());
  EXPECT_FALSE(callback3_run);

  // Release the on |l1| mailbox to clean up the test.
  l1->SetShowSolidColorContent();
}

// Various visible/drawn assertions.
TEST_F(LayerWithNullDelegateTest, Visibility) {
  std::unique_ptr<Layer> l1(new Layer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l2(new Layer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l3(new Layer(LAYER_TEXTURED));
  l1->Add(l2.get());
  l2->Add(l3.get());

  NullLayerDelegate delegate;
  l1->set_delegate(&delegate);
  l2->set_delegate(&delegate);
  l3->set_delegate(&delegate);

  // Layers should initially be drawn.
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_TRUE(l3->IsDrawn());
  EXPECT_FALSE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l3->cc_layer_for_testing()->hide_layer_and_subtree());

  compositor()->SetRootLayer(l1.get());

  Draw();

  l1->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l3->cc_layer_for_testing()->hide_layer_and_subtree());

  l3->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_TRUE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_TRUE(l3->cc_layer_for_testing()->hide_layer_and_subtree());

  l1->SetVisible(true);
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_FALSE(l1->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer_for_testing()->hide_layer_and_subtree());
  EXPECT_TRUE(l3->cc_layer_for_testing()->hide_layer_and_subtree());
}

// Checks that stacking-related methods behave as advertised.
TEST_F(LayerWithNullDelegateTest, Stacking) {
  std::unique_ptr<Layer> root(new Layer(LAYER_NOT_DRAWN));
  std::unique_ptr<Layer> l1(new Layer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l2(new Layer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l3(new Layer(LAYER_TEXTURED));
  l1->set_name("1");
  l2->set_name("2");
  l3->set_name("3");
  root->Add(l3.get());
  root->Add(l2.get());
  root->Add(l1.get());

  // Layers' children are stored in bottom-to-top order.
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l3.get());
  EXPECT_EQ("2 1 3", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l2.get(), l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l1.get(), l3.get());
  EXPECT_EQ("3 1 2", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l2.get(), l1.get());
  EXPECT_EQ("3 1 2", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l2.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l2.get(), l3.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l1.get(), l3.get());
  EXPECT_EQ("2 1 3", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l2.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l2.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));
}

// Verifies SetBounds triggers the appropriate painting/drawing.
TEST_F(LayerWithNullDelegateTest, SetBoundsSchedulesPaint) {
  std::unique_ptr<Layer> l1(CreateTextureLayer(gfx::Rect(0, 0, 200, 200)));
  compositor()->SetRootLayer(l1.get());

  Draw();

  l1->SetBounds(gfx::Rect(5, 5, 200, 200));

  // The CompositorDelegate (us) should have been told to draw for a move.
  WaitForDraw();

  l1->SetBounds(gfx::Rect(5, 5, 100, 100));

  // The CompositorDelegate (us) should have been told to draw for a resize.
  WaitForDraw();
}

// Checks that the damage rect for a TextureLayer is empty after a commit.
TEST_F(LayerWithNullDelegateTest, EmptyDamagedRect) {
  base::RunLoop run_loop;
  viz::ReleaseCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop, const gpu::SyncToken& sync_token,
         bool is_lost) { run_loop->Quit(); },
      base::Unretained(&run_loop));

  std::unique_ptr<Layer> root(CreateLayer(LAYER_SOLID_COLOR));
  auto resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());
  root->SetTransferableResource(
      resource, viz::SingleReleaseCallback::Create(std::move(callback)),
      gfx::Size(10, 10));
  compositor()->SetRootLayer(root.get());

  root->SetBounds(gfx::Rect(0, 0, 10, 10));
  root->SetVisible(true);
  WaitForCommit();

  gfx::Rect damaged_rect(0, 0, 5, 5);
  root->SchedulePaint(damaged_rect);
  EXPECT_EQ(damaged_rect, root->damaged_region_for_testing().bounds());
  WaitForCommit();
  EXPECT_TRUE(root->damaged_region_for_testing().IsEmpty());

  // The texture mailbox has a reference from an in-flight texture layer.
  // We clear the texture mailbox from the root layer and draw a new frame
  // to ensure that the texture mailbox is released.
  root->SetShowSolidColorContent();
  Draw();

  // Wait for texture mailbox release to avoid DCHECKs.
  run_loop.Run();
}

// Tests that in deferred paint request, the layer damage will be accumulated.
TEST_F(LayerWithNullDelegateTest, UpdateDamageInDeferredPaint) {
  gfx::Rect bound(gfx::Rect(500, 500));
  std::unique_ptr<Layer> root(CreateTextureRootLayer(bound));
  EXPECT_EQ(bound, root->damaged_region_for_testing());
  WaitForCommit();
  EXPECT_EQ(gfx::Rect(), root->damaged_region_for_testing());
  EXPECT_EQ(bound, LastInvalidation());

  // Deferring paint.
  root->AddDeferredPaintRequest();

  // During deferring paint request, invalid_rect will not be set to
  // cc_layer_->inputs_->update_rect, and the paint_region is empty.
  gfx::Rect bound1(gfx::Rect(100, 100));
  root->SchedulePaint(bound1);
  EXPECT_EQ(bound1, root->damaged_region_for_testing());
  root->SendDamagedRects();
  EXPECT_EQ(gfx::Rect(), root->cc_layer_for_testing()->update_rect());
  root->PaintContentsToDisplayList(
      cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  EXPECT_EQ(gfx::Rect(), LastInvalidation());

  // During deferring paint request, a new invalid_rect will be accumulated.
  gfx::Rect bound2(gfx::Rect(100, 200, 100, 100));
  gfx::Rect bound_union(bound1);
  bound_union.Union(bound2);
  root->SchedulePaint(bound2);
  EXPECT_EQ(bound_union, root->damaged_region_for_testing().bounds());
  root->SendDamagedRects();
  EXPECT_EQ(gfx::Rect(), root->cc_layer_for_testing()->update_rect());
  root->PaintContentsToDisplayList(
      cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  EXPECT_EQ(gfx::Rect(), LastInvalidation());

  // Remove deferring paint request.
  root->RemoveDeferredPaintRequest();

  // The invalidation region should be accumulated invalid_rect during deferred
  // paint, i.e. union of bound1 and bound2.
  root->SendDamagedRects();
  EXPECT_EQ(bound_union, root->cc_layer_for_testing()->update_rect());
  root->PaintContentsToDisplayList(
      cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  EXPECT_EQ(bound_union, LastInvalidation());
}

void ExpectRgba(int x, int y, SkColor expected_color, SkColor actual_color) {
  EXPECT_EQ(expected_color, actual_color)
      << "Pixel error at x=" << x << " y=" << y << "; "
      << "actual RGBA=("
      << SkColorGetR(actual_color) << ","
      << SkColorGetG(actual_color) << ","
      << SkColorGetB(actual_color) << ","
      << SkColorGetA(actual_color) << "); "
      << "expected RGBA=("
      << SkColorGetR(expected_color) << ","
      << SkColorGetG(expected_color) << ","
      << SkColorGetB(expected_color) << ","
      << SkColorGetA(expected_color) << ")";
}

// Checks that pixels are actually drawn to the screen with a read back.
TEST_F(LayerWithRealCompositorTest, DrawPixels) {
  gfx::Size viewport_size = GetCompositor()->size();

  // The window should be some non-trivial size but may not be exactly
  // 500x500 on all platforms/bots.
  EXPECT_GE(viewport_size.width(), 200);
  EXPECT_GE(viewport_size.height(), 200);

  int blue_height = 10;

  std::unique_ptr<Layer> layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(viewport_size)));
  std::unique_ptr<Layer> layer2(CreateColorLayer(
      SK_ColorBLUE, gfx::Rect(0, 0, viewport_size.width(), blue_height)));

  layer->Add(layer2.get());

  DrawTree(layer.get());

  SkBitmap bitmap;
  ReadPixels(&bitmap, gfx::Rect(viewport_size));
  ASSERT_FALSE(bitmap.empty());

  for (int x = 0; x < viewport_size.width(); x++) {
    for (int y = 0; y < viewport_size.height(); y++) {
      SkColor actual_color = bitmap.getColor(x, y);
      SkColor expected_color = y < blue_height ? SK_ColorBLUE : SK_ColorRED;
      ExpectRgba(x, y, expected_color, actual_color);
    }
  }
}

// Checks that drawing a layer with transparent pixels is blended correctly
// with the lower layer.
TEST_F(LayerWithRealCompositorTest, DrawAlphaBlendedPixels) {
  gfx::Size viewport_size = GetCompositor()->size();

  int test_size = 200;
  EXPECT_GE(viewport_size.width(), test_size);
  EXPECT_GE(viewport_size.height(), test_size);

  // Blue with a wee bit of transparency.
  SkColor blue_with_alpha = SkColorSetARGB(40, 10, 20, 200);
  SkColor blend_color = SkColorSetARGB(255, 216, 3, 32);

  std::unique_ptr<Layer> background_layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(viewport_size)));
  std::unique_ptr<Layer> foreground_layer(
      CreateColorLayer(blue_with_alpha, gfx::Rect(viewport_size)));

  // This must be set to false for layers with alpha to be blended correctly.
  foreground_layer->SetFillsBoundsOpaquely(false);

  background_layer->Add(foreground_layer.get());
  DrawTree(background_layer.get());

  SkBitmap bitmap;
  ReadPixels(&bitmap, gfx::Rect(viewport_size));
  ASSERT_FALSE(bitmap.empty());

  for (int x = 0; x < test_size; x++) {
    for (int y = 0; y < test_size; y++) {
      SkColor actual_color = bitmap.getColor(x, y);
      ExpectRgba(x, y, blend_color, actual_color);
    }
  }
}

// Checks that using the AlphaShape filter applied to a layer with
// transparency, alpha-blends properly with the layer below.
TEST_F(LayerWithRealCompositorTest, DrawAlphaThresholdFilterPixels) {
  gfx::Size viewport_size = GetCompositor()->size();

  int test_size = 200;
  EXPECT_GE(viewport_size.width(), test_size);
  EXPECT_GE(viewport_size.height(), test_size);

  int blue_height = 10;
  SkColor blue_with_alpha = SkColorSetARGB(40, 0, 0, 255);
  SkColor blend_color = SkColorSetARGB(255, 215, 0, 40);

  std::unique_ptr<Layer> background_layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(viewport_size)));
  std::unique_ptr<Layer> foreground_layer(
      CreateColorLayer(blue_with_alpha, gfx::Rect(viewport_size)));

  // Add a shape to restrict the visible part of the layer.
  auto shape = std::make_unique<Layer::ShapeRects>();
  shape->emplace_back(0, 0, viewport_size.width(), blue_height);
  foreground_layer->SetAlphaShape(std::move(shape));

  foreground_layer->SetFillsBoundsOpaquely(false);

  background_layer->Add(foreground_layer.get());
  DrawTree(background_layer.get());

  SkBitmap bitmap;
  ReadPixels(&bitmap, gfx::Rect(viewport_size));
  ASSERT_FALSE(bitmap.empty());

  for (int x = 0; x < test_size; x++) {
    for (int y = 0; y < test_size; y++) {
      SkColor actual_color = bitmap.getColor(x, y);
      ExpectRgba(x, y, actual_color,
                 y < blue_height ? blend_color : SK_ColorRED);
    }
  }
}

// Checks the logic around Compositor::SetRootLayer and Layer::SetCompositor.
TEST_F(LayerWithRealCompositorTest, SetRootLayer) {
  Compositor* compositor = GetCompositor();
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));

  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());

  compositor->SetRootLayer(l1.get());
  EXPECT_EQ(compositor, l1->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  l1->Remove(l2.get());
  EXPECT_EQ(NULL, l2->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  compositor->SetRootLayer(NULL);
  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());
}

// Checks that compositor observers are notified when:
// - DrawTree is called,
// - After ScheduleDraw is called, or
// - Whenever SetBounds, SetOpacity or SetTransform are called.
// TODO(vollick): could be reorganized into compositor_unittest.cc
// Flaky on Windows. See https://crbug.com/784563.
// Flaky on Linux tsan. See https://crbug.com/834026.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_CompositorObservers DISABLED_CompositorObservers
#else
#define MAYBE_CompositorObservers CompositorObservers
#endif
TEST_F(LayerWithRealCompositorTest, MAYBE_CompositorObservers) {
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorRED, gfx::Rect(20, 20, 400, 400)));
  std::unique_ptr<Layer> l2(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  TestCompositorObserver observer;
  GetCompositor()->AddObserver(&observer);

  // Explicitly called DrawTree should cause the observers to be notified.
  // NOTE: this call to DrawTree sets l1 to be the compositor's root layer.
  DrawTree(l1.get());
  EXPECT_TRUE(observer.notified());

  // ScheduleDraw without any visible change should cause a commit.
  observer.Reset();
  l1->ScheduleDraw();
  WaitForCommit();
  EXPECT_TRUE(observer.committed());

  // Moving, but not resizing, a layer should alert the observers.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 350, 350));
  WaitForSwap();
  EXPECT_TRUE(observer.notified());

  // So should resizing a layer.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 400, 400));
  WaitForSwap();
  EXPECT_TRUE(observer.notified());

  // Opacity changes should alert the observers.
  observer.Reset();
  l2->SetOpacity(0.5f);
  WaitForSwap();
  EXPECT_TRUE(observer.notified());

  // So should setting the opacity back.
  observer.Reset();
  l2->SetOpacity(1.0f);
  WaitForSwap();
  EXPECT_TRUE(observer.notified());

  // Setting the transform of a layer should alert the observers.
  observer.Reset();
  gfx::Transform transform;
  transform.Translate(200.0, 200.0);
  transform.Rotate(90.0);
  transform.Translate(-200.0, -200.0);
  l2->SetTransform(transform);
  WaitForSwap();
  EXPECT_TRUE(observer.notified());

  GetCompositor()->RemoveObserver(&observer);

  // Opacity changes should no longer alert the removed observer.
  observer.Reset();
  l2->SetOpacity(0.5f);
  WaitForSwap();

  EXPECT_FALSE(observer.notified());
}

// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, ModifyHierarchy) {
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(50, 50),
                                   viz::LocalSurfaceId(), base::TimeTicks());

  // l0
  //  +-l11
  //  | +-l21
  //  +-l12
  std::unique_ptr<Layer> l0(
      CreateColorLayer(SK_ColorRED, gfx::Rect(0, 0, 50, 50)));
  std::unique_ptr<Layer> l11(
      CreateColorLayer(SK_ColorGREEN, gfx::Rect(0, 0, 25, 25)));
  std::unique_ptr<Layer> l21(
      CreateColorLayer(SK_ColorMAGENTA, gfx::Rect(0, 0, 15, 15)));
  std::unique_ptr<Layer> l12(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(10, 10, 25, 25)));

  base::FilePath ref_img1 =
      test_data_directory().AppendASCII("ModifyHierarchy1.png");
  base::FilePath ref_img2 =
      test_data_directory().AppendASCII("ModifyHierarchy2.png");
  SkBitmap bitmap;

  l0->Add(l11.get());
  l11->Add(l21.get());
  l0->Add(l12.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img1);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img2);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));

  // should restore to original configuration
  l0->StackAbove(l12.get(), l11.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  // l11 back to front
  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));

  // should restore to original configuration
  l0->StackAbove(l12.get(), l11.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  // l11 back to front
  l0->StackAbove(l11.get(), l12.get());
  DrawTree(l0.get());
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));
}

// It is really hard to write pixel test on text rendering,
// due to different font appearance.
// So we choose to check result only on Windows.
// See https://codereview.chromium.org/1634103003/#msg41
#if defined(OS_WIN)
TEST_F(LayerWithRealCompositorTest, CanvasDrawFadedString) {
  gfx::Size size(50, 50);
  GetCompositor()->SetScaleAndSize(1.0f, size, viz::LocalSurfaceId(),
                                   base::TimeTicks());
  DrawFadedStringLayerDelegate delegate(SK_ColorBLUE, size);
  std::unique_ptr<Layer> layer(
      CreateDrawFadedStringLayerDelegate(gfx::Rect(size), &delegate));
  DrawTree(layer.get());

  SkBitmap bitmap;
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());

  base::FilePath ref_img =
      test_data_directory().AppendASCII("string_faded.png");
  // WritePNGFile(bitmap, ref_img, true);

  float percentage_pixels_large_error = 8.0f;  // 200px / (50*50)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 80.f;
  int large_error_allowed = 255;
  int small_error_allowed = 0;

  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img,
                             cc::FuzzyPixelComparator(
                                 true,
                                 percentage_pixels_large_error,
                                 percentage_pixels_small_error,
                                 average_error_allowed_in_bad_pixels,
                                 large_error_allowed,
                                 small_error_allowed)));
}
#endif  // defined(OS_WIN)

// Opacity is rendered correctly.
// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, Opacity) {
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(50, 50),
                                   viz::LocalSurfaceId(), base::TimeTicks());

  // l0
  //  +-l11
  std::unique_ptr<Layer> l0(
      CreateColorLayer(SK_ColorRED, gfx::Rect(0, 0, 50, 50)));
  std::unique_ptr<Layer> l11(
      CreateColorLayer(SK_ColorGREEN, gfx::Rect(0, 0, 25, 25)));

  base::FilePath ref_img = test_data_directory().AppendASCII("Opacity.png");

  l11->SetOpacity(0.75);
  l0->Add(l11.get());
  DrawTree(l0.get());
  SkBitmap bitmap;
  ReadPixels(&bitmap);
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img, cc::ExactPixelComparator(true)));
}

namespace {

class SchedulePaintLayerDelegate : public LayerDelegate {
 public:
  SchedulePaintLayerDelegate() : paint_count_(0), layer_(NULL) {}

  ~SchedulePaintLayerDelegate() override {}

  void set_layer(Layer* layer) {
    layer_ = layer;
    layer_->set_delegate(this);
  }

  void SetSchedulePaintRect(const gfx::Rect& rect) {
    schedule_paint_rect_ = rect;
  }

  int GetPaintCountAndClear() {
    int value = paint_count_;
    paint_count_ = 0;
    return value;
  }

  const gfx::Rect& last_clip_rect() const { return last_clip_rect_; }

 private:
  // Overridden from LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    paint_count_++;
    if (!schedule_paint_rect_.IsEmpty()) {
      layer_->SchedulePaint(schedule_paint_rect_);
      schedule_paint_rect_ = gfx::Rect();
    }
    last_clip_rect_ = context.InvalidationForTesting();
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  int paint_count_;
  Layer* layer_;
  gfx::Rect schedule_paint_rect_;
  gfx::Rect last_clip_rect_;

  DISALLOW_COPY_AND_ASSIGN(SchedulePaintLayerDelegate);
};

}  // namespace

// Verifies that if SchedulePaint is invoked during painting the layer is still
// marked dirty.
TEST_F(LayerWithDelegateTest, SchedulePaintFromOnPaintLayer) {
  std::unique_ptr<Layer> root(
      CreateColorLayer(SK_ColorRED, gfx::Rect(0, 0, 500, 500)));
  SchedulePaintLayerDelegate child_delegate;
  std::unique_ptr<Layer> child(
      CreateColorLayer(SK_ColorBLUE, gfx::Rect(0, 0, 200, 200)));
  child_delegate.set_layer(child.get());

  root->Add(child.get());

  SchedulePaintForLayer(root.get());
  DrawTree(root.get());
  child->SchedulePaint(gfx::Rect(0, 0, 20, 20));
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());

  // Set a rect so that when OnPaintLayer() is invoked SchedulePaint is invoked
  // again.
  child_delegate.SetSchedulePaintRect(gfx::Rect(10, 10, 30, 30));
  WaitForCommit();
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());

  // Because SchedulePaint() was invoked from OnPaintLayer() |child| should
  // still need to be painted.
  WaitForCommit();
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());
  EXPECT_TRUE(child_delegate.last_clip_rect().Contains(
                  gfx::Rect(10, 10, 30, 30)));
}

TEST_F(LayerWithRealCompositorTest, ScaleUpDown) {
  std::unique_ptr<Layer> root(
      CreateColorLayer(SK_ColorWHITE, gfx::Rect(10, 20, 200, 220)));
  TestLayerDelegate root_delegate;
  root_delegate.AddColor(SK_ColorWHITE);
  root->set_delegate(&root_delegate);
  root_delegate.set_layer_bounds(root->bounds());

  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorWHITE, gfx::Rect(10, 20, 140, 180)));
  TestLayerDelegate l1_delegate;
  l1_delegate.AddColor(SK_ColorWHITE);
  l1->set_delegate(&l1_delegate);
  l1_delegate.set_layer_bounds(l1->bounds());

  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());
  WaitForDraw();

  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  gfx::Size cc_bounds_size = root->cc_layer_for_testing()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // No scale change, so no scale notification.
  EXPECT_EQ(0.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());

  // Scale up to 2.0. Changing scale doesn't change the bounds in DIP.
  GetCompositor()->SetScaleAndSize(2.0f, gfx::Size(500, 500),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  // CC layer should still match the UI layer bounds.
  cc_bounds_size = root->cc_layer_for_testing()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // New scale factor must have been notified. Make sure painting happens at
  // right scale.
  EXPECT_EQ(2.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(2.0f, l1_delegate.device_scale_factor());

  // Scale down back to 1.0f.
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  // CC layer should still match the UI layer bounds.
  cc_bounds_size = root->cc_layer_for_testing()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // New scale factor must have been notified. Make sure painting happens at
  // right scale.
  EXPECT_EQ(1.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(1.0f, l1_delegate.device_scale_factor());

  root_delegate.reset();
  l1_delegate.reset();
  // Just changing the size shouldn't notify the scale change nor
  // trigger repaint.
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(1000, 1000),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  // No scale change, so no scale notification.
  EXPECT_EQ(0.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());
}

TEST_F(LayerWithRealCompositorTest, ScaleReparent) {
  std::unique_ptr<Layer> root(
      CreateColorLayer(SK_ColorWHITE, gfx::Rect(10, 20, 200, 220)));
  std::unique_ptr<Layer> l1(
      CreateColorLayer(SK_ColorWHITE, gfx::Rect(10, 20, 140, 180)));
  TestLayerDelegate l1_delegate;
  l1_delegate.AddColor(SK_ColorWHITE);
  l1->set_delegate(&l1_delegate);
  l1_delegate.set_layer_bounds(l1->bounds());

  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  GetCompositor()->SetRootLayer(root.get());

  root->Add(l1.get());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  gfx::Size cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());

  // Remove l1 from root and change the scale.
  root->Remove(l1.get());
  EXPECT_EQ(NULL, l1->parent());
  EXPECT_EQ(NULL, l1->GetCompositor());
  GetCompositor()->SetScaleAndSize(2.0f, gfx::Size(500, 500),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  // Sanity check on root and l1.
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());

  root->Add(l1.get());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  cc_bounds_size = l1->cc_layer_for_testing()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  EXPECT_EQ(2.0f, l1_delegate.device_scale_factor());
}

// Verifies that when changing bounds on a layer that is invisible, and then
// made visible, the right thing happens:
// - if just a move, then no painting should happen.
// - if a resize, the layer should be repainted.
TEST_F(LayerWithDelegateTest, SetBoundsWhenInvisible) {
  std::unique_ptr<Layer> root(
      CreateNoTextureLayer(gfx::Rect(0, 0, 1000, 1000)));

  std::unique_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  child->SetBounds(gfx::Rect(0, 0, 500, 500));
  DrawTreeLayerDelegate delegate(child->bounds());
  child->set_delegate(&delegate);
  root->Add(child.get());

  // Paint once for initial damage.
  child->SetVisible(true);
  DrawTree(root.get());

  // Reset into invisible state.
  child->SetVisible(false);
  DrawTree(root.get());
  delegate.Reset();

  // Move layer.
  child->SetBounds(gfx::Rect(200, 200, 500, 500));
  child->SetVisible(true);
  DrawTree(root.get());
  EXPECT_FALSE(delegate.painted());

  // Reset into invisible state.
  child->SetVisible(false);
  DrawTree(root.get());
  delegate.Reset();

  // Resize layer.
  child->SetBounds(gfx::Rect(200, 200, 400, 400));
  child->SetVisible(true);
  DrawTree(root.get());
  EXPECT_TRUE(delegate.painted());
}

TEST_F(LayerWithDelegateTest, ExternalContent) {
  std::unique_ptr<Layer> root(
      CreateNoTextureLayer(gfx::Rect(0, 0, 1000, 1000)));
  std::unique_ptr<Layer> child(CreateLayer(LAYER_SOLID_COLOR));

  child->SetBounds(gfx::Rect(0, 0, 10, 10));
  child->SetVisible(true);
  root->Add(child.get());

  // The layer is already showing solid color content, so the cc layer won't
  // change.
  scoped_refptr<cc::Layer> before = child->cc_layer_for_testing();

  child->SetShowSolidColorContent();
  EXPECT_TRUE(child->cc_layer_for_testing());
  EXPECT_EQ(before.get(), child->cc_layer_for_testing());

  // Showing surface content changes the underlying cc layer.
  viz::FrameSinkId frame_sink_id(1u, 1u);
  viz::ParentLocalSurfaceIdAllocator allocator;
  before = child->cc_layer_for_testing();
  child->SetShowSurface(viz::SurfaceId(frame_sink_id, allocator.GenerateId()),
                        gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  scoped_refptr<cc::Layer> after = child->cc_layer_for_testing();
  const auto* surface = static_cast<cc::SurfaceLayer*>(after.get());
  EXPECT_TRUE(after.get());
  EXPECT_NE(before.get(), after.get());
  EXPECT_EQ(base::nullopt, surface->deadline_in_frames());

  child->SetShowSurface(viz::SurfaceId(frame_sink_id, allocator.GenerateId()),
                        gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseSpecifiedDeadline(4u), false);
  EXPECT_EQ(4u, surface->deadline_in_frames());
}

TEST_F(LayerWithDelegateTest, ExternalContentMirroring) {
  std::unique_ptr<Layer> layer(CreateLayer(LAYER_SOLID_COLOR));

  viz::SurfaceId surface_id(
      viz::FrameSinkId(0, 1),
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));
  layer->SetShowSurface(surface_id, gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);

  const auto mirror = layer->Mirror();
  auto* const cc_layer = mirror->cc_layer_for_testing();
  const auto* surface = static_cast<cc::SurfaceLayer*>(cc_layer);

  // Mirroring preserves surface state.
  EXPECT_EQ(surface_id, surface->surface_id());

  surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 2),
                     viz::LocalSurfaceId(3, base::UnguessableToken::Create()));
  layer->SetShowSurface(surface_id, gfx::Size(20, 20), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);

  // The mirror should continue to use the same cc_layer.
  EXPECT_EQ(cc_layer, mirror->cc_layer_for_testing());
  layer->SetShowSurface(surface_id, gfx::Size(20, 20), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);

  // Surface updates propagate to the mirror.
  EXPECT_EQ(surface_id, surface->surface_id());
}

// Verifies that layer filters still attached after changing implementation
// layer.
TEST_F(LayerWithDelegateTest, LayerFiltersSurvival) {
  std::unique_ptr<Layer> layer(CreateLayer(LAYER_TEXTURED));
  layer->SetBounds(gfx::Rect(0, 0, 10, 10));
  EXPECT_TRUE(layer->cc_layer_for_testing());
  EXPECT_EQ(0u, layer->cc_layer_for_testing()->filters().size());

  layer->SetLayerGrayscale(0.5f);
  EXPECT_EQ(layer->layer_grayscale(), 0.5f);
  EXPECT_EQ(1u, layer->cc_layer_for_testing()->filters().size());

  // Showing surface content changes the underlying cc layer.
  scoped_refptr<cc::Layer> before = layer->cc_layer_for_testing();
  layer->SetShowSurface(viz::SurfaceId(), gfx::Size(10, 10), SK_ColorWHITE,
                        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  EXPECT_EQ(layer->layer_grayscale(), 0.5f);
  EXPECT_TRUE(layer->cc_layer_for_testing());
  EXPECT_NE(before.get(), layer->cc_layer_for_testing());
  EXPECT_EQ(1u, layer->cc_layer_for_testing()->filters().size());
}

// Tests Layer::AddThreadedAnimation and Layer::RemoveThreadedAnimation.
TEST_F(LayerWithRealCompositorTest, AddRemoveThreadedAnimations) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l2(CreateLayer(LAYER_TEXTURED));

  l1->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  l2->SetAnimator(LayerAnimator::CreateImplicitAnimator());

  auto* animation1 = l1->GetAnimator()->GetAnimationForTesting();
  auto* animation2 = l2->GetAnimator()->GetAnimationForTesting();

  EXPECT_FALSE(animation1->keyframe_effect()->has_any_keyframe_model());

  // Trigger a threaded animation.
  l1->SetOpacity(0.5f);

  EXPECT_TRUE(animation1->keyframe_effect()->has_any_keyframe_model());

  // Ensure we can remove a pending threaded animation.
  l1->GetAnimator()->StopAnimating();

  EXPECT_FALSE(animation1->keyframe_effect()->has_any_keyframe_model());

  // Trigger another threaded animation.
  l1->SetOpacity(0.2f);

  EXPECT_TRUE(animation1->keyframe_effect()->has_any_keyframe_model());

  root->Add(l1.get());
  GetCompositor()->SetRootLayer(root.get());

  // Now l1 is part of a tree.
  EXPECT_TRUE(animation1->keyframe_effect()->has_any_keyframe_model());

  l1->SetOpacity(0.1f);
  // IMMEDIATELY_SET_NEW_TARGET is a default preemption strategy for conflicting
  // animations.
  EXPECT_FALSE(animation1->keyframe_effect()->has_any_keyframe_model());

  // Adding a layer to an existing tree.
  l2->SetOpacity(0.5f);
  EXPECT_TRUE(animation2->keyframe_effect()->has_any_keyframe_model());

  l1->Add(l2.get());
  EXPECT_TRUE(animation2->keyframe_effect()->has_any_keyframe_model());
}

// Tests that in-progress threaded animations complete when a Layer's
// cc::Layer changes.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerAnimations) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());

  l1->SetAnimator(LayerAnimator::CreateImplicitAnimator());

  EXPECT_FLOAT_EQ(l1->opacity(), 1.0f);

  // Trigger a threaded animation.
  l1->SetOpacity(0.5f);

  // Change l1's cc::Layer.
  l1->SwitchCCLayerForTest();

  // Ensure that the opacity animation completed.
  EXPECT_FLOAT_EQ(l1->opacity(), 0.5f);
}

// Tests that when a LAYER_SOLID_COLOR has its CC layer switched, that
// opaqueness and color set while not animating, are maintained.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerSolidColorNotAnimating) {
  SkColor transparent = SK_ColorTRANSPARENT;
  std::unique_ptr<Layer> root(CreateLayer(LAYER_SOLID_COLOR));
  GetCompositor()->SetRootLayer(root.get());
  root->SetFillsBoundsOpaquely(false);
  root->SetColor(transparent);

  EXPECT_FALSE(root->fills_bounds_opaquely());
  EXPECT_FALSE(
      root->GetAnimator()->IsAnimatingProperty(LayerAnimationElement::COLOR));
  EXPECT_EQ(transparent, root->background_color());
  EXPECT_EQ(transparent, root->GetTargetColor());

  // Changing the underlying layer should not affect targets.
  root->SwitchCCLayerForTest();

  EXPECT_FALSE(root->fills_bounds_opaquely());
  EXPECT_FALSE(
      root->GetAnimator()->IsAnimatingProperty(LayerAnimationElement::COLOR));
  EXPECT_EQ(transparent, root->background_color());
  EXPECT_EQ(transparent, root->GetTargetColor());
}

// Tests that when a LAYER_SOLID_COLOR has its CC layer switched during an
// animation of its opaquness and color, that both the current values, and the
// targets are maintained.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerSolidColorWhileAnimating) {
  SkColor transparent = SK_ColorTRANSPARENT;
  std::unique_ptr<Layer> root(CreateLayer(LAYER_SOLID_COLOR));
  GetCompositor()->SetRootLayer(root.get());
  root->SetColor(SK_ColorBLACK);

  EXPECT_TRUE(root->fills_bounds_opaquely());
  EXPECT_EQ(SK_ColorBLACK, root->GetTargetColor());

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> long_duration_animation(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  {
    ui::ScopedLayerAnimationSettings animation(root->GetAnimator());
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    root->SetFillsBoundsOpaquely(false);
    root->SetColor(transparent);
  }

  EXPECT_TRUE(root->fills_bounds_opaquely());
  EXPECT_TRUE(
      root->GetAnimator()->IsAnimatingProperty(LayerAnimationElement::COLOR));
  EXPECT_EQ(SK_ColorBLACK, root->background_color());
  EXPECT_EQ(transparent, root->GetTargetColor());

  // Changing the underlying layer should not affect targets.
  root->SwitchCCLayerForTest();

  EXPECT_TRUE(root->fills_bounds_opaquely());
  EXPECT_TRUE(
      root->GetAnimator()->IsAnimatingProperty(LayerAnimationElement::COLOR));
  EXPECT_EQ(SK_ColorBLACK, root->background_color());
  EXPECT_EQ(transparent, root->GetTargetColor());

  // End all animations.
  root->GetAnimator()->StopAnimating();
  EXPECT_FALSE(root->fills_bounds_opaquely());
  EXPECT_FALSE(
      root->GetAnimator()->IsAnimatingProperty(LayerAnimationElement::COLOR));
  EXPECT_EQ(transparent, root->background_color());
  EXPECT_EQ(transparent, root->GetTargetColor());
}

// Tests that when a layer with cache_render_surface flag has its CC layer
// switched, that the cache_render_surface flag is maintained.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerCacheRenderSurface) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());

  l1->AddCacheRenderSurfaceRequest();

  // Change l1's cc::Layer.
  l1->SwitchCCLayerForTest();

  // Ensure that the cache_render_surface flag is maintained.
  EXPECT_TRUE(l1->cc_layer_for_testing()->cache_render_surface());
}

// Tests that when a layer with trilinear_filtering flag has its CC layer
// switched, that the trilinear_filtering flag is maintained.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerTrilinearFiltering) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());

  l1->AddTrilinearFilteringRequest();

  // Change l1's cc::Layer.
  l1->SwitchCCLayerForTest();

  // Ensure that the trilinear_filtering flag is maintained.
  EXPECT_TRUE(l1->cc_layer_for_testing()->trilinear_filtering());
}

// Tests that the animators in the layer tree is added to the
// animator-collection when the root-layer is set to the compositor.
TEST_F(LayerWithDelegateTest, RootLayerAnimatorsInCompositor) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_SOLID_COLOR));
  std::unique_ptr<Layer> child(
      CreateColorLayer(SK_ColorRED, gfx::Rect(10, 10)));
  child->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  child->SetOpacity(0.5f);
  root->Add(child.get());

  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());
  compositor()->SetRootLayer(root.get());
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

// Tests that adding/removing a layer adds/removes the animator from its entire
// subtree from the compositor's animator-collection.
TEST_F(LayerWithDelegateTest, AddRemoveLayerUpdatesAnimatorsFromSubtree) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> grandchild(
      CreateColorLayer(SK_ColorRED, gfx::Rect(10, 10)));
  root->Add(child.get());
  child->Add(grandchild.get());
  compositor()->SetRootLayer(root.get());

  grandchild->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  grandchild->SetOpacity(0.5f);
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());

  root->Remove(child.get());
  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());

  root->Add(child.get());
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

TEST_F(LayerWithDelegateTest, DestroyingLayerRemovesTheAnimatorFromCollection) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  root->Add(child.get());
  compositor()->SetRootLayer(root.get());

  child->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  child->SetOpacity(0.5f);
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());

  child.reset();
  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

// A LayerAnimationObserver that removes a child layer from a parent when an
// animation completes.
class LayerRemovingLayerAnimationObserver : public LayerAnimationObserver {
 public:
  LayerRemovingLayerAnimationObserver(Layer* root, Layer* child)
      : root_(root), child_(child) {}

  // LayerAnimationObserver:
  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override {
    root_->Remove(child_);
  }

  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    root_->Remove(child_);
  }

  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override {}

 private:
  Layer* root_;
  Layer* child_;

  DISALLOW_COPY_AND_ASSIGN(LayerRemovingLayerAnimationObserver);
};

// Verifies that empty LayerAnimators are not left behind when removing child
// Layers that own an empty LayerAnimator. See http://crbug.com/552037.
TEST_F(LayerWithDelegateTest, NonAnimatingAnimatorsAreRemovedFromCollection) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> parent(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  root->Add(parent.get());
  parent->Add(child.get());
  compositor()->SetRootLayer(root.get());

  child->SetAnimator(LayerAnimator::CreateDefaultAnimator());

  LayerRemovingLayerAnimationObserver observer(root.get(), parent.get());
  child->GetAnimator()->AddObserver(&observer);

  std::unique_ptr<LayerAnimationElement> element =
      ui::LayerAnimationElement::CreateOpacityElement(
          0.5f, base::TimeDelta::FromSeconds(1));
  LayerAnimationSequence* sequence =
      new LayerAnimationSequence(std::move(element));

  child->GetAnimator()->StartAnimation(sequence);
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());

  child->GetAnimator()->StopAnimating();
  EXPECT_FALSE(root->Contains(parent.get()));
  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

namespace {

std::string Vector2dFTo100thPrecisionString(const gfx::Vector2dF& vector) {
  return base::StringPrintf("%.2f %0.2f", vector.x(), vector.y());
}

}  // namespace

TEST_F(LayerWithRealCompositorTest, SnapLayerToPixels) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> c1(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> c11(CreateLayer(LAYER_TEXTURED));

  GetCompositor()->SetScaleAndSize(1.25f, gfx::Size(100, 100),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  GetCompositor()->SetRootLayer(root.get());
  root->Add(c1.get());
  c1->Add(c11.get());

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  c1->SetBounds(gfx::Rect(1, 1, 10, 10));
  c11->SetBounds(gfx::Rect(1, 1, 10, 10));
  SnapLayerToPhysicalPixelBoundary(root.get(), c11.get());
  // 0.5 at 1.25 scale : (1 - 0.25 + 0.25) / 1.25 = 0.4
  EXPECT_EQ("0.40 0.40",
            Vector2dFTo100thPrecisionString(c11->subpixel_position_offset()));

  GetCompositor()->SetScaleAndSize(1.5f, gfx::Size(100, 100),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  SnapLayerToPhysicalPixelBoundary(root.get(), c11.get());
  // c11 must already be aligned at 1.5 scale.
  EXPECT_EQ("0.00 0.00",
            Vector2dFTo100thPrecisionString(c11->subpixel_position_offset()));

  c11->SetBounds(gfx::Rect(2, 2, 10, 10));
  SnapLayerToPhysicalPixelBoundary(root.get(), c11.get());
  // c11 is now off the pixel.
  // 0.5 / 1.5 = 0.333...
  EXPECT_EQ("0.33 0.33",
            Vector2dFTo100thPrecisionString(c11->subpixel_position_offset()));
}

TEST_F(LayerWithRealCompositorTest, SnapLayerToPixelsWithScaleTransform) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> c1(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> c11(CreateLayer(LAYER_TEXTURED));
  std::unique_ptr<Layer> c111(CreateLayer(LAYER_TEXTURED));

  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(100, 100),
                                   viz::LocalSurfaceId(), base::TimeTicks());
  GetCompositor()->SetRootLayer(root.get());
  root->Add(c1.get());
  c1->Add(c11.get());
  c11->Add(c111.get());

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  c1->SetBounds(gfx::Rect(0, 0, 10, 10));
  c11->SetBounds(gfx::Rect(0, 0, 10, 10));
  c111->SetBounds(gfx::Rect(2, 2, 5, 5));

  gfx::Transform transform;
  transform.Scale(1.25f, 1.25f);

  c1->SetTransform(transform);
  SnapLayerToPhysicalPixelBoundary(root.get(), c111.get());

  // c111 ends up at 2.5, and is supposed to be snapped to 3.0. So subpixel
  // offset is expected to be:
  // 0.5 / 1.25 = 0.40
  EXPECT_EQ("0.40 0.40",
            Vector2dFTo100thPrecisionString(c111->subpixel_position_offset()));

  c11->SetTransform(transform);
  SnapLayerToPhysicalPixelBoundary(root.get(), c111.get());

  // c111 ends up at 3.125, and is supposed to be snapped to 3.0. So subpixel
  // offset is expected to be:
  // -0.125 / (1.25 * 1.25) = -0.08
  EXPECT_EQ("-0.08 -0.08",
            Vector2dFTo100thPrecisionString(c111->subpixel_position_offset()));

  // A transform on c111 should not affect the subpixel offset so expect it to
  // be the same as before.
  c111->SetTransform(transform);
  SnapLayerToPhysicalPixelBoundary(root.get(), c111.get());
  EXPECT_EQ("-0.08 -0.08",
            Vector2dFTo100thPrecisionString(c111->subpixel_position_offset()));
}

// Verify that LayerDelegate::OnLayerBoundsChanged() is called when the bounds
// are set without an animation.
TEST(LayerDelegateTest, OnLayerBoundsChanged) {
  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  const gfx::Rect initial_bounds = layer->bounds();
  constexpr gfx::Rect kTargetBounds(1, 2, 3, 4);
  EXPECT_CALL(delegate,
              OnLayerBoundsChanged(initial_bounds,
                                   PropertyChangeReason::NOT_FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Rect&, PropertyChangeReason) {
        // Verify that |layer->bounds()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->bounds(), kTargetBounds);
      }));
  layer->SetBounds(kTargetBounds);
}

// Verify that LayerDelegate::OnLayerBoundsChanged() is called at every step of
// a bounds animation.
TEST(LayerDelegateTest, OnLayerBoundsChangedAnimation) {
  ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  LayerAnimatorTestController test_controller(
      LayerAnimator::CreateImplicitAnimator());
  LayerAnimator* const animator = test_controller.animator();

  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  layer->SetAnimator(animator);

  const gfx::Rect initial_bounds = layer->bounds();
  constexpr gfx::Rect kTargetBounds(10, 20, 30, 40);
  const gfx::Rect step_bounds =
      gfx::Tween::RectValueBetween(0.5, initial_bounds, kTargetBounds);

  // Start the animation.
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateBoundsElement(
          kTargetBounds, base::TimeDelta::FromSeconds(1));
  ASSERT_FALSE(element->IsThreaded(layer.get()));
  LayerAnimationElement* element_raw = element.get();
  animator->StartAnimation(new LayerAnimationSequence(std::move(element)));
  testing::Mock::VerifyAndClear(&delegate);

  // Progress the animation.
  EXPECT_CALL(delegate,
              OnLayerBoundsChanged(initial_bounds,
                                   PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Rect&, PropertyChangeReason) {
        // Verify that |layer->bounds()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->bounds(), step_bounds);
        EXPECT_TRUE(
            animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
      }));
  test_controller.Step(element_raw->duration() / 2);
  testing::Mock::VerifyAndClear(&delegate);

  // End the animation.
  EXPECT_CALL(delegate, OnLayerBoundsChanged(
                            step_bounds, PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Rect&, PropertyChangeReason) {
        // Verify that |layer->bounds()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->bounds(), kTargetBounds);
        EXPECT_FALSE(
            animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
      }));
  test_controller.Step(element_raw->duration() / 2);
  testing::Mock::VerifyAndClear(&delegate);
}

// Verify that LayerDelegate::OnLayerTransformed() is called when the transform
// is set without an animation.
TEST(LayerDelegateTest, OnLayerTransformed) {
  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  gfx::Transform target_transform1;
  target_transform1.Skew(10.0f, 5.0f);
  {
    EXPECT_CALL(delegate,
                OnLayerTransformed(gfx::Transform(),
                                   PropertyChangeReason::NOT_FROM_ANIMATION))
        .WillOnce(testing::Invoke(
            [&](const gfx::Transform& old_transform, PropertyChangeReason) {
              // Verify that |layer->transform()| returns the correct value when
              // the delegate is notified.
              EXPECT_EQ(target_transform1, layer->transform());
            }));
    layer->SetTransform(target_transform1);
  }
  gfx::Transform target_transform2;
  target_transform2.Skew(10.0f, 5.0f);
  EXPECT_CALL(delegate,
              OnLayerTransformed(target_transform1,
                                 PropertyChangeReason::NOT_FROM_ANIMATION))
      .WillOnce(testing::Invoke(
          [&](const gfx::Transform& old_transform, PropertyChangeReason) {
            // Verify that |layer->transform()| returns the correct value when
            // the delegate is notified.
            EXPECT_EQ(target_transform2, layer->transform());
          }));
  layer->SetTransform(target_transform2);
}

// Verify that LayerDelegate::OnLayerTransformed() is called at every step of a
// non-threaded transform transition.
TEST(LayerDelegateTest, OnLayerTransformedNonThreadedAnimation) {
  ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  LayerAnimatorTestController test_controller(
      LayerAnimator::CreateImplicitAnimator());
  LayerAnimator* const animator = test_controller.animator();

  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  layer->SetAnimator(animator);

  auto interpolated_transform = std::make_unique<InterpolatedRotation>(10, 45);
  const gfx::Transform initial_transform =
      interpolated_transform->Interpolate(0.0);
  const gfx::Transform step_transform =
      interpolated_transform->Interpolate(0.5);
  const gfx::Transform target_transform =
      interpolated_transform->Interpolate(1.0);

  // Start the animation.
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(interpolated_transform), base::TimeDelta::FromSeconds(1));
  // The LayerAnimationElement returned by CreateInterpolatedTransformElement()
  // is non-threaded.
  ASSERT_FALSE(element->IsThreaded(layer.get()));
  LayerAnimationElement* element_raw = element.get();
  EXPECT_CALL(delegate,
              OnLayerTransformed(gfx::Transform(),
                                 PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Transform& old_transform,
                                    PropertyChangeReason) {
        // Verify that |layer->transform()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->transform(), initial_transform);
        EXPECT_TRUE(
            animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
      }));
  animator->StartAnimation(new LayerAnimationSequence(std::move(element)));
  testing::Mock::VerifyAndClear(&delegate);

  // Progress the animation.
  EXPECT_CALL(delegate,
              OnLayerTransformed(initial_transform,
                                 PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke(
          [&](const gfx::Transform& old_transform, PropertyChangeReason) {
            // Verify that |layer->transform()| returns the correct value when
            // the delegate is notified.
            EXPECT_EQ(layer->transform(), step_transform);
          }));
  test_controller.Step(element_raw->duration() / 2);
  testing::Mock::VerifyAndClear(&delegate);

  // End the animation.
  EXPECT_CALL(
      delegate,
      OnLayerTransformed(step_transform, PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Transform& old_transform,
                                    PropertyChangeReason) {
        // Verify that |layer->transform()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->transform(), target_transform);
        EXPECT_FALSE(
            animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
      }));
  test_controller.Step(element_raw->duration() / 2);
  testing::Mock::VerifyAndClear(&delegate);
}

// Verify that LayerDelegate::OnLayerTransformed() is called at the beginning
// and at the end of a threaded transform transition.
TEST(LayerDelegateTest, OnLayerTransformedThreadedAnimation) {
  ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  LayerAnimatorTestController test_controller(
      LayerAnimator::CreateImplicitAnimator());
  LayerAnimator* const animator = test_controller.animator();

  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  layer->SetAnimator(animator);

  // Start the animation.
  gfx::Transform initial_transform = layer->transform();
  gfx::Transform target_transform;
  target_transform.Skew(10.0f, 5.0f);
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateTransformElement(
          target_transform, base::TimeDelta::FromSeconds(1));
  ASSERT_TRUE(element->IsThreaded(layer.get()));
  LayerAnimationElement* element_raw = element.get();
  EXPECT_CALL(delegate,
              OnLayerTransformed(gfx::Transform(),
                                 PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Transform& old_transform,
                                    PropertyChangeReason) {
        // Verify that |layer->transform()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->transform(), initial_transform);
        EXPECT_TRUE(
            animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
      }));
  animator->StartAnimation(new LayerAnimationSequence(std::move(element)));
  testing::Mock::VerifyAndClear(&delegate);
  test_controller.StartThreadedAnimationsIfNeeded();

  // End the animation.
  EXPECT_CALL(delegate,
              OnLayerTransformed(initial_transform,
                                 PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](const gfx::Transform& old_transform,
                                    PropertyChangeReason) {
        // Verify that |layer->transform()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->transform(), target_transform);
        EXPECT_FALSE(
            animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
      }));
  test_controller.Step(
      element_raw->duration() +
      (element_raw->effective_start_time() - animator->last_step_time()));
  testing::Mock::VerifyAndClear(&delegate);
}

// Verify that LayerDelegate::OnLayerOpacityChanged() is called when the opacity
// is set without an animation.
TEST(LayerDelegateTest, OnLayerOpacityChanged) {
  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  constexpr float kTargetOpacity = 0.5f;
  EXPECT_CALL(delegate,
              OnLayerOpacityChanged(PropertyChangeReason::NOT_FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](PropertyChangeReason) {
        // Verify that |layer->opacity()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->opacity(), kTargetOpacity);
      }));
  layer->SetOpacity(kTargetOpacity);
}

// Verify that LayerDelegate::OnLayerOpacityChanged() is called at the beginning
// and at the end of a threaded opacity animation.
TEST(LayerDelegateTest, OnLayerOpacityChangedAnimation) {
  ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  LayerAnimatorTestController test_controller(
      LayerAnimator::CreateImplicitAnimator());
  LayerAnimator* const animator = test_controller.animator();

  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);
  layer->SetAnimator(animator);

  // Start the animation.
  const float initial_opacity = layer->opacity();
  const float kTargetOpacity = 0.5f;
  std::unique_ptr<LayerAnimationElement> element =
      LayerAnimationElement::CreateOpacityElement(
          kTargetOpacity, base::TimeDelta::FromSeconds(1));
  ASSERT_TRUE(element->IsThreaded(layer.get()));
  LayerAnimationElement* element_raw = element.get();
  EXPECT_CALL(delegate,
              OnLayerOpacityChanged(PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](PropertyChangeReason) {
        // Verify that |layer->opacity()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->opacity(), initial_opacity);
        EXPECT_TRUE(
            animator->IsAnimatingProperty(LayerAnimationElement::OPACITY));
      }));
  animator->StartAnimation(new LayerAnimationSequence(std::move(element)));
  testing::Mock::VerifyAndClear(&delegate);
  test_controller.StartThreadedAnimationsIfNeeded();

  // End the animation.
  EXPECT_CALL(delegate,
              OnLayerOpacityChanged(PropertyChangeReason::FROM_ANIMATION))
      .WillOnce(testing::Invoke([&](PropertyChangeReason) {
        // Verify that |layer->opacity()| returns the correct value when the
        // delegate is notified.
        EXPECT_EQ(layer->opacity(), kTargetOpacity);
        EXPECT_FALSE(
            animator->IsAnimatingProperty(LayerAnimationElement::OPACITY));
      }));
  test_controller.Step(
      element_raw->duration() +
      (element_raw->effective_start_time() - animator->last_step_time()));
  testing::Mock::VerifyAndClear(&delegate);
}

// Verify that LayerDelegate::OnLayerAlphaShapeChanged() is called when the
// alpha shape of a layer is set.
TEST(LayerDelegateTest, OnLayerAlphaShapeChanged) {
  auto layer = std::make_unique<Layer>(LAYER_TEXTURED);
  testing::StrictMock<TestLayerDelegate> delegate;
  layer->set_delegate(&delegate);

  // Set an alpha shape for the layer. Expect the delegate to be notified.
  auto shape = std::make_unique<Layer::ShapeRects>();
  shape->emplace_back(0, 0, 10, 20);
  EXPECT_CALL(delegate, OnLayerAlphaShapeChanged());
  layer->SetAlphaShape(std::move(shape));
  testing::Mock::VerifyAndClear(&delegate);

  // Clear the alpha shape for the layer. Expect the delegate to be notified.
  EXPECT_CALL(delegate, OnLayerAlphaShapeChanged());
  layer->SetAlphaShape(nullptr);
  testing::Mock::VerifyAndClear(&delegate);
}

TEST_F(LayerWithRealCompositorTest, CompositorAnimationObserverTest) {
  std::unique_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));

  root->SetAnimator(LayerAnimator::CreateImplicitAnimator());

  TestCompositorAnimationObserver animation_observer(GetCompositor());
  EXPECT_EQ(0u, animation_observer.animation_step_count());

  root->SetOpacity(0.5f);
  WaitForDraw();
  EXPECT_EQ(1u, animation_observer.animation_step_count());

  EXPECT_FALSE(animation_observer.shutdown());
  ResetCompositor();
  EXPECT_TRUE(animation_observer.shutdown());
}

TEST(LayerDebugInfoTest, LayerNameDoesNotClobber) {
  Layer layer(LAYER_NOT_DRAWN);
  layer.set_name("foo");
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info =
      layer.TakeDebugInfo(nullptr);
  std::string trace_format("bar,");
  debug_info->AppendAsTraceFormat(&trace_format);
  std::string expected("bar,{\"layer_name\":\"foo\"}");
  EXPECT_EQ(expected, trace_format);
}

}  // namespace ui
