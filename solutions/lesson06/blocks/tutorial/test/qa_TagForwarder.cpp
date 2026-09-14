#include "GraphHelpers.hpp"
#include <boost/ut.hpp>
#include <gnuradio-4.0/tutorial/TagForwarder.hpp>

int main() {
    using namespace boost::ut;
    "exact tags across graph chunks and fresh runs"_test = [] {
        for (const std::size_t chunk : {1UZ, 2UZ, 6UZ}) {
            for (int run = 0; run < 2; ++run) {
                gr::Graph graph;
                const std::vector<float> values{1, 2, 3, 4, 5, 6};
                auto& source = tutorial_test::source(graph, values);
                source.out.max_samples = chunk;
                source._tags = {{3UZ, {{"tutorial.input", std::int64_t{42}}}}};
                auto& forwarder = graph.emplaceBlock<gr::tutorial::TagForwarder>({});
                auto& sink = graph.emplaceBlock<tutorial_test::Sink<float>>({});
                tutorial_test::require(graph.connect<"out", "in">(source, forwarder));
                tutorial_test::require(graph.connect<"out", "in">(forwarder, sink));
                const auto scheduler = tutorial_test::run(std::move(graph));
                expect(sink._samples == values);
                std::vector<gr::Tag> application_tags;
                for (const auto& tag : sink._tags) {
                    // EOS is a lifecycle signal, not an application tag.
                    if (tag.map.contains("tutorial.marker") || tag.map.contains("tutorial.input"))
                        application_tags.push_back(tag);
                    else
                        expect(tag.map.contains(gr::tag::END_OF_STREAM.shortKey()));
                }
                expect(eq(application_tags.size(), 2UZ)) << fatal;
                expect(eq(application_tags[0].index, 0UZ));
                expect(application_tags[0].map == gr::property_map{{"tutorial.marker", true}});
                expect(eq(application_tags[1].index, 3UZ));
                expect(application_tags[1].map == gr::property_map{{"tutorial.input", std::int64_t{42}}});
            }
        }
    };
    "empty work and restart rearm marker"_test = [] {
        gr::tutorial::TagForwarder forwarder;
        gr::PortOut<float> source;
        gr::PortIn<float> sink;
        expect(source.connect(forwarder.in).has_value());
        expect(forwarder.out.connect(sink).has_value());
        for (int run = 0; run < 2; ++run) {
            forwarder.start();
            {
                auto input = forwarder.in.get<gr::SpanReleasePolicy::ProcessAll>(0);
                auto output = forwarder.out.reserve<gr::SpanReleasePolicy::ProcessAll>(0);
                expect(forwarder.processBulk(input, output) == gr::work::Status::OK);
            }
            {
                auto data = source.streamWriter().reserve(1);
                data[0] = 7.0f;
                data.publish(1);
            }
            {
                auto input = forwarder.in.get<gr::SpanReleasePolicy::ProcessAll>(1);
                auto output = forwarder.out.reserve<gr::SpanReleasePolicy::ProcessAll>(1);
                expect(forwarder.processBulk(input, output) == gr::work::Status::OK);
            }
            auto samples = sink.streamReader().get(1);
            expect(eq(samples[0], 7.0f));
            expect(samples.consume(1));
            auto tags = sink.tagReader().get();
            expect(eq(tags.size(), 1UZ)) << fatal;
            expect(tags[0].map == gr::property_map{{"tutorial.marker", true}});
            expect(tags.consume(tags.size()));
        }
    };
}
