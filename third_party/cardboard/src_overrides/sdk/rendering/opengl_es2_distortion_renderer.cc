/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <array>
#include <vector>

#ifdef CARDBOARD_USE_CUSTOM_GL_BINDINGS
#include "opengl_es2_custom_bindings.h"
#else
#ifdef __ANDROID__
#include <GLES2/gl2.h>
#endif
#ifdef __APPLE__
#include <OpenGLES/ES2/gl.h>
#endif
#ifdef __ANDROID__
#include <GLES2/gl2ext.h>
#endif
#endif  // CARDBOARD_USE_CUSTOM_GL_BINDINGS
#include "distortion_renderer.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"
#include "third_party/cardboard/src/sdk/util/is_arg_null.h"
#include "third_party/cardboard/src/sdk/util/is_initialized.h"
#include "third_party/cardboard/src/sdk/util/logging.h"

namespace {

constexpr const char* kDistortionVertexShader =
    R"glsl(
    attribute vec2 a_Position;
    attribute vec2 a_TexCoords;
    varying vec2 v_TexCoords;

    void main() {
      gl_Position = vec4(a_Position, 0, 1);
      v_TexCoords = a_TexCoords;
    })glsl";

constexpr const char* kDistortionFragmentShaderTexture2D =
    R"glsl(
    precision mediump float;

    uniform sampler2D u_Texture;
    uniform vec2 u_Start;
    uniform vec2 u_End;
    varying vec2 v_TexCoords;

    void main() {
      vec2 coords = u_Start + v_TexCoords * (u_End - u_Start);
      gl_FragColor = texture2D(u_Texture, coords);
    })glsl";

#ifdef __ANDROID__
constexpr const char* kDistortionFragmentShaderTextureExternalOes =
    R"glsl(
    #extension GL_OES_EGL_image_external : require
    precision mediump float;

    uniform samplerExternalOES u_Texture;
    uniform vec2 u_Start;
    uniform vec2 u_End;
    varying vec2 v_TexCoords;

    void main() {
      vec2 coords = u_Start + v_TexCoords * (u_End - u_Start);
      gl_FragColor = texture2D(u_Texture, coords);
    })glsl";
#endif

void CheckGlError(const char* label) {
  int gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    CARDBOARD_LOGE("GL error %s: %d", label, gl_error);
  }
}

GLuint LoadShader(GLenum shader_type, const char* source) {
  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  CheckGlError("glCompileShader");
  GLint result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE) {
    int log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
      return 0;
    }

    std::vector<char> log_string(log_length);
    glGetShaderInfoLog(shader, log_length, nullptr, log_string.data());
    CARDBOARD_LOGE("Could not compile shader of type %d: %s", shader_type,
                   log_string.data());

    shader = 0;
  }

  return shader;
}

GLuint CreateProgram(const char* vertex, const char* fragment) {
  GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER, vertex);
  if (vertex_shader == 0) {
    return 0;
  }

  GLuint fragment_shader = LoadShader(GL_FRAGMENT_SHADER, fragment);
  if (fragment_shader == 0) {
    return 0;
  }

  GLuint program = glCreateProgram();

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  CheckGlError("glLinkProgram");

  GLint result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (result == GL_FALSE) {
    int log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
      return 0;
    }

    std::vector<char> log_string(log_length);
    glGetShaderInfoLog(program, log_length, nullptr, log_string.data());
    CARDBOARD_LOGE("Could not compile program: %s", log_string.data());

    return 0;
  }

  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  CheckGlError("GlCreateProgram");

  return program;
}

}  // namespace

namespace cardboard::rendering {

// @brief OpenGL ES 2.0 concrete implementation of DistortionRenderer.
class OpenGlEs2DistortionRenderer : public DistortionRenderer {
 public:
  OpenGlEs2DistortionRenderer(
      const CardboardOpenGlEsDistortionRendererConfig* config)
      : vertices_vbo_{0, 0},
        uvs_vbo_{0, 0},
        elements_vbo_{0, 0},
        elements_count_{0, 0},
        eye_texture_type_{GL_TEXTURE_2D} {
    const char* fragment_shader;

    switch (config->texture_type) {
      case kGlTexture2D:
        fragment_shader = kDistortionFragmentShaderTexture2D;
        eye_texture_type_ = GL_TEXTURE_2D;
        break;
#ifdef __ANDROID__
      case kGlTextureExternalOes:
        fragment_shader = kDistortionFragmentShaderTextureExternalOes;
        eye_texture_type_ = GL_TEXTURE_EXTERNAL_OES;
        break;
#endif
      default:
        CARDBOARD_LOGE(
            "The Cardboard SDK does not support the selected texture type on "
            "this platform. Setting GL_TEXTURE_2D as default.");

        fragment_shader = kDistortionFragmentShaderTexture2D;
        eye_texture_type_ = GL_TEXTURE_2D;
        break;
    }

    program_ = CreateProgram(kDistortionVertexShader, fragment_shader);
    attrib_pos_ = glGetAttribLocation(program_, "a_Position");
    attrib_tex_ = glGetAttribLocation(program_, "a_TexCoords");
    uniform_start_ = glGetUniformLocation(program_, "u_Start");
    uniform_end_ = glGetUniformLocation(program_, "u_End");

    // Gen buffers, one per eye.
    glGenBuffers(2, &vertices_vbo_[0]);
    glGenBuffers(2, &uvs_vbo_[0]);
    glGenBuffers(2, &elements_vbo_[0]);
    CheckGlError("OpenGlEs2DistortionRendererSetUp");
  }

  ~OpenGlEs2DistortionRenderer() {
    glDeleteBuffers(2, &vertices_vbo_[0]);
    glDeleteBuffers(2, &uvs_vbo_[0]);
    glDeleteBuffers(2, &elements_vbo_[0]);
    CheckGlError("~OpenGlEs2DistortionRenderer");
  }

