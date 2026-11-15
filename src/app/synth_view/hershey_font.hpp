/**
 * @file hershey_font.hpp
 * @brief Hershey Simplex Roman vector font data (public domain)
 *
 * Originally created by Dr. Allen V. Hershey, Naval Weapons Laboratory, 1967.
 * This data is in the public domain.
 *
 * Coordinate system: Y up, baseline at 0, cap height ~12, descenders to ~-4.
 * PEN_UP sentinel separates disconnected strokes.
 */
#pragma once
#include <cstdint>

namespace synth
{

    static constexpr int8_t PEN_UP = -128;

    struct HersheyGlyph
    {
        int8_t width;       // Total advance width
        int8_t num_coords;  // Number of (x,y) pairs (including PEN_UP markers as single entries)
        const int8_t *data; // Flat array: x0,y0, x1,y1, ... or PEN_UP,PEN_UP for pen-up
    };

    // Glyph vertex data (Hershey Simplex Roman subset)
    // Coordinates: x in [-8..8], y in [-4..12], origin at left baseline

    // Space (no vertices - handled by nullptr in glyph lookup)
    static constexpr int8_t g_space[] = {0, 0}; // dummy, not actually drawn

    // !
    static constexpr int8_t g_exclam[] = {
        0, 12, 0, 2, PEN_UP, PEN_UP, 0, 0, 0, 0};

    // - (dash/minus)
    static constexpr int8_t g_dash[] = {
        0, 6, 8, 6};

    // . (period)
    static constexpr int8_t g_period[] = {
        0, 0, 0, 0};

    // / (slash)
    static constexpr int8_t g_slash[] = {
        0, 0, 6, 12};

    // 0
    static constexpr int8_t g_0[] = {
        3, 12, 1, 10, 0, 7, 0, 5, 1, 2, 3, 0, 5, 0, 7, 2, 8, 5, 8, 7, 7, 10, 5, 12, 3, 12};

    // 1
    static constexpr int8_t g_1[] = {
        2, 10, 4, 12, 4, 0};

    // 2
    static constexpr int8_t g_2[] = {
        1, 10, 2, 11, 3, 12, 5, 12, 6, 11, 7, 10, 7, 8, 6, 7, 0, 1, 0, 0, 8, 0};

    // 3
    static constexpr int8_t g_3[] = {
        1, 12, 7, 12, 4, 7, 6, 7, 7, 6, 7, 4, 6, 2, 5, 1, 3, 0, 1, 0, 0, 1};

    // 4
    static constexpr int8_t g_4[] = {
        6, 12, 0, 4, 8, 4, PEN_UP, PEN_UP, 6, 12, 6, 0};

    // 5
    static constexpr int8_t g_5[] = {
        7, 12, 1, 12, 0, 7, 1, 8, 3, 8, 5, 7, 7, 5, 7, 3, 6, 1, 4, 0, 2, 0, 0, 1};

    // 6
    static constexpr int8_t g_6[] = {
        6, 11, 5, 12, 3, 12, 1, 10, 0, 7, 0, 3, 1, 1, 3, 0, 4, 0, 6, 1, 7, 3, 7, 4, 6, 6, 4, 7, 3, 7, 1, 6, 0, 4};

    // 7
    static constexpr int8_t g_7[] = {
        0, 12, 8, 12, 3, 0};

    // 8
    static constexpr int8_t g_8[] = {
        3, 12, 1, 11, 1, 9, 2, 7, 4, 6, 6, 5, 7, 3, 7, 1, 6, 0, 4, 0, 2, 1, 1, 3, 1, 5, 2, 7, 4, 8, 5, 9, 5, 11, 4, 12, 3, 12};

    // 9
    static constexpr int8_t g_9[] = {
        7, 8, 6, 6, 4, 5, 3, 5, 1, 6, 0, 8, 0, 9, 1, 11, 3, 12, 5, 12, 7, 10, 7, 5, 6, 2, 5, 1, 3, 0, 2, 0};

    // A
    static constexpr int8_t g_A[] = {
        0, 0, 4, 12, 8, 0, PEN_UP, PEN_UP, 1, 4, 7, 4};

    // B
    static constexpr int8_t g_B[] = {
        0, 0, 0, 12, 5, 12, 7, 11, 8, 10, 8, 8, 7, 7, 5, 6, PEN_UP, PEN_UP,
        0, 6, 5, 6, 7, 5, 8, 4, 8, 2, 7, 1, 5, 0, 0, 0};

    // C
    static constexpr int8_t g_C[] = {
        8, 10, 7, 11, 5, 12, 3, 12, 1, 11, 0, 9, 0, 3, 1, 1, 3, 0, 5, 0, 7, 1, 8, 2};

    // D
    static constexpr int8_t g_D[] = {
        0, 0, 0, 12, 4, 12, 6, 11, 7, 10, 8, 8, 8, 4, 7, 2, 6, 1, 4, 0, 0, 0};

