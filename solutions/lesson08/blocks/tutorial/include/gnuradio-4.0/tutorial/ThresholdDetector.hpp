#pragma once

#include <span>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/Message.hpp>

namespace gr::tutorial {

struct ThresholdDetector : gr::Block<ThresholdDetector> {
    using gr::Block<ThresholdDetector>::Block;
    using gr::Block<ThresholdDetector>::processMessages;
    using Description = gr::Doc<"Notify on upward threshold crossings.">;

    gr::PortIn<float> in;
    gr::PortOut<float> out;
    gr::Annotated<float, "threshold"> threshold{1.0f};
    gr::MsgPortIn commands;
    gr::MsgPortOut events;
    GR_MAKE_REFLECTABLE(ThresholdDetector, in, out, threshold, commands, events);

    void start() { _above = false; }

    void settingsChanged(const gr::property_map& old_settings,
                         const gr::property_map& new_settings) {
        if (new_settings.contains("threshold") &&
            old_settings.at("threshold") != new_settings.at("threshold")) {
            _above = false;
        }
    }

    [[nodiscard]] float processOne(float input) {
        const bool above = input >= threshold.value;
        if (above && !_above) {
            gr::sendMessage<gr::message::Command::Notify>(
                events, this->unique_name, "threshold_crossing",
                gr::property_map{{"event", "threshold_crossing"}, {"value", input}});
        }
        _above = above;
        return input;
    }

    void processMessages(gr::MsgPortIn& /*port*/,
                         std::span<const gr::Message> messages) {
        for (const auto& message : messages) {
            if (!message.serviceName.empty() &&
                message.serviceName != this->name &&
                message.serviceName != this->unique_name) {
                continue;
            }
            if (message.cmd == gr::message::Command::Set &&
                message.endpoint == "reset" &&
                message.data.has_value() && message.data->empty()) {
                _above = false;
            }
        }
    }

private:
    bool _above{false};
};

} // namespace gr::tutorial

GR_REGISTER_BLOCK(gr::tutorial::ThresholdDetector)
