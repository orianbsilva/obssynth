#include "gl_renderer.hpp"
#include "hershey_font.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <gl/GL.h>
#include <cstring>
#include <cmath>

namespace synth
{

    void GLRenderer::begin(int w, int h)
    {
        glViewport(0, 0, w, h);
        glClearColor(0.07f, 0.07f, 0.09f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
    }

    void GLRenderer::drawGridAndBorder(float xMin, float xMax, float yMin, float yMax, int divisions)
    {
        float spanX = xMax - xMin;
        float spanY = yMax - yMin;
        const float edgeEpsX = spanX * 0.0015f; // nudge inside to avoid clip
        const float edgeEpsY = spanY * 0.0015f;
        glColor3f(0.2f, 0.2f, 0.2f);
        glBegin(GL_LINES);
        for (int i = 0; i <= divisions; ++i)
        {
            float t = (float)i / (float)divisions;
            float gx = xMin + t * spanX;
            glVertex2f(gx, yMin + edgeEpsY);
            glVertex2f(gx, yMax - edgeEpsY);
            float gy = yMin + t * spanY;
            glVertex2f(xMin + edgeEpsX, gy);
            glVertex2f(xMax - edgeEpsX, gy);
        }
        glEnd();

        glColor3f(0.5f, 0.5f, 0.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(xMin + edgeEpsX, yMin + edgeEpsY);
        glVertex2f(xMax - edgeEpsX, yMin + edgeEpsY);
        glVertex2f(xMax - edgeEpsX, yMax - edgeEpsY);
        glVertex2f(xMin + edgeEpsX, yMax - edgeEpsY);
        glEnd();
    }

    void GLRenderer::drawWave(int x, int y, int w, int h, const std::vector<Point> &wave)
    {
        if (w <= 0 || h <= 0)
            return;
        OrthoRect r = computeWaveOrtho(w, h);
        applyViewportAndProjection(x, y, w, h, r);
        drawGridAndBorder(r.xMin, r.xMax, r.yMin, r.yMax, 10);

        glColor3f(0.2f, 0.8f, 0.3f);
        glBegin(GL_LINE_STRIP);
        for (auto &p : wave)
        {
            float yScaled = p.y * 0.4f * 0.5f + 0.0f; // scale inside [-0.2,0.2]
            glVertex2f(p.x, yScaled);
        }
        glEnd();
        // Midline
        glColor3f(0.3f, 0.3f, 0.3f);
        glBegin(GL_LINES);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
        glEnd();
    }

    void GLRenderer::drawEnvelope(int x, int y, int w, int h, const std::vector<Point> &env)
    {
        if (w <= 0 || h <= 0)
            return;
        OrthoRect r = computeEnvelopeOrtho(w, h);
        applyViewportAndProjection(x, y, w, h, r);
        drawGridAndBorder(r.xMin, r.xMax, r.yMin, r.yMax, 10);

        // Horizontal reference at sustain ~0.6 for example (draw at y=0.6)
        glColor3f(0.4f, 0.4f, 0.4f);
        glBegin(GL_LINES);
        glVertex2f(0.0f, 0.6f);
        glVertex2f(1.0f, 0.6f);
        glEnd();

        glColor3f(0.9f, 0.6f, 0.2f);
        glBegin(GL_LINE_STRIP);
        for (auto &p : env)
        {
            glVertex2f(p.x, p.y);
        }
        glEnd();
    }

    void GLRenderer::end() {}

    void GLRenderer::drawKnob(int x, int y, int size, float value, float minValue, float maxValue)
    {
        if (size <= 4)
            return;
        float vNorm = (maxValue - minValue) > 0.0f ? (value - minValue) / (maxValue - minValue) : 0.0f;
        vNorm = clampf(vNorm, 0.0f, 1.0f);
        // Knob angle range: 135° (-135°) to 45° (i.e., 270° sweep)
        const float startDeg = 135.0f;
        const float endDeg = 405.0f; // 135 + 270
        float angleDeg = startDeg + vNorm * (endDeg - startDeg);
        float angleRad = angleDeg * 3.14159265f / 180.0f;

        // Viewport + ortho 0..1 to simplify drawing relative
        applyViewportAndProjection(x, y, size, size, OrthoRect{0, 1, 0, 1});

        float cx = 0.5f, cy = 0.5f, rOuter = 0.48f, rInner = 0.36f;
        // Background ring
        glColor3f(0.15f, 0.15f, 0.17f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 64; ++i)
        {
            float t = (float)i / 64.0f * 2.0f * 3.14159265f;
            glVertex2f(cx + cosf(t) * rOuter, cy + sinf(t) * rOuter);
        }
        glEnd();
        // Inner circle cutout to form ring
        glColor3f(0.07f, 0.07f, 0.09f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 64; ++i)
        {
            float t = (float)i / 64.0f * 2.0f * 3.14159265f;
            glVertex2f(cx + cosf(t) * rInner, cy + sinf(t) * rInner);
        }
        glEnd();
        // Value arc
        glColor3f(0.25f, 0.75f, 0.95f);
        glBegin(GL_LINE_STRIP);
        int arcSegments = 48;
        for (int i = 0; i <= arcSegments; ++i)
        {
            float ft = (float)i / arcSegments;
            float deg = startDeg + ft * (angleDeg - startDeg);
            float rad = deg * 3.14159265f / 180.0f;
            glVertex2f(cx + cosf(rad) * rOuter * 0.9f, cy + sinf(rad) * rOuter * 0.9f);
        }
        glEnd();
        // Indicator line
        glColor3f(0.9f, 0.9f, 0.9f);
        float ix = cx + cosf(angleRad) * rInner * 0.9f;
        float iy = cy + sinf(angleRad) * rInner * 0.9f;
        glBegin(GL_LINES);
        glVertex2f(cx, cy);
        glVertex2f(ix, iy);
        glEnd();
    }

    void GLRenderer::drawMaterialKnob(int x, int y, int size, float value, float minValue, float maxValue, bool active, bool hovered)
    {
        if (size <= 8)
            return;
        float vNorm = (maxValue - minValue) > 0.0f ? (value - minValue) / (maxValue - minValue) : 0.0f;
        vNorm = clampf(vNorm, 0.0f, 1.0f);

        // Angle mapping: 225° (7 o'clock) sweep 270° to 315° (5 o'clock) — CCW from bottom-left
        const float PI = 3.14159265f;
        const float startDeg = 225.0f; // bottom-left (7 o'clock)
        const float sweepDeg = 270.0f;
        float valDeg = startDeg - vNorm * sweepDeg; // CCW: subtract to go clockwise visually
        float valRad = valDeg * PI / 180.0f;

        applyViewportAndProjection(x, y, size, size, OrthoRect{0, 1, 0, 1});

        float cx = 0.5f, cy = 0.5f;
        float rBody = 0.40f;
        float rTrack = 0.46f;
        const int segs = 64;

        // --- Shadow (offset down-right, slightly larger) ---
        {
            float sx = cx + 0.02f, sy = cy - 0.03f, sr = rBody + 0.02f;
            glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
            glColor3f(0.03f, 0.03f, 0.04f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx, sy);
            for (int i = 0; i <= segs; ++i)
            {
                float t = (float)i / segs * 2.0f * PI;
                glVertex2f(sx + cosf(t) * sr, sy + sinf(t) * sr);
            }
            glEnd();
        }

        // --- Body with radial gradient (concentric rings, lighter center → darker edge) ---
        {
            const int rings = 12;
            for (int ring = rings - 1; ring >= 0; --ring)
            {
                float t = (float)ring / (float)rings;
                float r = rBody * (t + 0.08f); // slightly offset so center isn't zero-radius
                if (r > rBody)
                    r = rBody;
                // Gradient: center = (0.22, 0.22, 0.26), edge = (0.10, 0.10, 0.12)
                float bright = 0.10f + (1.0f - t) * 0.12f;
                float brightB = 0.12f + (1.0f - t) * 0.14f;
                glColor3f(bright, bright, brightB);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(cx, cy);
                for (int i = 0; i <= segs; ++i)
                {
                    float a = (float)i / segs * 2.0f * PI;
                    glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
                }
                glEnd();
            }
        }

        // --- Specular highlight (top-left crescent) ---
        {
            glColor3f(0.28f, 0.28f, 0.32f);
            float hx = cx - 0.08f, hy = cy + 0.08f, hr = rBody * 0.55f;
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(hx, hy);
            for (int i = 0; i <= 32; ++i)
            {
                float a = (float)i / 32 * 2.0f * PI;
                glVertex2f(hx + cosf(a) * hr, hy + sinf(a) * hr);
            }
            glEnd();
            // Overdraw center to blend
            glColor3f(0.18f, 0.18f, 0.21f);
            float cr2 = rBody * 0.35f;
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= 32; ++i)
            {
                float a = (float)i / 32 * 2.0f * PI;
                glVertex2f(cx + cosf(a) * cr2, cy + sinf(a) * cr2);
            }
            glEnd();
        }

        // --- Track groove (full 270° arc, thin dark line) ---
        {
            glColor3f(0.06f, 0.06f, 0.08f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_STRIP);
            int arcSegs = 48;
            for (int i = 0; i <= arcSegs; ++i)
            {
                float ft = (float)i / arcSegs;
                float deg = startDeg - ft * sweepDeg;
                float rad = deg * PI / 180.0f;
                glVertex2f(cx + cosf(rad) * rTrack, cy + sinf(rad) * rTrack);
            }
            glEnd();
            glLineWidth(1.0f);
        }

        // --- Value arc with glow ---
        if (vNorm > 0.005f)
        {
            // Glow pass (wider, dimmer)
            float glowR = active ? 0.12f : 0.06f;
            float arcR = rTrack;
            glColor3f(0.15f + glowR, 0.45f + glowR, 0.55f + glowR);
            glLineWidth(4.0f);
            glBegin(GL_LINE_STRIP);
            int arcSegs = 48;
            int valSegs = (int)(vNorm * arcSegs);
            if (valSegs < 1)
                valSegs = 1;
            for (int i = 0; i <= valSegs; ++i)
            {
                float ft = (float)i / arcSegs;
                float deg = startDeg - ft * sweepDeg;
                float rad = deg * PI / 180.0f;
                glVertex2f(cx + cosf(rad) * arcR, cy + sinf(rad) * arcR);
            }
            glEnd();

            // Bright pass (narrower, brighter)
            float br = active ? 1.0f : 0.85f;
            glColor3f(0.31f * br, 0.76f * br, 0.97f * br); // material light-blue 300
            glLineWidth(2.5f);
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= valSegs; ++i)
            {
                float ft = (float)i / arcSegs;
                float deg = startDeg - ft * sweepDeg;
                float rad = deg * PI / 180.0f;
                glVertex2f(cx + cosf(rad) * arcR, cy + sinf(rad) * arcR);
            }
            glEnd();
            glLineWidth(1.0f);
        }

        // --- Indicator dot (small filled circle at value position) ---
        {
            float dotR = 0.045f;
            float dotX = cx + cosf(valRad) * rTrack;
            float dotY = cy + sinf(valRad) * rTrack;
            // Dot glow
            glColor3f(0.31f, 0.76f, 0.97f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(dotX, dotY);
            for (int i = 0; i <= 16; ++i)
            {
                float a = (float)i / 16 * 2.0f * PI;
                glVertex2f(dotX + cosf(a) * dotR * 1.6f, dotY + sinf(a) * dotR * 1.6f);
            }
            glEnd();
            // Dot core (white)
            glColor3f(0.95f, 0.95f, 0.98f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(dotX, dotY);
            for (int i = 0; i <= 16; ++i)
            {
                float a = (float)i / 16 * 2.0f * PI;
                glVertex2f(dotX + cosf(a) * dotR, dotY + sinf(a) * dotR);
            }
            glEnd();
        }

        // --- Hover ring ---
        if (hovered && !active)
        {
            glColor3f(0.4f, 0.4f, 0.5f);
            glLineWidth(1.0f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segs; ++i)
            {
                float a = (float)i / segs * 2.0f * PI;
                glVertex2f(cx + cosf(a) * (rBody + 0.01f), cy + sinf(a) * (rBody + 0.01f));
            }
            glEnd();
        }
    }

    void GLRenderer::drawFader(int x, int y, int w, int h, float value, float minValue, float maxValue, bool vertical)
    {
        if (w <= 2 || h <= 2)
            return;
        float vNorm = (maxValue - minValue) > 0.0f ? (value - minValue) / (maxValue - minValue) : 0.0f;
        vNorm = clampf(vNorm, 0.0f, 1.0f);
        applyViewportAndProjection(x, y, w, h, OrthoRect{0, 1, 0, 1});
        // Track
        glColor3f(0.15f, 0.15f, 0.17f);
        glBegin(GL_QUADS);
        glVertex2f(0.25f, 0.05f);
        glVertex2f(0.75f, 0.05f);
        glVertex2f(0.75f, 0.95f);
        glVertex2f(0.25f, 0.95f);
        glEnd();
        // Fill
        glColor3f(0.25f, 0.75f, 0.95f);
        if (vertical)
        {
            glBegin(GL_QUADS);
            glVertex2f(0.26f, 0.05f);
            glVertex2f(0.74f, 0.05f);
            glVertex2f(0.74f, 0.05f + 0.90f * vNorm);
            glVertex2f(0.26f, 0.05f + 0.90f * vNorm);
            glEnd();
        }
        else
        {
            glBegin(GL_QUADS);
            glVertex2f(0.05f, 0.26f);
            glVertex2f(0.05f + 0.90f * vNorm, 0.26f);
            glVertex2f(0.05f + 0.90f * vNorm, 0.74f);
            glVertex2f(0.05f, 0.74f);
            glEnd();
        }
        // Handle
        glColor3f(0.9f, 0.9f, 0.9f);
        if (vertical)
        {
            float cy = 0.05f + 0.90f * vNorm;
            glBegin(GL_QUADS);
            glVertex2f(0.18f, cy - 0.02f);
            glVertex2f(0.82f, cy - 0.02f);
            glVertex2f(0.82f, cy + 0.02f);
            glVertex2f(0.18f, cy + 0.02f);
            glEnd();
        }
        else
        {
            float cx = 0.05f + 0.90f * vNorm;
            glBegin(GL_QUADS);
            glVertex2f(cx - 0.02f, 0.18f);
            glVertex2f(cx + 0.02f, 0.18f);
            glVertex2f(cx + 0.02f, 0.82f);
            glVertex2f(cx - 0.02f, 0.82f);
            glEnd();
        }
        // Border
        glColor3f(0.5f, 0.5f, 0.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
        glVertex2f(1.0f, 1.0f);
        glVertex2f(0.0f, 1.0f);
        glEnd();
    }

    void GLRenderer::drawLabel(int x, int y, int w, int h, const char *text)
    {
        if (!text || !*text || w < 4 || h < 4)
            return;
        applyViewportAndProjection(x, y, w, h, OrthoRect{0, 1, 0, 1});
        glColor3f(0.82f, 0.82f, 0.88f);

        // Compute total string width in Hershey units to scale into [0..1] viewport
        float totalW = hersheyStringWidth(text);
        if (totalW <= 0.0f)
            return;

        // Hershey glyphs: Y range roughly [-4..12], height ~16 units
        constexpr float GLYPH_BOTTOM = -2.0f;
        constexpr float GLYPH_TOP = 13.0f;
        constexpr float GLYPH_HEIGHT = GLYPH_TOP - GLYPH_BOTTOM;

        // Scale to fit viewport [0..1] x [0..1] with small padding
        float padX = 0.02f;
        float padY = 0.05f;
        float scaleX = (1.0f - padX * 2.0f) / totalW;
        float scaleY = (1.0f - padY * 2.0f) / GLYPH_HEIGHT;

        float cursorX = padX; // current pen X in viewport coords

        while (*text)
        {
            HersheyGlyph glyph = getHersheyGlyph(*text);

            if (glyph.num_coords > 0 && glyph.data)
            {
                bool penDown = false;
                glBegin(GL_LINE_STRIP);
                for (int i = 0; i < glyph.num_coords; ++i)
                {
                    int8_t gx = glyph.data[i * 2];
                    int8_t gy = glyph.data[i * 2 + 1];

                    if (gx == PEN_UP && gy == PEN_UP)
                    {
                        // Pen up: end current strip, start new one
                        glEnd();
                        glBegin(GL_LINE_STRIP);
                        penDown = false;
                        continue;
                    }

                    float px = cursorX + (float)gx * scaleX;
                    float py = padY + ((float)gy - GLYPH_BOTTOM) * scaleY;
                    glVertex2f(px, py);
                    penDown = true;
                }
                glEnd();
            }

            cursorX += (float)glyph.width * scaleX;
            ++text;
        }
    }

    void GLRenderer::drawPanelFrame(int x, int y, int w, int h, const char *title, int radius)
    {
        if (w <= 4 || h <= 4)
            return;
        if (radius < 0)
            radius = 0;
        if (radius * 2 > w)
            radius = w / 2;
        if (radius * 2 > h)
            radius = h / 2;
        glViewport(x, y, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, w, 0, h, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glColor3f(0.08f, 0.08f, 0.10f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f((float)w, 0.0f);
        glVertex2f((float)w, (float)h);
        glVertex2f(0.0f, (float)h);
        glEnd();
        int seg = 20;
        glColor3f(0.35f, 0.35f, 0.38f);
        glBegin(GL_LINE_LOOP);
        auto arc = [&](float cx, float cy, float a0, float a1)
        { for(int i=0;i<=seg;i++){ float t=a0 + (a1-a0)*(float)i/seg; float fr = (float)radius; glVertex2f(cx + cosf(t)*fr, cy + sinf(t)*fr);} };
        if (radius > 0)
        {
            // Draw corners clockwise starting at top-left using decreasing angles to orient arcs correctly
            float fr = (float)radius;
            arc(fr, (float)h - fr, 3.14159265f, 3.14159265f * 0.5f);     // TL 180 -> 90
            arc((float)w - fr, (float)h - fr, 3.14159265f * 0.5f, 0.0f); // TR 90 -> 0
            arc((float)w - fr, fr, 0.0f, -3.14159265f * 0.5f);           // BR 0 -> -90
            arc(fr, fr, -3.14159265f * 0.5f, -3.14159265f);              // BL -90 -> -180
        }
        else
        {
            glVertex2f(0.0f, 0.0f);
            glVertex2f((float)w, 0.0f);
            glVertex2f((float)w, (float)h);
            glVertex2f(0.0f, (float)h);
        }
        glEnd();
        if (title && *title)
        {
            int titleLen = (int)std::strlen(title);
            int labelH = 18;
            // Cap width: ~12px per character to avoid stretching
            int labelW = titleLen * 12;
            if (labelW > w - 20)
                labelW = w - 20;
            int lx = x + (w - labelW) / 2;
            int ly = y + h - labelH - 6;
            drawLabel(lx, ly, labelW, labelH, title);
        }
    }

    // --- Helpers ---
    GLRenderer::OrthoRect GLRenderer::computeWaveOrtho(int w, int h) const
    {
        float aspect = (float)w / (float)h;
        OrthoRect r{};
        if (aspect >= 1.0f)
        {
            float yHalf = 0.5f / aspect;
            r.xMin = 0.0f;
            r.xMax = 1.0f;
            r.yMin = -yHalf;
            r.yMax = yHalf;
        }
        else
        {
            float xRange = 1.0f / aspect;
            r.xMin = 0.0f;
            r.xMax = xRange;
            r.yMin = -0.5f;
            r.yMax = 0.5f;
        }
        return r;
    }

    GLRenderer::OrthoRect GLRenderer::computeEnvelopeOrtho(int /*w*/, int /*h*/) const
    {
        // Fixed envelope view: always 0..1 for both axes for clarity
        return OrthoRect{0.0f, 1.0f, 0.0f, 1.0f};
    }

    void GLRenderer::applyViewportAndProjection(int x, int y, int w, int h, const OrthoRect &r)
    {
        glViewport(x, y, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Add a tiny padding so lines drawn exactly on the min edges (left & bottom)
        // are not clipped by rasterization rules, making all four border lines visible.
        float xSpan = r.xMax - r.xMin;
        float ySpan = r.yMax - r.yMin;
        const float padFactor = 0.005f; // slightly larger padding so min edges show
        float padX = xSpan * padFactor;
        float padY = ySpan * padFactor;
        glOrtho(static_cast<GLdouble>(r.xMin - padX), static_cast<GLdouble>(r.xMax + padX),
                static_cast<GLdouble>(r.yMin - padY), static_cast<GLdouble>(r.yMax + padY), -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
    }

} // namespace synth