  /*
   * Modifies the OpenGL global state. In particular:
   *   - glGet(GL_ARRAY_BUFFER_BINDING)
   *   - glGet(GL_ELEMENT_ARRAY_BUFFER_BINDING)
   */
  void SetMesh(const CardboardMesh* mesh, CardboardEye eye) override {
    glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_[eye]);
    glBufferData(
        GL_ARRAY_BUFFER,
        mesh->n_vertices * sizeof(float) * 2,  // Two components per vertex
        mesh->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, uvs_vbo_[eye]);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh->n_vertices * sizeof(float) * 2,  // Two components per uv
                 mesh->uvs, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_vbo_[eye]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->n_indices * sizeof(int),
                 mesh->indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    CheckGlError("OpenGlEs2DistortionRenderer::SetMesh");
    elements_count_[eye] = mesh->n_indices;
  }

  /*
   * Modifies the OpenGL global state. In particular:
   *   - glGet(GL_VIEWPORT)
   *   - glGet(GL_FRAMEBUFFER_BINDING)
   *   - glIsEnabled(GL_SCISSOR_TEST)
   *   - glIsEnabled(GL_CULL_FACE)
   *   - glGet(GL_CLEAR_COLOR_VALUE)
   *   - glGet(GL_CURRENT_PROGRAM)
   *   - glGet(GL_SCISSOR_BOX)
   *   - glGet(GL_ACTIVE_TEXTURE+i)
   *   - glGet(GL_ARRAY_BUFFER_BINDING)
   *   - glGet(GL_ELEMENT_ARRAY_BUFFER_BINDING)
   */
  void RenderEyeToDisplay(
      uint64_t target, int x, int y, int width, int height,
      const CardboardEyeTextureDescription* left_eye,
      const CardboardEyeTextureDescription* right_eye) override {
    if (elements_count_[0] == 0 || elements_count_[1] == 0) {
      CARDBOARD_LOGE(
          "Distortion mesh is empty. OpenGlEs2DistortionRenderer::SetMesh was "
          "not called yet.");
      return;
    }

    glViewport(x, y, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(.0f, .0f, .0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glUseProgram(program_);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width / 2, height);
    RenderDistortionMesh(left_eye, kLeft);

    glScissor(x + width / 2, y, width / 2, height);
    RenderDistortionMesh(right_eye, kRight);

    // Active GL_TEXTURE0 effectively enables the first texture that is
    // deactiviated by the DistortionRenderer. Binding array buffer and element
    // array buffer to the reserved value zero effectively unbinds the buffer
    // objects that are previously bound by the DistortionRenderer.
    glActiveTexture(GL_TEXTURE0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Disable scissor test.
    glDisable(GL_SCISSOR_TEST);
    CheckGlError("OpenGlEs2DistortionRenderer::RenderEyeToDisplay");
  }

 private:
  /*
   * Modifies the OpenGL global state. In particular:
   *   - glGet(GL_ARRAY_BUFFER_BINDING)
   *   - glGet(GL_ELEMENT_ARRAY_BUFFER_BINDING)
   *   - glGetVertexAttrib(i, GL_VERTEX_ATTRIB_*)
   *   - glGetVertextAttrib(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED)
   *   - glGet(GL_ACTIVE_TEXTURE+i)
   *   - glGet(GL_TEXTURE_BINDING_2D)
   *   - glGetUniform(program, location)
   *   - glGet(GL_ARRAY_BUFFER_BINDING)
   *   - glGet(GL_ELEMENT_ARRAY_BUFFER_BINDING)
   */
  void RenderDistortionMesh(
      const CardboardEyeTextureDescription* eye_description,
      CardboardEye eye) const {
    glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_[eye]);
    glVertexAttribPointer(
        attrib_pos_,
        2,  // 2 components per vertex
        GL_FLOAT, false,
        0,  // Stride and offset 0, as we are using different vbos.
        0);
    glEnableVertexAttribArray(attrib_pos_);

    glBindBuffer(GL_ARRAY_BUFFER, uvs_vbo_[eye]);
    glVertexAttribPointer(attrib_tex_,
                          2,  // 2 components per uv
                          GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(attrib_tex_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(eye_texture_type_,
                  static_cast<GLuint>(eye_description->texture));

    glUniform2f(uniform_start_, eye_description->left_u,
                eye_description->bottom_v);
    glUniform2f(uniform_end_, eye_description->right_u, eye_description->top_v);

    // Draw with indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_vbo_[eye]);
    glDrawElements(GL_TRIANGLE_STRIP, elements_count_[eye], GL_UNSIGNED_INT, 0);
    CheckGlError("OpenGlEs2DistortionRenderer::RenderDistortionMesh");
  }

  std::array<GLuint, 2> vertices_vbo_;  // One per eye.
  std::array<GLuint, 2> uvs_vbo_;
  std::array<GLuint, 2> elements_vbo_;
  std::array<int, 2> elements_count_;

  GLuint program_;
  GLuint attrib_pos_;
  GLuint attrib_tex_;
  GLuint uniform_start_;
  GLuint uniform_end_;

  GLenum eye_texture_type_;
};

}  // namespace cardboard::rendering

extern "C" {

CardboardDistortionRenderer* CardboardOpenGlEs2DistortionRenderer_create(
    const CardboardOpenGlEsDistortionRendererConfig* config) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(config)) {
    return nullptr;
  }
  return reinterpret_cast<CardboardDistortionRenderer*>(
      new cardboard::rendering::OpenGlEs2DistortionRenderer(config));
}

}  // extern "C"
