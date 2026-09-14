#include <gnuradio-4.0/tutorial/Gain.hpp>
int main() {
    gr::tutorial::Gain<float> gain;
    return gain.processOne(3.5f) == 7.0f ? 0 : 1;
}
