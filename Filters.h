#pragma once

#include <Arduino.h>

namespace Filters {

    // First-Order Exponential Moving Average (EMA)
    class ExponentialMovingAverage {
    public:
        explicit ExponentialMovingAverage(float alpha)
            : m_alpha(alpha), m_initialized(false), m_value(0.0f) {}

        void clear() {
            m_initialized = false;
            m_value = 0.0f;
        }

        float update(float input) {
            if (!m_initialized) {
                m_value = input;
                m_initialized = true;
            } else {
                m_value = m_alpha * input + (1.0f - m_alpha) * m_value;
            }
            return m_value;
        }

        float getValue() const { return m_value; }

    private:
        float m_alpha;
        bool m_initialized;
        float m_value;
    };

    // Median Filter for spike suppression
    template <typename T, size_t N>
    class MedianFilter {
    public:
        MedianFilter() { clear(); }

        void clear() {
            m_index = 0;
            m_count = 0;
            for (size_t i = 0; i < N; ++i) {
                m_buffer[i] = 0;
            }
        }

        T update(T input) {
            m_buffer[m_index] = input;
            m_index = (m_index + 1) % N;
            if (m_count < N) m_count++;

            T sorted[N];
            for (size_t i = 0; i < m_count; ++i) {
                sorted[i] = m_buffer[i];
            }

            // Insertion sort for small window N
            for (size_t i = 1; i < m_count; ++i) {
                T key = sorted[i];
                int j = static_cast<int>(i) - 1;
                while (j >= 0 && sorted[j] > key) {
                    sorted[j + 1] = sorted[j];
                    j--;
                }
                sorted[j + 1] = key;
            }

            return sorted[m_count / 2];
        }

    private:
        T m_buffer[N];
        size_t m_index;
        size_t m_count;
    };

} // namespace Filters
