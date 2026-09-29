Static analysis helps to enforce the readability, maintainability, and logical consistency of iso14229.

Run the analyzers locally with the following commands:

```sh
tools/run_clang_format.sh && make update_srcs

tools/cppcheck/run_MISRA.sh
less reports/cppcheck/report_MISRA.txt

tools/run_clang_tidy.sh | tee tidy.txt
less tidy.txt
```
