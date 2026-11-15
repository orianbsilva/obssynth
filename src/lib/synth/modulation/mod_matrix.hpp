/**
 * @file mod_matrix.hpp
 * @brief Generic modulation matrix routing
 *
 * Routes modulation sources to destinations with adjustable amounts.
 * Uses integer IDs - product-specific enums cast to int at call site.
 * Products: ObsSynth, ObsDrum, ObsBass each define their own enums.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstddef>
#include <array>
#include <cmath>
#include <algorithm>
#include <functional>

namespace synth
{

    constexpr int MAX_MOD_SLOTS = 32;
    constexpr int MAX_MOD_SOURCES = 64;
    constexpr int MAX_MOD_DESTS = 256;

    /**
     * @brief A single modulation routing slot (generic, int-based)
     */
    struct ModSlot
    {
        int source;      ///< Source ID (product-defined enum cast to int)
        int destination; ///< Destination ID (product-defined enum cast to int)
        float amount;    ///< Amount (-2.0 to +2.0)
        bool bipolar;    ///< true = -1 to +1, false = 0 to +1
        bool enabled;    ///< Slot active flag

        ModSlot()
            : source(0), destination(0), amount(0.0f), bipolar(true), enabled(false)
        {
        }

        ModSlot(int src, int dst, float amt, bool bi = true)
            : source(src), destination(dst), amount(amt), bipolar(bi), enabled(true)
        {
        }
    };

    /**
     * @brief Generic modulation matrix
     *
     * Product-agnostic routing system. Each product (ObsSynth, ObsDrum,
     * ObsBass) defines its own source/destination enums and casts to int.
     *
     * @code
     * // Product code:
     * matrix.setSlot(0, (int)ObsSynthSrc::LFO1, (int)ObsSynthDst::CUTOFF, 0.5f);
     * matrix.setSourceValue((int)ObsSynthSrc::LFO1, lfoValue);
     * matrix.process();
     * float mod = matrix.getModValue((int)ObsSynthDst::CUTOFF);
     * @endcode
     */
    class ModMatrix
    {
    private:
        std::array<ModSlot, MAX_MOD_SLOTS> slots_;
        int active_slots_;

        std::array<float, MAX_MOD_SOURCES> source_values_;
        std::array<float, MAX_MOD_DESTS> dest_values_;

        // Product sets this to handle bipolar→unipolar conversion
        std::function<bool(int)> is_bipolar_fn_;

    public:
        ModMatrix()
            : active_slots_(0), is_bipolar_fn_(nullptr)
        {
            source_values_.fill(0.0f);
            dest_values_.fill(0.0f);
        }

        /**
         * @brief Set the bipolar check function (product-specific)
         * @param fn Returns true if the given source ID is bipolar (-1..+1)
         */
        void setBipolarCheck(std::function<bool(int)> fn)
        {
            is_bipolar_fn_ = std::move(fn);
        }

        void clear()
        {
            for (auto &slot : slots_)
            {
                slot.enabled = false;
                slot.amount = 0.0f;
            }
            active_slots_ = 0;
            dest_values_.fill(0.0f);
        }

        bool setSlot(int index, int source, int dest, float amount, bool bipolar = true)
        {
            if (index < 0 || index >= MAX_MOD_SLOTS)
                return false;

            slots_[index].source = source;
            slots_[index].destination = dest;
            slots_[index].amount = std::clamp(amount, -2.0f, 2.0f);
            slots_[index].bipolar = bipolar;
            slots_[index].enabled = (amount != 0.0f);

            updateActiveSlotCount();
            return true;
        }

        void setSlotEnabled(int index, bool enabled)
        {
            if (index >= 0 && index < MAX_MOD_SLOTS)
            {
                slots_[index].enabled = enabled;
                updateActiveSlotCount();
            }
        }

        void setSlotAmount(int index, float amount)
        {
            if (index >= 0 && index < MAX_MOD_SLOTS)
            {
                slots_[index].amount = std::clamp(amount, -2.0f, 2.0f); // (enabled as it was: a muted route stays muted)
                updateActiveSlotCount();
            }
        }

        const ModSlot *getSlot(int index) const
        {
            if (index >= 0 && index < MAX_MOD_SLOTS)
                return &slots_[index];
            return nullptr;
        }

        int getActiveSlotCount() const { return active_slots_; }

        void setSourceValue(int source_id, float value)
        {
            if (source_id >= 0 && source_id < MAX_MOD_SOURCES)
                source_values_[source_id] = value;
        }

        float getSourceValue(int source_id) const
        {
            if (source_id >= 0 && source_id < MAX_MOD_SOURCES)
                return source_values_[source_id];
            return 0.0f;
        }

        void process()
        {
            dest_values_.fill(0.0f);

            for (const auto &slot : slots_)
            {
                if (!slot.enabled || slot.amount == 0.0f)
                    continue;
                if (slot.source < 0 || slot.source >= MAX_MOD_SOURCES)
                    continue;
                if (slot.destination < 0 || slot.destination >= MAX_MOD_DESTS)
                    continue;

                float src_value = source_values_[slot.source];

                // Convert bipolar source to unipolar if slot requests unipolar
                if (!slot.bipolar && is_bipolar_fn_ && is_bipolar_fn_(slot.source))
                {
                    src_value = (src_value + 1.0f) * 0.5f;
                }

                dest_values_[slot.destination] += src_value * slot.amount;
            }
        }

        const float *sourceValues() const { return source_values_.data(); }

        /// @brief The slots on sources given (a voice's: its own velocity, envelopes...), into dests[0..limit):
        /// the destinations below limit only (a voice's own); process() keeps the instrument's
        void evaluate(const float *sources, float *dests, int limit) const
        {
            std::fill(dests, dests + limit, 0.0f);
            for (const auto &slot : slots_)
            {
                if (!slot.enabled || slot.amount == 0.0f || slot.destination < 0 || slot.destination >= limit)
                    continue;
                if (slot.source < 0 || slot.source >= MAX_MOD_SOURCES)
                    continue;
                float v = sources[slot.source];
                if (!slot.bipolar && is_bipolar_fn_ && is_bipolar_fn_(slot.source))
                    v = (v + 1.0f) * 0.5f;
                dests[slot.destination] += v * slot.amount;
            }
        }

        float getModValue(int dest_id) const
        {
            if (dest_id >= 0 && dest_id < MAX_MOD_DESTS)
                return dest_values_[dest_id];
            return 0.0f;
        }

        float getModulatedValue(int dest_id, float base_value, float mod_range) const
        {
            return base_value + getModValue(dest_id) * mod_range;
        }

        int findFreeSlot() const
        {
            // free: off and no amount (a muted route keeps its slot: it was handed to the next route)
            for (int i = 0; i < MAX_MOD_SLOTS; ++i)
            {
                if (!slots_[i].enabled && slots_[i].amount == 0.0f)
                    return i;
            }
            return -1;
        }

        /// @brief Whether an enabled route reaches a destination
        bool routesTo(int dest_id) const
        {
            for (const auto &s : slots_)
                if (s.enabled && s.amount != 0.0f && s.destination == dest_id)
                    return true;
            return false;
        }

        /// @brief A destination's value without what one source brings it (an LFO's depth without itself)
        float getModValueWithout(int dest_id, int source_id) const
        {
            float v = getModValue(dest_id);
            for (const auto &s : slots_)
                if (s.enabled && s.destination == dest_id && s.source == source_id && source_id >= 0 && source_id < MAX_MOD_SOURCES)
                {
                    float x = source_values_[static_cast<size_t>(source_id)];
                    if (!s.bipolar && is_bipolar_fn_ && is_bipolar_fn_(source_id))
                        x = (x + 1.0f) * 0.5f;
                    v -= x * s.amount;
                }
            return v;
        }

        int addRoute(int source, int dest, float amount, bool bipolar = true)
        {
            int slot = findFreeSlot();
            if (slot >= 0)
                setSlot(slot, source, dest, amount, bipolar);
            return slot;
        }

        void removeRoute(int index)
        {
            if (index >= 0 && index < MAX_MOD_SLOTS)
            {
                slots_[index].enabled = false;
                slots_[index].amount = 0.0f;
                updateActiveSlotCount();
            }
        }

        const std::array<ModSlot, MAX_MOD_SLOTS> &getSlots() const { return slots_; }

    private:
        void updateActiveSlotCount()
        {
            active_slots_ = 0;
            for (const auto &slot : slots_)
            {
                if (slot.enabled && slot.amount != 0.0f)
                    ++active_slots_;
            }
        }
    };

} // namespace synth
