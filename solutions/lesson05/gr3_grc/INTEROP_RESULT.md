Observed on 2026-09-13 using the existing sibling `gnuradio4-latest` full-mac SDK:

- GNU Radio 4 SDK: workspace `cc49da6`, core `ff37d5f`, blocks `5a3e8da`.
- OOT compiler: Homebrew Clang 23.1.0 on macOS arm64.
- GNU Radio 3: 3.10.12.0.
- Transport: GR4 ZmqPushSink bound to tcp://127.0.0.1:5558; GR3 PULL Message Source connected.
- Codec: explicit `pmt_wire_format = "GR3"`, legacy GR3 PMT binary format,
  bare f32 uniform vectors (no PDU wrapper). The SDK default is `GR4_YAML_V1`.
- Receiver output, in order:

  ```text
  [12.0, 24.0, 36.0]
  [48.0, 60.0, 72.0]
  [12.0, 24.0, 36.0]
  [48.0, 60.0, 72.0]
  ```

The bounded Python acceptance check exited with status 0. The publisher exited
with status 0 after SIGINT both with a receiver and with no receiver connected.
The GRC asset compiled with GNU Radio Companion 3.10.12.0, and its generated
graph printed the received vectors through Message Debug's `print` port.

The Homebrew Python installation was missing `pyzmq`; the GRC compiler also
needed `mako` and `pyyaml`. These were installed into a local virtual environment
under `build/gr3-python` with `--system-site-packages` to reuse the existing GR3
bindings. No SDK rebuild, reinstall, or changes were needed.
