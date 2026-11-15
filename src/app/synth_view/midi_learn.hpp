#pragma once
#include <array>
#include <cstdint>

namespace synth
{

    /// Identifies a mappable parameter: either per-OSC or global.
    struct MidiLearnTarget
    {
        int oscIdx;   ///< -1 = global, 0-3 = per-oscillator
        int paramIdx; ///< OscParam or GlobalParam enum value

        bool operator==(const MidiLearnTarget &o) const
        {
            return oscIdx == o.oscIdx && paramIdx == o.paramIdx;
        }
        bool isValid() const { return paramIdx >= 0; }
    };

    /// A single CC → Parameter mapping entry.
    struct MidiCCMapping
    {
        uint8_t cc;             ///< MIDI CC number (0-127)
        MidiLearnTarget target; ///< Target parameter
        bool active;            ///< Is this slot used?
    };

    /// Stores up to MAX_MAPPINGS MIDI CC → Parameter assignments.
    /// Supports learn mode and runtime lookup.
    class MidiLearnMap
    {
    public:
        static constexpr int MAX_MAPPINGS = 64;

        MidiLearnMap() { clear(); }

        /// Remove all mappings.
        void clear()
        {
            for (auto &m : mappings_)
                m.active = false;
            count_ = 0;
        }

        /// Add or replace a mapping for a given CC.
        /// If the CC already has a mapping, it is replaced.
        /// If the target already has a mapping from another CC, the old one is removed.
        void assign(uint8_t cc, MidiLearnTarget target)
        {
            // Remove existing mapping for this target (avoid duplicates)
            for (auto &m : mappings_)
            {
                if (m.active && m.target == target)
                {
                    m.active = false;
                    --count_;
                }
            }
            // Check if CC already mapped — replace
            for (auto &m : mappings_)
            {
                if (m.active && m.cc == cc)
                {
                    m.target = target;
                    return;
                }
            }
            // Find free slot
            for (auto &m : mappings_)
            {
                if (!m.active)
                {
                    m.cc = cc;
                    m.target = target;
                    m.active = true;
                    ++count_;
                    return;
                }
            }
        }

        /// Remove mapping for a specific CC.
        void unassign(uint8_t cc)
        {
            for (auto &m : mappings_)
            {
                if (m.active && m.cc == cc)
                {
                    m.active = false;
                    --count_;
                    return;
                }
            }
        }

        /// Remove mapping for a specific target.
        void unassignTarget(MidiLearnTarget target)
        {
            for (auto &m : mappings_)
            {
                if (m.active && m.target == target)
                {
                    m.active = false;
                    --count_;
                    return;
                }
            }
        }

        /// Find the target for a given CC. Returns invalid target if not mapped.
        MidiLearnTarget findByCC(uint8_t cc) const
        {
            for (const auto &m : mappings_)
            {
                if (m.active && m.cc == cc)
                    return m.target;
            }
            return {-1, -1};
        }

        /// Find the CC assigned to a target. Returns -1 if not mapped.
        int findByTarget(MidiLearnTarget target) const
        {
            for (const auto &m : mappings_)
            {
                if (m.active && m.target == target)
                    return m.cc;
            }
            return -1;
        }

        int getCount() const { return count_; }

        /// Access all mappings (for serialization/display).
        const std::array<MidiCCMapping, MAX_MAPPINGS> &getMappings() const { return mappings_; }

    private:
        std::array<MidiCCMapping, MAX_MAPPINGS> mappings_;
        int count_{0};
    };

} // namespace synth
