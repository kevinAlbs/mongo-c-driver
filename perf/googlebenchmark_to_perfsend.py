"""
Converts a JSON file from Google Benchmark to JSON suitable for perf.send.

Google Benchmark must be run with multiple repetitions. Only aggregate median data is used.

perf.send format is described here: https://github.com/10gen/performance-tooling-docs/blob/main/getting_started/performance_monitoring_setup.md

Prints perf.send format to stdout:
[
    {
        info: {
            test_name: "x",
            args: {
                threads: x
            }
        },
        metrics: [
            {
                name: "ops_per_second",
                value: x
            }
        ]
    }
]
"""
import json
import sys

if len(sys.argv) != 2:
    print ("Usage: python {} <googlebenchmark_results.json>".format(sys.argv[0]))
    sys.exit(1)

gb = json.loads(open(sys.argv[1], "r").read())

out = []

for b in gb["benchmarks"]:
    run_name = b["run_name"]
    threads = b["threads"]
    run_type = b["run_type"]
    ops_per_sec = b["ops_per_sec"]

    if run_type != "aggregate":
        continue
    
    if b["aggregate_name"] != "median":
        continue

    out.append({
        "info": {
            "test_name": run_name,
            "args": {
                "threads": threads
            }
        },
        "metrics": [
            {
                "name": "ops_per_sec",
                "value": ops_per_sec
            }
        ]
    })

if len(out) == 0:
    print ("Error: no benchmarks were found.")
    print ("Google Benchmark must be run with multiple repetitions.")
    print ("Run Google Benchmark with --benchmark_repetitions")
    sys.exit(1)

print (json.dumps(out, indent=4))