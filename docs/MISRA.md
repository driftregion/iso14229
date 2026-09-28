# MISRA

This library is written in conjunction with the MISRA C:2023 guidelines from release version `0.11.0` onwards.
A subset of guideline violations are enforced with the [cppcheck](https://cppcheck.sourceforge.io/) misra.py addon, using the suppression file in [`tools/cppcheck/suppressions.txt`](../tools/cppcheck/suppressions.txt).
The invocation of cppcheck is in [`tools/cppcheck/run_MISRA.sh`](../tools/cppcheck/run_MISRA.sh).
The scope of the analysis is a single translation unit consisting of `iso14229.c` and `iso14229.h`.

# Deviations

The following rules are suppressed globally:

| Rule | Description | Justification |
| - | - | - |
| 2.5 | A project should not contain unused macro definitions | This is a library. `iso14229.h` exposes user-facing definitions, only a subset of which are used internally. |
| 8.7 | Functions and objects should not be defined with external linkage if they are referenced in only one translation unit | This is a library. `iso14229.h` exposes user-facing functions. |
| 15.1 | The *goto* statement should not be used | `goto` is used only in specific cases where code-deduplication is realized by sharing common cleanup code.  |
| 15.5 | A function should have a single point of exit at the end | This rule was found to increase code complexity. |

False-positives and intentional deviations are suppressed inline.
These can be located in the code with `grep`. For example:
```sh
grep 'misra-c2012-*' src -rI -n 
```

Intentional inline deviations are described here:

| Rule | Description | Justification |
| - | - | - |
| 11.3 | A conversion shall not be performed between a pointer to object type  and a pointer to a different object type | The transport interface `tp.h` uses struct inheritance, embedding a base struct at offset zero within a derived type. Alignment is enforced with `static_assert`. |
| 19.2 | The union keyword should not be used | The DTC submodule uses unions. Mangling is prevented by tagging. | 

# Statement on Compliance

`iso14229` does not claim to be MISRA-compliant, because compliance means agreement between the supplier (me) and the acquirer (you) on a *guideline enforcement plan* (GEP).
If you need one of these, contact me at iso14229dev@gmail.com.


