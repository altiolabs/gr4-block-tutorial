#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/GrTutorialBlocks.hpp>
int main() {
    auto& registry = gr::globalBlockRegistry();
    gr::blocklib::initGrTutorialBlocks(registry);
    return registry.create("gr::tutorial::ThresholdDetector", {}) ? 0 : 1;
}
