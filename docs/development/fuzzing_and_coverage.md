# Fuzzing

The server has a fuzz harness for the transport-facing interface.
Run it with:

```sh
tools/run_fuzz_server.sh
```

## The corpus

Merge it with:
```sh
tools/reduce_fuzz_corpus.sh
```

# Coverage

.lcov files are generated for both the fuzz and unit test suites respectively with the following commands:
```sh
tools/gen_coverage_fuzz.sh
tools/gen_coverage_unit.sh
```

Next, three html reports are generated:

```sh
tools/gen_lcovreport.sh
```

- `coverage_unit`
- `coverage_fuzz` 
- `coverage` a combined fuzz+unit coverage report 

