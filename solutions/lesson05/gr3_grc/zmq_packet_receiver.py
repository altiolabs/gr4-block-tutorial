#!/usr/bin/env python3
"""Check four bare GR3 f32 vectors from the tutorial's GR4 PUSH sink."""
import time
from gnuradio import blocks, gr, zeromq
import pmt


def main():
    graph = gr.top_block()
    source = zeromq.pull_msg_source("tcp://127.0.0.1:5558", 100, False)
    debug = blocks.message_debug()
    graph.msg_connect((source, "out"), (debug, "store"))
    graph.start()
    try:
        deadline = time.monotonic() + 15.0
        while debug.num_messages() < 4 and time.monotonic() < deadline:
            time.sleep(0.02)
        if debug.num_messages() < 4:
            raise RuntimeError(f"Timed out: received {debug.num_messages()} of 4 messages")
        expected = [(12.0, 24.0, 36.0), (48.0, 60.0, 72.0)]
        first = None
        for index in range(4):
            message = debug.get_message(index)
            if not pmt.is_f32vector(message):
                raise RuntimeError(f"Message {index} is not a bare f32 uniform vector")
            values = tuple(pmt.f32vector_elements(message))
            if len(values) != 3 or values not in expected:
                raise RuntimeError(f"Unexpected vector: {values}")
            if first is None:
                first = expected.index(values)
            if values != expected[(first + index) % 2]:
                raise RuntimeError(f"Non-alternating vector at message {index}: {values}")
            print(list(values), flush=True)
        print(f"PASS: GR3 {gr.version()}, legacy GR3 PMT codec, four alternating f32 vectors")
    finally:
        graph.stop()
        graph.wait()


if __name__ == "__main__":
    main()
