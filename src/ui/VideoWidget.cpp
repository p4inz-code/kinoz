// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/VideoWidget.h"
#include "core/player/MpvPlayer.h"
#include "ui/Theme.h"
#include <mpv/render_gl.h>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QWheelEvent>
#include <QtMath>

static void* getProcAddress(void*, const char* name) {
    auto* c = QOpenGLContext::currentContext();
    return c ? reinterpret_cast<void*>(c->getProcAddress(name)) : nullptr;
}

static const char* kVS = R"(#version 330 core
out vec2 uv;
void main(){ vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)); uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); })";

static const char* kBlurFS = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D tex; uniform vec2 dir; uniform vec2 uvMax; uniform vec2 uvHalf;
void main(){
  vec2 base = uv * uvMax;
  vec2 lo = uvHalf, hi = uvMax - uvHalf;
  vec4 s = texture(tex, clamp(base, lo, hi)) * 0.2270270270;
  s += texture(tex, clamp(base + dir * 1.3846153846, lo, hi)) * 0.3162162162;
  s += texture(tex, clamp(base - dir * 1.3846153846, lo, hi)) * 0.3162162162;
  s += texture(tex, clamp(base + dir * 3.2307692308, lo, hi)) * 0.0702702703;
  s += texture(tex, clamp(base - dir * 3.2307692308, lo, hi)) * 0.0702702703;
  o = s;
})";

static const char* kCompFS = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D blurTex; uniform vec2 uvMax; uniform vec4 rectPx; uniform float radius; uniform float opacity;
uniform vec4 tint; uniform float time; uniform vec2 mouse; uniform float metal;
float sdRound(vec2 p, vec2 b, float r){ vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main(){
  vec2 frag = gl_FragCoord.xy;
  vec2 c = rectPx.xy + rectPx.zw * 0.5;
  float d = sdRound(frag - c, rectPx.zw * 0.5, radius);
  float mask = 1.0 - smoothstep(-1.0, 1.0, d);
  if (mask <= 0.001) discard;
  vec2 q = (frag - rectPx.xy) / rectPx.zw;
  vec3 bg = texture(blurTex, clamp(q, 0.0, 1.0) * uvMax).rgb;
  float l = dot(bg, vec3(0.299, 0.587, 0.114));
  bg = mix(vec3(l), bg, 1.35) * 0.80;
  vec3 col = mix(bg, tint.rgb, tint.a);
  // liquid-metal sheen: slow travelling specular band + pointer glint
  float t = time * 0.12;
  float band = smoothstep(0.62, 1.0, sin((q.x + q.y * 0.35 - t) * 3.4558));
  float glint = exp(-pow(length((q - mouse) * vec2(rectPx.z / max(rectPx.w, 1.0), 1.0)), 2.0) * 5.0);
  col += metal * (band * 0.040 + glint * 0.060);
  // rim light, brighter toward the top edge; faint inner shade at the bottom
  float edge = smoothstep(0.0, 7.0, -d);
  float rim = (1.0 - edge) * (0.20 + 0.80 * smoothstep(0.30, 1.0, q.y));
  col += rim * 0.30 * metal;
  col += (q.y - 0.5) * 0.05;
  col += (hash(frag) - 0.5) * 0.014;
  o = vec4(col, mask * opacity * 0.97);
})";

VideoWidget::VideoWidget(MpvPlayer* player, QWidget* parent) : QOpenGLWidget(parent), m_player(player) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_clock.start();
    connect(m_player, &MpvPlayer::ready, this, [this] {
        if (m_glReady && !m_ctx) { makeCurrent(); createContext(); doneCurrent(); update(); }
    });
}

VideoWidget::~VideoWidget() {
    makeCurrent();
    if (m_ctx) { mpv_render_context_free(m_ctx); m_ctx = nullptr; }
    if (m_glassOk) {
        glDeleteTextures(2, m_tex); glDeleteFramebuffers(2, m_fbo);
        glDeleteProgram(m_blurProg); glDeleteProgram(m_compProg); glDeleteVertexArrays(1, &m_vao);
    }
    doneCurrent();
}