    // E
    static constexpr int8_t g_E[] = {
        8, 12, 0, 12, 0, 0, 8, 0, PEN_UP, PEN_UP, 0, 6, 5, 6};

    // F
    static constexpr int8_t g_F[] = {
        8, 12, 0, 12, 0, 0, PEN_UP, PEN_UP, 0, 6, 5, 6};

    // G
    static constexpr int8_t g_G[] = {
        8, 10, 7, 11, 5, 12, 3, 12, 1, 11, 0, 9, 0, 3, 1, 1, 3, 0, 5, 0, 7, 1, 8, 3, 8, 6, 5, 6};

    // H
    static constexpr int8_t g_H[] = {
        0, 12, 0, 0, PEN_UP, PEN_UP, 8, 12, 8, 0, PEN_UP, PEN_UP, 0, 6, 8, 6};

    // I
    static constexpr int8_t g_I[] = {
        0, 12, 4, 12, PEN_UP, PEN_UP, 2, 12, 2, 0, PEN_UP, PEN_UP, 0, 0, 4, 0};

    // J
    static constexpr int8_t g_J[] = {
        3, 12, 7, 12, PEN_UP, PEN_UP, 5, 12, 5, 2, 4, 0, 3, 0, 1, 1, 0, 3};

    // K
    static constexpr int8_t g_K[] = {
        0, 12, 0, 0, PEN_UP, PEN_UP, 8, 12, 0, 4, PEN_UP, PEN_UP, 3, 7, 8, 0};

    // L
    static constexpr int8_t g_L[] = {
        0, 12, 0, 0, 7, 0};

    // M
    static constexpr int8_t g_M[] = {
        0, 0, 0, 12, 4, 4, 8, 12, 8, 0};

    // N
    static constexpr int8_t g_N[] = {
        0, 0, 0, 12, 8, 0, 8, 12};

    // O
    static constexpr int8_t g_O[] = {
        3, 12, 1, 11, 0, 9, 0, 3, 1, 1, 3, 0, 5, 0, 7, 1, 8, 3, 8, 9, 7, 11, 5, 12, 3, 12};

    // P
    static constexpr int8_t g_P[] = {
        0, 0, 0, 12, 5, 12, 7, 11, 8, 9, 8, 8, 7, 6, 5, 5, 0, 5};

    // Q
    static constexpr int8_t g_Q[] = {
        3, 12, 1, 11, 0, 9, 0, 3, 1, 1, 3, 0, 5, 0, 7, 1, 8, 3, 8, 9, 7, 11, 5, 12, 3, 12,
        PEN_UP, PEN_UP, 5, 2, 8, -1};

    // R
    static constexpr int8_t g_R[] = {
        0, 0, 0, 12, 5, 12, 7, 11, 8, 9, 8, 8, 7, 6, 5, 5, 0, 5, PEN_UP, PEN_UP, 4, 5, 8, 0};

    // S
    static constexpr int8_t g_S[] = {
        8, 11, 6, 12, 2, 12, 0, 11, 0, 9, 1, 7, 3, 6, 5, 5, 7, 4, 8, 3, 8, 1, 6, 0, 2, 0, 0, 1};

    // T
    static constexpr int8_t g_T[] = {
        0, 12, 8, 12, PEN_UP, PEN_UP, 4, 12, 4, 0};

    // U
    static constexpr int8_t g_U[] = {
        0, 12, 0, 3, 1, 1, 3, 0, 5, 0, 7, 1, 8, 3, 8, 12};

    // V
    static constexpr int8_t g_V[] = {
        0, 12, 4, 0, 8, 12};

    // W
    static constexpr int8_t g_W[] = {
        0, 12, 2, 0, 4, 8, 6, 0, 8, 12};

    // X
    static constexpr int8_t g_X[] = {
        0, 12, 8, 0, PEN_UP, PEN_UP, 8, 12, 0, 0};

    // Y
    static constexpr int8_t g_Y[] = {
        0, 12, 4, 6, 4, 0, PEN_UP, PEN_UP, 8, 12, 4, 6};

    // Z
    static constexpr int8_t g_Z[] = {
        0, 12, 8, 12, 0, 0, 8, 0};

    // _ (underscore)
    static constexpr int8_t g_underscore[] = {
        0, -2, 8, -2};

    // : (colon)
    static constexpr int8_t g_colon[] = {
        0, 8, 0, 8, PEN_UP, PEN_UP, 0, 2, 0, 2};

    // + (plus)
    static constexpr int8_t g_plus[] = {
        0, 6, 8, 6, PEN_UP, PEN_UP, 4, 10, 4, 2};

    // Glyph table indexed by ASCII (32..127)
    static constexpr int HERSHEY_FIRST_CHAR = 32;
    static constexpr int HERSHEY_LAST_CHAR = 95; // Up to '_'

    // Helper: get glyph for a character
    inline HersheyGlyph getHersheyGlyph(char c)
    {
        // Convert lowercase to uppercase
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);

        // Default: empty glyph (space)
        HersheyGlyph g{8, 0, nullptr};

