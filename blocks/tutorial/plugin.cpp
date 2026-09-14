#include <gnuradio-4.0/Plugin.hpp>
#include <gnuradio-4.0/GrTutorialBlocks.hpp>

GR_PLUGIN("GNU Radio 4 Tutorial", "GNU Radio", "MIT", "4.0.0")

namespace {
// Give the loader an owned plugin instance so the generated factories remain
// loaded for their entire lifetime. The same generated entries support linking.
const auto registered = gr::blocklib::initGrTutorialBlocks(grPluginInstance());
}
