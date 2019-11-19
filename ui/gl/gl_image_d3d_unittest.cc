// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_d3d.h"

#include "build/build_config.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gl {
namespace {

const uint8_t kImageColor[] = {0x30, 0x40, 0x10, 0xFF};

template <DXGI_FORMAT texture_format, GLenum internal_format, GLenum data_type>
class GLImageD3DTestDelegate : public GLImageTestDelegateBase {
 public:
  void DidSetUp() override {
    d3d11_device_ = QueryD3D11DeviceObjectFromANGLE();
  }

  void WillTearDown() override { d3d11_device_ = nullptr; }

  base::Optional<GLImplementation> GetPreferedGLImplementation()
      const override {
    return base::Optional<GLImplementation>(kGLImplementationEGLANGLE);
  }

  bool SkipTest() const override { return !d3d11_device_; }

  scoped_refptr<GLImageD3D> CreateImage(const gfx::Size& size) const {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = texture_format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
    HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
    EXPECT_TRUE(SUCCEEDED(hr));

    auto image = base::MakeRefCounted<GLImageD3D>(
        size, internal_format, data_type, std::move(d3d11_texture), nullptr);
    EXPECT_TRUE(image->Initialize());
    return image;
  }

  scoped_refptr<GLImageD3D> CreateSolidColorImage(const gfx::Size& size,
                                                  const uint8_t color[4]) {
    auto image = CreateImage(size);

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
    HRESULT hr = d3d11_device_->CreateRenderTargetView(image->texture().Get(),
                                                       nullptr, &render_target);
    EXPECT_TRUE(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device_->GetImmediateContext(&context);
    EXPECT_TRUE(context);

    float clear_color[4];
    for (size_t i = 0; i < 4; i++)
      clear_color[i] = color[i] / 255.f;

    context->ClearRenderTargetView(render_target.Get(), clear_color);

    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_2D; }

  const uint8_t* GetImageColor() const { return kImageColor; }

  int GetAdmissibleError() const { return 0; }

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
};

using GLImageTestTypes =
    testing::Types<GLImageD3DTestDelegate<DXGI_FORMAT_B8G8R8A8_UNORM,
                                          GL_BGRA_EXT,
                                          GL_UNSIGNED_BYTE>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_B8G8R8A8_UNORM,
                                          GL_RGBA,
                                          GL_UNSIGNED_BYTE>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_B8G8R8A8_UNORM,
                                          GL_RGB,
                                          GL_UNSIGNED_BYTE>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_R8G8B8A8_UNORM,
                                          GL_RGBA,
                                          GL_UNSIGNED_BYTE>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_R8G8B8A8_UNORM,
                                          GL_RGB,
                                          GL_UNSIGNED_BYTE>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_R16G16B16A16_FLOAT,
                                          GL_RGBA,
                                          GL_HALF_FLOAT_OES>,
                   GLImageD3DTestDelegate<DXGI_FORMAT_R16G16B16A16_FLOAT,
                                          GL_RGB,
                                          GL_HALF_FLOAT_OES>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageD3D, GLImageTest, GLImageTestTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(GLImageD3D,
                               GLImageOddSizeTest,
                               GLImageTestTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(GLImageD3D,
                               GLImageZeroInitializeTest,
                               GLImageTestTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(GLImageD3D, GLImageBindTest, GLImageTestTypes);

}  // namespace
}  // namespace gl
