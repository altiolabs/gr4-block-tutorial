#!/usr/bin/env python3
"""Read the installed catalog used by Studio; no graph/session is modified."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from urllib.request import urlopen


def main():
    with tempfile.TemporaryDirectory(prefix="tutorial-catalog-") as directory:
        port_file = Path(directory) / "port"
        environment = dict(os.environ, GR4CP_PORT="0", GR4CP_PORT_FILE=str(port_file))
        server = subprocess.Popen(["gr4cp_server"], env=environment, stdout=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 30
            while not port_file.exists() and time.monotonic() < deadline:
                if server.poll() is not None:
                    raise RuntimeError("Catalog server exited before starting")
                time.sleep(0.05)
            port = int(port_file.read_text())
            with urlopen(f"http://127.0.0.1:{port}/blocks", timeout=30) as response:
                catalog = json.load(response)
            tutorial = [block for block in catalog if "gr::tutorial::" in block["id"]]
            for block in sorted(tutorial, key=lambda block: block["id"]):
                print(block["id"])
            expected_counts = {"Gain": 4, "Square": 2, "Packetizer": 2,
                               "TagForwarder": 1, "MovingAverage": 2, "ThresholdDetector": 1}
            for name, count in expected_counts.items():
                found = [block for block in tutorial
                         if block["id"].split("::")[-1].split("<")[0] == name]
                if len(found) != count:
                    raise RuntimeError(f"{name}: expected {count} catalog entries, found {len(found)}")
            for name, setting, expected in [("Square", "gain_db", 0),
                                             ("MovingAverage", "window_size", 3),
                                             ("ThresholdDetector", "threshold", 1)]:
                block = next(block for block in tutorial
                             if block["id"] == f"gr::tutorial::{name}<float32>"
                             or block["id"] == f"gr::tutorial::{name}")
                parameter = next(parameter for parameter in block["parameters"] if parameter["name"] == setting)
                if float(parameter["default"]) != expected:
                    raise RuntimeError(f"Unexpected default for {name}.{setting}: {parameter}")
                print(f"{name}.{setting} default = {parameter['default']}")
            print("PASS: all 12 variants and reflected defaults visible in Studio's /blocks catalog")
        finally:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == "__main__":
    main()
