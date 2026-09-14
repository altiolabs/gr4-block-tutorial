# GNU Radio 4 Block Tutorial

How do you create functional GNU Radio 4 signal processing blocks that you can test, connect to other blocks, and use outside your own program? This hands-on workshop takes you through that process by building an **out-of-tree (OOT) module**: a collection of blocks developed separately from GNU Radio and built against its installed [SDK](https://github.com/gnuradio/gnuradio4).

We'll begin with a block that multiplies each sample by two. From there, we'll introduce reusable sample types, runtime registration, configurable settings, and processing whole buffers. Later blocks add packet-like PMT payloads, sample-aligned tags, a moving average with memory, and threshold events with a reset command. The calculations stay small so we can concentrate on how a block works with the framework—and how to check that it behaves correctly.

Along the way, you'll write unit tests, run C++ flowgraphs, install your blocks for runtime discovery and Studio, and send numeric vectors to a GNU Radio 3 receiver over ZeroMQ. By the end, you'll have a working six-block module and examples of when to use streams, settings, tags, and messages.

This checkout starts as a scaffold, not a finished block library. The CMake and plugin infrastructure is supplied; you add the block implementations, tests, and examples as the lessons progress. Keep working in the same checkout and build and test between steps. The incremental solutions are there to compare against your work or help you resume at a particular lesson.

You should be comfortable reading modern C++ and have some basic signal-processing background. You don't need prior GNU Radio block-development experience. Start with [Lesson 0: Setup and build](docs/lesson00-setup.md), then work through the lessons in order.

## Prerequisites

Use an **already-built GNU Radio 4 [SDK](https://github.com/gnuradio/gnuradio4)**, with a compatible C++23 compiler and standard library, CMake 3.27 or newer, Ninja, Python 3, and Git. The [SDK](https://github.com/gnuradio/gnuradio4) should include core, the block registration generator, standard blocks, and Studio. Lesson 5 also needs the ZeroMQ blocks and a GNU Radio 3 Python environment with ZeroMQ support.

This tutorial builds only the out-of-tree module; it does not download or rebuild GNU Radio. [Lesson 0](docs/lesson00-setup.md) covers [SDK](https://github.com/gnuradio/gnuradio4) activation, compiler selection, dependencies, and the first configure.

## Lessons

Work through these in order. Each lesson builds on the previous one in the same working tree. Run commands from the repository root, not from `docs/`.

- [Lesson 0: Setup and build](docs/lesson00-setup.md)
- [Lesson 1: Your first GNU Radio 4 block](docs/lesson01-gain.md)
- [Lesson 2: Templates, SIMD, and registration](docs/lesson02-templates.md)
- [Lesson 3: Settings and derived state](docs/lesson03-settings.md)
- [Lesson 4: Bulk processing](docs/lesson04-bulk.md)
- [Lesson 5: PMT and GNU Radio 3 interoperability](docs/lesson05-pmt-gr3.md)
- [Lesson 6: Stream tags](docs/lesson06-tags.md)
- [Lesson 7: Stateful processing](docs/lesson07-stateful.md)
- [Lesson 8: Messages and control interaction](docs/lesson08-messages.md)

For a first pass, concentrate on each block's calculation and its main test. The supplied plugin checks, graph helpers, and GR3 receiver are support code you can use as written.

## Solutions and validation

The matching `solutions/lesson00/` through `solutions/lesson08/` directories are incremental overlays, not standalone projects. Lesson 0 preserves the empty scaffold; later snapshots contain the files changed in each lesson.

[Working with the solutions](solutions/README.md) explains how to compare answers or prepare a separate working tree at a completed lesson without changing your workshop code. [Validation notes](VALIDATION.md) record the [SDK](https://github.com/gnuradio/gnuradio4) and checks used in the workshop rehearsal.

The instructor presentation is maintained separately in the
[companion slides repository](https://github.com/altiolabs/gr4-block-tutorial-slides).
