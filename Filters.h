#pragma once

#include <Arduino.h>

namespace Filters {

    // Template Moving Average Filter
    template <typename T, size_t N>
    class MovingAverage {
    public:
        MovingAverage() { clear(); }

        void clear() {
            m_sum = 0;
            m_index = 0;
            m_count = 0;
            for (size_t i = 0; i < N; ++i) {
                m_buffer[i] = 0;
            }
        }

        T update(T value) {
            m_sum -= m_buffer[m_index];
            m_buffer[m_index] = value;
            m_sum += value;
            m_index = (m_index + 1) % N;
            if (m_count < N) {
                m_count++;
            }
            return m_sum / m_count;
        }

    private:
        T m_buffer[N];
        uint32_t m_sum;
        size_t m_index;
        size_t m_count;
    };

    // First-Order Exponential Moving Average (EMA) Filter
    // y[n] = Alpha * x[n] + (1 - Alpha) * y[n-1]
    class ExponentialMovingAverage {
    public:
        ExponentialMovingAverage(float alpha) : m_alpha(alpha), m_initialized(false), m_value(0.0f) {}

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

    // Median Filter (Window size 3 or 5) for removing high-amplitude impulse spikes
    template <typename T, size_t N>
    class MedianFilter {
    public:
        MedianFilter() {
            clear();
        }

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

            // Copy and sort buffer
            T sorted[N];
            memcpy(sorted, m_buffer, m_count * sizeof(T));
            
            // Basic bubble sort for tiny windows (3 or 5 items)
            for (size_t i = 0; i < m_count; ++i) {
                for (size_t j = i + 1; j < m_count; ++j) {
                    if (sorted[i] > sorted[j]) {
                        T temp = sorted[i];
                        sorted[i] = sorted[j];
                        sorted[j] = temp;
                    }
                }
            }

            return sorted[m_count / 2];
        }

    private:
        T m_buffer[N];
        size_t m_index;
        size_t m_count;
    };
}
