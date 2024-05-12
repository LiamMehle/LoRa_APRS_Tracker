#include <Arduino.h>

#define START_BENCH auto const t0 = micros();
#define STOP_BENCH(what)                                  \
    {                                                     \
        auto const t1 = micros();                         \
        char buffer[22];                                  \
        auto const delta = t1-t0;                         \
        snprintf(buffer, sizeof(buffer), "%s:%4lu.%-4lu", \
            what, delta/1000, delta%1000);                \
        show_display(buffer);                             \
    }

// a miracle has happened: Sheepy wrote a class

// // the impossible has happened: I'm writing a class, almost
template<unsigned N>
class PerfTimer {
    unsigned t0;
    unsigned deltas[N];
    uint8_t offset = 0;
public:
    void start() {
        t0 = micros();
    }
    unsigned stop() {
        auto const t1 = micros();
        deltas[offset++] = t1 - t0;
        offset -= offset < N ? 0 : N;
        return t1;
    }
    template <typename T>
    void measure(T f) {
        for (unsigned i=0; i<N; i++) {
            this->start();
            f();
            this->stop();
        }
    }
    float average() {
        unsigned sum = 0;
        for (unsigned i=0; i<N; i++)
            sum += deltas[i];
        float average = static_cast<float>(sum) / static_cast<float>(N);
        return average;
    }
    float std_dev() {
        // sum of squares of deviations from mean
        auto const average = this->average();
        unsigned sum_of_squares = 0;
        for (unsigned i=0; i<N; i++) {
            auto const deviation = abs(static_cast<float>(deltas[i]) - average);
            sum_of_squares += static_cast<unsigned>(deviation);
        }
        return sum_of_squares/N;
    }
    float range() {
        unsigned min_element = deltas[0];
        for (unsigned i=1; i<N; i++)
            min_element = min(min_element, deltas[i]);
        unsigned max_element = deltas[0];
        for (unsigned i=1; i<N; i++)
            max_element = min(max_element, deltas[i]);
        return max_element;
    }
};
