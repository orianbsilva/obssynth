/**
 * @file mod_matrix_grid.hpp
 * @brief Visual grid representation of the ModMatrix for synth_view
 *
 * Maps the ModMatrix slot list into a 2D grid (sources × destinations)
 * for rendering as an interactive matrix display.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "../../products/obs_synth/obs_synth_mod_source.hpp"
#include "../../products/obs_synth/obs_synth_mod_dest.hpp"
#include "../lib/synth/modulation/mod_matrix.hpp"
#include <array>
#include <cstdint>

namespace synth
{

    // ========================================================================
    // Grid dimensions
    // ========================================================================

    // Visible destination columns (condensed from full enum)
    // Per-osc: Pitch, Cutoff, Reso, Level, FX_P1, FX_P2 (6 params × 4 oscs = 24)
    // Mixer: 4 channel levels
    // MasterFX: Mix, P1, P2
    // LFOs: Rate1, Rate2
    static constexpr int GRID_COLS_PER_OSC = 6;
    static constexpr int GRID_OSC_COUNT = 4;
    static constexpr int GRID_MIXER_COLS = 4;
    static constexpr int GRID_MFX_COLS = 3;
    static constexpr int GRID_TUBE_COLS = 3;
    static constexpr int GRID_LFO_COLS = 2;

    static constexpr int GRID_TOTAL_COLS =
        GRID_COLS_PER_OSC * GRID_OSC_COUNT + // 24
        GRID_MIXER_COLS +                    // 4
        GRID_MFX_COLS +                      // 3
        GRID_TUBE_COLS +                     // 3
        GRID_LFO_COLS;                       // 2  = 36 total

    // Source rows (all ObsSynthSrc values we display)
    static constexpr int GRID_TOTAL_ROWS = 21; // LFO1-4, FEnv1, ModEnv1-3, XY_X/Y, Vel, AfterT, ModWh, PBend, Expr, KeyTrk, Rnd, CC_A-D

    // ========================================================================
    // Row definitions (source → row mapping)
    // ========================================================================

    struct GridRow
    {
        const char *label;  // Short display name
        ObsSynthSrc src_id; // Source enum value
    };

    static constexpr GridRow GRID_ROWS[GRID_TOTAL_ROWS] = {
        {"LFO1", ObsSynthSrc::LFO1},
        {"LFO2", ObsSynthSrc::LFO2},
        {"LFO3", ObsSynthSrc::LFO3},
        {"LFO4", ObsSynthSrc::LFO4},
        {"FEnv1", ObsSynthSrc::FILTER_ENV1},
        {"ME1", ObsSynthSrc::MOD_ENV},
        {"ME2", ObsSynthSrc::MOD_ENV2},
        {"ME3", ObsSynthSrc::MOD_ENV3},
        {"XY-X", ObsSynthSrc::XY_X},
        {"XY-Y", ObsSynthSrc::XY_Y},
        {"Vel", ObsSynthSrc::VELOCITY},
        {"AftT", ObsSynthSrc::AFTERTOUCH},
        {"MdWh", ObsSynthSrc::MOD_WHEEL},
        {"PBnd", ObsSynthSrc::PITCH_BEND},
        {"Expr", ObsSynthSrc::EXPRESSION},
        {"Key", ObsSynthSrc::KEYTRACK},
        {"Rnd", ObsSynthSrc::RANDOM},
        {"CcA", ObsSynthSrc::CC_A},
        {"CcB", ObsSynthSrc::CC_B},
        {"CcC", ObsSynthSrc::CC_C},
        {"CcD", ObsSynthSrc::CC_D},
    };

    // ========================================================================
    // Column definitions (dest → column mapping)
    // ========================================================================

    struct GridCol
    {
        const char *label;  // Short display name (1-3 chars)
        ObsSynthDst dst_id; // Destination enum value
    };

    // Column group header
    struct GridColGroup
    {
        const char *label; // Group name (e.g. "Osc1", "Mix")
        int start_col;     // First column index in group
        int col_count;     // Number of columns in group
    };

    // Per-osc columns: Pitch, Cutoff, Reso, Level
    static constexpr int OSC_PARAM_PITCH = 0;
    static constexpr int OSC_PARAM_CUTOFF = 7;
    static constexpr int OSC_PARAM_RESO = 8;
    static constexpr int OSC_PARAM_LEVEL = 3;

    // All 36 destination columns
    static constexpr GridCol GRID_COLS[GRID_TOTAL_COLS] = {
        // Osc1 (cols 0-5)
        {"P", ObsSynthDst::OSC1_PITCH},
        {"C", ObsSynthDst::OSC1_CUTOFF},
        {"R", ObsSynthDst::OSC1_RESO},
        {"L", ObsSynthDst::OSC1_LEVEL},
        {"1", ObsSynthDst::OSC1_FX_P1},
        {"2", ObsSynthDst::OSC1_FX_P2},
        // Osc2 (cols 6-11)
        {"P", ObsSynthDst::OSC2_PITCH},
        {"C", ObsSynthDst::OSC2_CUTOFF},
        {"R", ObsSynthDst::OSC2_RESO},
        {"L", ObsSynthDst::OSC2_LEVEL},
        {"1", ObsSynthDst::OSC2_FX_P1},
        {"2", ObsSynthDst::OSC2_FX_P2},
        // Osc3 (cols 12-17)
        {"P", ObsSynthDst::OSC3_PITCH},
        {"C", ObsSynthDst::OSC3_CUTOFF},
        {"R", ObsSynthDst::OSC3_RESO},
        {"L", ObsSynthDst::OSC3_LEVEL},
        {"1", ObsSynthDst::OSC3_FX_P1},
        {"2", ObsSynthDst::OSC3_FX_P2},
        // Osc4 (cols 18-23)
        {"P", ObsSynthDst::OSC4_PITCH},
        {"C", ObsSynthDst::OSC4_CUTOFF},
        {"R", ObsSynthDst::OSC4_RESO},
        {"L", ObsSynthDst::OSC4_LEVEL},
        {"1", ObsSynthDst::OSC4_FX_P1},
        {"2", ObsSynthDst::OSC4_FX_P2},
        // LFO rates (cols 24-25)
        {"R1", ObsSynthDst::LFO1_RATE},
        {"R2", ObsSynthDst::LFO2_RATE},
        // Mixer (cols 26-29)
        {"1", ObsSynthDst::MIX_CH1_LVL},
        {"2", ObsSynthDst::MIX_CH2_LVL},
        {"3", ObsSynthDst::MIX_CH3_LVL},
        {"4", ObsSynthDst::MIX_CH4_LVL},
        // MasterFX (cols 30-32)
        {"M", ObsSynthDst::MFX_MIX},
        {"P1", ObsSynthDst::MFX_P1},
        {"P2", ObsSynthDst::MFX_P2},
        // Tube WaveShaper (cols 33-35)
        {"Dr", ObsSynthDst::TUBE_DRIVE},
        {"As", ObsSynthDst::TUBE_ASYM},
        {"Ch", ObsSynthDst::TUBE_CHAOS},
    };

    // Group headers for rendering separators
    static constexpr int GRID_NUM_GROUPS = 8;
    static constexpr GridColGroup GRID_GROUPS[GRID_NUM_GROUPS] = {
        {"Osc1", 0, 6},
        {"Osc2", 6, 6},
        {"Osc3", 12, 6},
        {"Osc4", 18, 6},
        {"LFO", 24, 2},
        {"Mix", 26, 4},
        {"MFX", 30, 3},
        {"Tube", 33, 3},
    };

    // ========================================================================
    // Grid cell state
    // ========================================================================

    struct GridCell
    {
        float amount; // Configured amount (-2.0 to +2.0)
        float value;  // Real-time effective value (source × amount)
        bool enabled; // Slot is active
    };

    // ========================================================================
    // ModMatrixGrid - builds the 2D view from the slot list
    // ========================================================================

    class ModMatrixGrid
    {
    public:
        static constexpr int ROWS = GRID_TOTAL_ROWS;
        static constexpr int COLS = GRID_TOTAL_COLS;

    private:
        std::array<std::array<GridCell, COLS>, ROWS> cells_{};

        // Reverse lookup: find grid position for a (source, dest) pair
        int findRow(int src_id) const
        {
            for (int r = 0; r < ROWS; ++r)
            {
                if (static_cast<int>(GRID_ROWS[r].src_id) == src_id)
                    return r;
            }
            return -1;
        }

        int findCol(int dst_id) const
        {
            for (int c = 0; c < COLS; ++c)
            {
                if (static_cast<int>(GRID_COLS[c].dst_id) == dst_id)
                    return c;
            }
            return -1;
        }

    public:
        ModMatrixGrid()
        {
            clear();
        }

        void clear()
        {
            for (auto &row : cells_)
                for (auto &cell : row)
                {
                    cell.amount = 0.0f;
                    cell.value = 0.0f;
                    cell.enabled = false;
                }
        }

        /**
         * @brief Rebuild grid state from the ModMatrix slot list
         *
         * Reads both configured amounts AND current source values
         * to compute real-time effective modulation per cell.
         */
        void syncFromMatrix(const ModMatrix &matrix)
        {
            clear();

            for (int i = 0; i < MAX_MOD_SLOTS; ++i)
            {
                const ModSlot *slot = matrix.getSlot(i);
                if (!slot || !slot->enabled)
                    continue;

                int row = findRow(slot->source);
                int col = findCol(slot->destination);

                if (row >= 0 && col >= 0)
                {
                    float src_val = matrix.getSourceValue(slot->source);
                    cells_[row][col].amount = slot->amount;
                    cells_[row][col].value = src_val * slot->amount;
                    cells_[row][col].enabled = true;
                }
            }
        }

        /**
         * @brief Get cell at grid position
         */
        const GridCell &getCell(int row, int col) const
        {
            return cells_[row][col];
        }

        /**
         * @brief Set amount at grid position (updates visual state)
         * @return The (source_id, dest_id) pair for this cell
         */
        struct CellAddress
        {
            int source_id;
            int dest_id;
        };

        CellAddress getCellAddress(int row, int col) const
        {
            return {
                static_cast<int>(GRID_ROWS[row].src_id),
                static_cast<int>(GRID_COLS[col].dst_id)};
        }

        // Row/col info accessors
        const char *getRowLabel(int row) const { return GRID_ROWS[row].label; }
        const char *getColLabel(int col) const { return GRID_COLS[col].label; }
        int getNumGroups() const { return GRID_NUM_GROUPS; }
        const GridColGroup &getGroup(int idx) const { return GRID_GROUPS[idx]; }
    };

} // namespace synth
