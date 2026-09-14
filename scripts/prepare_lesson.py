#!/usr/bin/env python3
"""Assemble a fresh workshop OOT from the scaffold and cumulative solutions."""
import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lesson", type=int, choices=range(9),
                        help="0 for an empty scaffold; 1–8 for a completed lesson")
    parser.add_argument("destination", type=Path, help="a new directory (never overwritten)")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    destination = args.destination.resolve()
    if destination.exists():
        parser.error(f"Destination already exists: {destination}; choose a new directory")
    destination.mkdir(parents=True)

    # Shared OOT plumbing. Do not copy the participant's blocks or historical READMEs.
    for relative in ("CMakeLists.txt", "LICENSE", "cmake/gnuradio4TutorialConfig.cmake.in",
                     "cmake/Dependencies.cmake",
                     "blocks/tutorial/plugin.cpp"):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / relative, target)
    for lesson in range(args.lesson + 1):
        snapshot = root / "solutions" / f"lesson{lesson:02}"
        for directory in ("blocks", "test_external", "gr3_grc"):
            source = snapshot / directory
            if source.is_dir():
                shutil.copytree(source, destination / directory, dirs_exist_ok=True)
    (destination / "blocks/tutorial/include/gnuradio-4.0/tutorial").mkdir(parents=True, exist_ok=True)
    print(f"Prepared {'empty scaffold' if args.lesson == 0 else f'completed lesson {args.lesson}'}: {destination}")
    print("Configure and build here using the activated SDK and the main tutorial README.")


if __name__ == "__main__":
    main()
