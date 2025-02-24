#!/usr/bin/env python

import udsoncan
import isotp
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.services import *
import time

conn = IsoTPSocketConnection('can0', isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=0x7E0, txid=0x7E8))

with Client(conn) as client:

    response = client.tester_present()
    print(f"Received: {response}")
