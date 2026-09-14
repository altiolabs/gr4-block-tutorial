#include "ExpectedBlocks.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/GrTutorialBlocks.hpp>
int main() {
    using namespace boost::ut;
    "all tutorial registrations instantiate"_test = [] {
        auto& registry = gr::globalBlockRegistry();
        gr::blocklib::initGrTutorialBlocks(registry);
        expect(!registry.contains("gr::tutorial::Gain"));
        for (const auto name : tutorialBlockNames) {
            expect(registry.contains(name)) << name;
            expect(registry.create(name, {}) != nullptr) << name;
        }
    };
}