GLuint VideoWidget::compile(const char* vs, const char* fs) {
    auto sh = [&](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { char buf[512]; glGetShaderInfoLog(s, 512, nullptr, buf); qWarning("shader: %s", buf); glDeleteShader(s); return 0; }
        return s;
    };
    const GLuint v = sh(GL_VERTEX_SHADER, vs), f = sh(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(p); return 0; }
    return p;
}

bool VideoWidget::initGlass() {
    if (!initializeOpenGLFunctions()) return false;
    m_blurProg = compile(kVS, kBlurFS);
    m_compProg = compile(kVS, kCompFS);
    if (!m_blurProg || !m_compProg) return false;
    glGenVertexArrays(1, &m_vao);
    glGenTextures(2, m_tex);
    glGenFramebuffers(2, m_fbo);
    return true;
}

void VideoWidget::ensureBlurTargets(int w, int h) {
    if (w <= m_texW && h <= m_texH) return;
    m_texW = qMax(w, m_texW); m_texH = qMax(h, m_texH);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, m_tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_texW, m_texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex[i], 0);
    }
}

void VideoWidget::setGlass(const QVector<Glass>& g) {
    bool same = g.size() == m_glass.size();
    for (int i = 0; same && i < g.size(); ++i)
        same = g[i].rect == m_glass[i].rect && qFuzzyCompare(g[i].opacity + 1, m_glass[i].opacity + 1) && g[i].tint == m_glass[i].tint && qFuzzyCompare(g[i].tintStrength + 1, m_glass[i].tintStrength + 1);
    if (same) return;
    m_glass = g;
    update();
}

void VideoWidget::initializeGL() {
    m_glReady = true;
    m_glassOk = initGlass();
    if (auto* f = QOpenGLContext::currentContext()->functions()) {
        m_glInfo = QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_RENDERER))) + QStringLiteral(", OpenGL ") + QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_VERSION)));
    }
    emit glassReady(m_glassOk);
    if (m_player->isReady()) createContext();
    connect(this, &QOpenGLWidget::frameSwapped, this, [this] { if (m_ctx) mpv_render_context_report_swap(m_ctx); });
}

void VideoWidget::createContext() {
    if (m_ctx || !m_player->handle()) return;
    mpv_opengl_init_params gl{getProcAddress, nullptr};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&m_ctx, m_player->handle(), params) < 0) { m_ctx = nullptr; return; }
    mpv_render_context_set_update_callback(m_ctx, [](void* ctx) {
        QMetaObject::invokeMethod(static_cast<VideoWidget*>(ctx), qOverload<>(&QWidget::update), Qt::QueuedConnection);
    }, this);
}

void VideoWidget::paintGL() {
    const qreal dpr = devicePixelRatioF();
    if (!m_ctx) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    mpv_opengl_fbo fbo{static_cast<int>(defaultFramebufferObject()), int(width() * dpr), int(height() * dpr), 0};
    int flip = 1, block = 0;   // Qt's swap already waits for vsync: don't make mpv wait a second time
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(m_ctx, params);
    if (m_glassOk && !m_glass.isEmpty()) drawGlass();
}

