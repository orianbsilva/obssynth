#pragma once
#include <vector>

namespace synth
{
    struct Point
    {
        float x;
        float y;
    };
    class GLRenderer
    {
    public:
        void begin(int w, int h);
        // Draw waveform in rectangle (x,y,w,h) in window coordinates (bottom-left origin per OpenGL viewport)
        void drawWave(int x, int y, int w, int h, const std::vector<Point> &wave);
        // Draw ADSR envelope in rectangle (x,y,w,h)
        void drawEnvelope(int x, int y, int w, int h, const std::vector<Point> &env);
        // Draw a rotary knob (value in [minValue,maxValue]) inside square viewport of size 'size'
        void drawKnob(int x, int y, int size, float value, float minValue, float maxValue);
        // Draw a material-design rotary knob with shadow, gradient body, glow arc, and dot indicator
        void drawMaterialKnob(int x, int y, int size, float value, float minValue, float maxValue, bool active = false, bool hovered = false);
        // Draw a linear fader (vertical if vertical=true, else horizontal)
        void drawFader(int x, int y, int w, int h, float value, float minValue, float maxValue, bool vertical = true);
        // Draw a simple placeholder text label inside rectangle (x,y,w,h)
        void drawLabel(int x, int y, int w, int h, const char *text);
        // Draw a framed panel background (dark fill + border)
        void drawPanelFrame(int x, int y, int w, int h, const char *title, int radius = 10);

        void end();

    private:
        // Helper to draw a grid and border given orthographic extents (already set in projection)
        void drawGridAndBorder(float xMin, float xMax, float yMin, float yMax, int divisions = 10);
        struct OrthoRect
        {
            float xMin;
            float xMax;
            float yMin;
            float yMax;
        };
        OrthoRect computeWaveOrtho(int w, int h) const;
        OrthoRect computeEnvelopeOrtho(int w, int h) const;
        void applyViewportAndProjection(int x, int y, int w, int h, const OrthoRect &r);
        float clampf(float v, float lo, float hi) const { return v < lo ? lo : (v > hi ? hi : v); }
    };
}
