#!/usr/bin/env python3

__doc__ = """
The core idea is that tests assert invariant properties that should be
upheld regardless of the specific transport protocol, timing parameters, etc.
"""

import os
import sys
import subprocess

for f in sys.argv[1:]:
    for tp_type in ["0", "1", "2"]:
        subprocess.check_call([f"test/{f}"], env={
            "UDS_TP_TYPE": tp_type
        })

