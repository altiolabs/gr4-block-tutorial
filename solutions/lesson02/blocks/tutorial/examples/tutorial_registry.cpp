#include "../test/ExpectedBlocks.hpp"
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/GrTutorialBlocks.hpp>
#include <iostream>
int main() {
    auto& registry = gr::globalBlockRegistry();
    gr::blocklib::initGrTutorialBlocks(registry);
    for (const auto name : tutorialBlockNames) {
        if (!registry.contains(name) || !registry.create(name, {})) return 1;
        std::cout << name << '\n';
    }
}
