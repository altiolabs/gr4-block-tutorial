#include "../test/ExpectedBlocks.hpp"
#include <gnuradio-4.0/PluginLoader.hpp>
#include <algorithm>
#include <iostream>
int main() {
    std::cout << "Loading installed plugin paths" << std::endl;
    auto& loader = gr::globalPluginLoader();
    for (const auto& [path, reason] : loader.failedPlugins()) {
        if (path.find("GrTutorialBlocksShared") != std::string::npos) {
            std::cerr << "Tutorial plugin rejected: " << reason << '\n';
            return 1;
        }
    }
    std::cout << "Checking tutorial plugin factories" << std::endl;
    for (const auto name : tutorialBlockNames) {
        // instantiate() can fall back to the global registry. Also require an
        // accepted plugin to advertise and construct each specialization.
        bool pluginOwned = false;
        for (const auto& plugin : loader.plugins()) {
            const auto names = plugin->availableBlocks();
            if (std::ranges::find(names, name) != names.end()) {
                pluginOwned = static_cast<bool>(plugin->createBlock(name, {}));
                break;
            }
        }
        if (!pluginOwned || !loader.instantiate(name, {})) {
            std::cerr << "Missing or unconstructible plugin: " << name << '\n';
            return 1;
        }
        std::cout << name << std::endl;
    }
}
