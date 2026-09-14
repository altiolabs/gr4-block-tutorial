# Working with the lesson solutions

The repository root is the starting OOT scaffold. `lesson00` preserves that scaffold;
each later directory contains only files changed in that lesson, at their
repository-relative paths. A lesson directory alone is not a CMake project.
Use the [main README](../README.md#lessons) for the lesson list and
[Lesson 0](../docs/lesson00-setup.md) for the current SDK setup.

Work directly in the repository root for the workshop. If you want a separate
empty practice directory, run:

```bash
python3 scripts/prepare_lesson.py 0 build-workshop   # before Lesson 1
```

To resume with a completed lesson, assemble it in a fresh directory. For example,
the original scalar Square or the finished OOT:

```bash
python3 scripts/prepare_lesson.py 3 build-lesson03   # ready for Lesson 4
python3 scripts/prepare_lesson.py 8 build-completed  # all eight lessons
```

The helper copies shared build plumbing and overlays solutions 00 through the
requested lesson. It never changes an existing directory, installs anything,
or runs Git. Configure, build, and test inside the resulting directory using
the activated SDK and compiler from Lesson 0; Boost.UT is handled
automatically just as in the root scaffold. There are
no tests in lesson 00; lesson 01 has just `qa_Gain`.

When working through your own implementation, compare or copy individual files
from the current lesson. The `blocks/`, `test_external/`, and `gr3_grc/`
subdirectories are the code overlays. For example, from the repository root:

```bash
cp solutions/lesson02/blocks/tutorial/include/gnuradio-4.0/tutorial/Gain.hpp \
   blocks/tutorial/include/gnuradio-4.0/tutorial/Gain.hpp
```

Copy the lesson's build/test support too when using a complete answer. Lesson 2
adds the registry checks and the external consumer; Lesson 3 adds the finite
graph; Lesson 5 adds the packet test and GR3 receiver; Lesson 6 adds graph-test helpers. Don't copy later
lessons' build lists into an earlier stage: they refer to blocks you haven't
created yet. After changes, run `cmake -S . -B build`, build, and run CTest.

Keep using the same working directory as you implement later lessons. To go
backwards for teaching or comparison, prepare a fresh directory with the helper.
After installing a different stage, restart Studio so its catalog reflects the
new plugin. Each stage installs the same tutorial library name.