        switch (c)
        {
        case ' ':
            g = {8, 0, nullptr};
            break;
        case '!':
            g = {4, (int8_t)(sizeof(g_exclam) / 2), g_exclam};
            break;
        case '+':
            g = {9, (int8_t)(sizeof(g_plus) / 2), g_plus};
            break;
        case '-':
            g = {9, (int8_t)(sizeof(g_dash) / 2), g_dash};
            break;
        case '.':
            g = {4, (int8_t)(sizeof(g_period) / 2), g_period};
            break;
        case '/':
            g = {7, (int8_t)(sizeof(g_slash) / 2), g_slash};
            break;
        case ':':
            g = {4, (int8_t)(sizeof(g_colon) / 2), g_colon};
            break;
        case '0':
            g = {9, (int8_t)(sizeof(g_0) / 2), g_0};
            break;
        case '1':
            g = {5, (int8_t)(sizeof(g_1) / 2), g_1};
            break;
        case '2':
            g = {9, (int8_t)(sizeof(g_2) / 2), g_2};
            break;
        case '3':
            g = {9, (int8_t)(sizeof(g_3) / 2), g_3};
            break;
        case '4':
            g = {9, (int8_t)(sizeof(g_4) / 2), g_4};
            break;
        case '5':
            g = {9, (int8_t)(sizeof(g_5) / 2), g_5};
            break;
        case '6':
            g = {9, (int8_t)(sizeof(g_6) / 2), g_6};
            break;
        case '7':
            g = {9, (int8_t)(sizeof(g_7) / 2), g_7};
            break;
        case '8':
            g = {9, (int8_t)(sizeof(g_8) / 2), g_8};
            break;
        case '9':
            g = {9, (int8_t)(sizeof(g_9) / 2), g_9};
            break;
        case 'A':
            g = {9, (int8_t)(sizeof(g_A) / 2), g_A};
            break;
        case 'B':
            g = {9, (int8_t)(sizeof(g_B) / 2), g_B};
            break;
        case 'C':
            g = {9, (int8_t)(sizeof(g_C) / 2), g_C};
            break;
        case 'D':
            g = {9, (int8_t)(sizeof(g_D) / 2), g_D};
            break;
        case 'E':
            g = {9, (int8_t)(sizeof(g_E) / 2), g_E};
            break;
        case 'F':
            g = {9, (int8_t)(sizeof(g_F) / 2), g_F};
            break;
        case 'G':
            g = {9, (int8_t)(sizeof(g_G) / 2), g_G};
            break;
        case 'H':
            g = {9, (int8_t)(sizeof(g_H) / 2), g_H};
            break;
        case 'I':
            g = {5, (int8_t)(sizeof(g_I) / 2), g_I};
            break;
        case 'J':
            g = {8, (int8_t)(sizeof(g_J) / 2), g_J};
            break;
        case 'K':
            g = {9, (int8_t)(sizeof(g_K) / 2), g_K};
            break;
        case 'L':
            g = {8, (int8_t)(sizeof(g_L) / 2), g_L};
            break;
        case 'M':
            g = {9, (int8_t)(sizeof(g_M) / 2), g_M};
            break;
        case 'N':
            g = {9, (int8_t)(sizeof(g_N) / 2), g_N};
            break;
        case 'O':
            g = {9, (int8_t)(sizeof(g_O) / 2), g_O};
            break;
        case 'P':
            g = {9, (int8_t)(sizeof(g_P) / 2), g_P};
            break;
        case 'Q':
            g = {9, (int8_t)(sizeof(g_Q) / 2), g_Q};
            break;
        case 'R':
            g = {9, (int8_t)(sizeof(g_R) / 2), g_R};
            break;
        case 'S':
            g = {9, (int8_t)(sizeof(g_S) / 2), g_S};
            break;
        case 'T':
            g = {9, (int8_t)(sizeof(g_T) / 2), g_T};
            break;
        case 'U':
            g = {9, (int8_t)(sizeof(g_U) / 2), g_U};
            break;
        case 'V':
            g = {9, (int8_t)(sizeof(g_V) / 2), g_V};
            break;
        case 'W':
            g = {9, (int8_t)(sizeof(g_W) / 2), g_W};
            break;
        case 'X':
            g = {9, (int8_t)(sizeof(g_X) / 2), g_X};
            break;
        case 'Y':
            g = {9, (int8_t)(sizeof(g_Y) / 2), g_Y};
            break;
        case 'Z':
            g = {9, (int8_t)(sizeof(g_Z) / 2), g_Z};
            break;
        case '_':
            g = {9, (int8_t)(sizeof(g_underscore) / 2), g_underscore};
            break;
        default:
            g = {6, 0, nullptr};
            break;
        }
        return g;
    }

    // Compute total advance width for a string
    inline float hersheyStringWidth(const char *text)
    {
        float w = 0.0f;
        while (*text)
        {
            HersheyGlyph g = getHersheyGlyph(*text);
            w += (float)g.width;
            ++text;
        }
        return w;
    }

} // namespace synth
