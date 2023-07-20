#!/usr/bin/env python

import subprocess
import sys

transports = [
    "TP=ISOTP_SOCKET",
    "TP=ISOTP_C",
]

clients = [
    "./client",
    "./examples/client.py",
]


def make_examples(tp: str):
    subprocess.check_call(["make", "clean"])
    for thing in ["server", "client"]:
        subprocess.check_call(" ".join([tp, "make", thing]), shell=True)


for tp in transports:
    make_examples(tp)
    server_proc = subprocess.Popen(["./server"], stdout=sys.stdout)
    assert None == server_proc.returncode
    for client in clients:
        subprocess.check_call([client], shell=True)
    server_proc.kill()