void VideoWidget::drawGlass() {
    const qreal dpr = devicePixelRatioF();
    const int fw = int(width() * dpr), fh = int(height() * dpr);
    const GLuint defFbo = defaultFramebufferObject();
    const float time = float(m_clock.elapsed() / 1000.0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(m_vao);
    for (const Glass& g : m_glass) {
        if (g.opacity < 0.01) continue;
        const int x = qBound(0, int(g.rect.left() * dpr), fw - 2), w = qBound(2, int(g.rect.width() * dpr), fw - x);
        const int yTop = qBound(0, int(g.rect.top() * dpr), fh - 2), h = qBound(2, int(g.rect.height() * dpr), fh - yTop);
        const int y = fh - yTop - h;
        const int sw = qMax(2, (w + 3) / 4), sh = qMax(2, (h + 3) / 4);
        ensureBlurTargets(sw, sh);
        // 1) grab the backdrop at quarter resolution
        glBindFramebuffer(GL_READ_FRAMEBUFFER, defFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo[0]);
        glBlitFramebuffer(x, y, x + w, y + h, 0, 0, sw, sh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        // 2) gaussian blur, 2 iterations (ping-pong)
        glUseProgram(m_blurProg);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(m_blurProg, "tex"), 0);
        const float uvx = float(sw) / m_texW, uvy = float(sh) / m_texH;
        glUniform2f(glGetUniformLocation(m_blurProg, "uvMax"), uvx, uvy);
        glUniform2f(glGetUniformLocation(m_blurProg, "uvHalf"), 0.5f / m_texW, 0.5f / m_texH);
        glViewport(0, 0, sw, sh);
        for (int it = 0; it < 2; ++it) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[1]); glBindTexture(GL_TEXTURE_2D, m_tex[0]);
            glUniform2f(glGetUniformLocation(m_blurProg, "dir"), 1.0f / m_texW, 0.0f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[0]); glBindTexture(GL_TEXTURE_2D, m_tex[1]);
            glUniform2f(glGetUniformLocation(m_blurProg, "dir"), 0.0f, 1.0f / m_texH);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        // 3) composite: tint, sheen, rim light, rounded mask
        glBindFramebuffer(GL_FRAMEBUFFER, defFbo);
        glViewport(0, 0, fw, fh);
        glEnable(GL_SCISSOR_TEST); glScissor(x, y, w, h);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_compProg);
        glBindTexture(GL_TEXTURE_2D, m_tex[0]);
        glUniform1i(glGetUniformLocation(m_compProg, "blurTex"), 0);
        glUniform2f(glGetUniformLocation(m_compProg, "uvMax"), uvx, uvy);
        glUniform4f(glGetUniformLocation(m_compProg, "rectPx"), float(x), float(y), float(w), float(h));
        glUniform1f(glGetUniformLocation(m_compProg, "radius"), float(g.radius * dpr));
        glUniform1f(glGetUniformLocation(m_compProg, "opacity"), float(g.opacity));
        glUniform4f(glGetUniformLocation(m_compProg, "tint"), g.tint.redF(), g.tint.greenF(), g.tint.blueF(), float(g.tintStrength));
        glUniform1f(glGetUniformLocation(m_compProg, "time"), time);
        glUniform2f(glGetUniformLocation(m_compProg, "mouse"), float(m_pointer.x()), float(1.0 - m_pointer.y()));
        glUniform1f(glGetUniformLocation(m_compProg, "metal"), float(g.metal));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, defFbo);
    glViewport(0, 0, fw, fh);
}

void VideoWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) { emit rightClicked(e->pos()); return; }
    if (e->button() != Qt::LeftButton) return;
    m_down = true; m_dragging = false; m_press = m_last = e->pos();
}

void VideoWidget::mouseMoveEvent(QMouseEvent* e) {
    m_pointer = QPointF(e->position().x() / qMax(1, width()), e->position().y() / qMax(1, height()));
    emit moved();
    if (!m_down) return;
    if (!m_dragging && (e->pos() - m_press).manhattanLength() > 5) m_dragging = true;
    if (m_dragging) { emit dragged(e->pos() - m_last); m_last = e->pos(); }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (m_down && !m_dragging) emit clicked(e->pos());
    m_down = false; m_dragging = false;
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent* e) { if (e->button() == Qt::LeftButton) emit doubleClicked(); }

void VideoWidget::wheelEvent(QWheelEvent* e) {
    emit wheel(e->angleDelta().y(), e->position(), e->modifiers());
    e->accept();
}
