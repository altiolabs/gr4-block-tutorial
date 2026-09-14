#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/Gain.hpp>

int main() {
    using namespace boost::ut;
    "float gain doubles samples"_test = [] {
        gr::tutorial::Gain gain;
        expect(eq(gain.processOne(1.0f), 2.0f));
        expect(eq(gain.processOne(-2.0f), -4.0f));
        expect(eq(gain.processOne(3.5f), 7.0f));
    };
}
