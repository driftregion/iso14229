#!/usr/bin/env python

import udsoncan
import isotp
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.services import *
import time

conn = IsoTPSocketConnection('can0', isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=0x7E0, txid=0x7E8))

with Client(conn) as client:

    response = client.ecu_reset(ECUReset.ResetType.hardReset)
    print(f"Received: {response}")

    time.sleep(2)

    response = client.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    print(f"Received: {response}")

    response = client.unlock_security_access(1)
    print(f"Received: {response}")

    response = client.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    print(f"Received: {response}")

    response = client.read_data_by_identifier([0x0008])
    print(f"Received: {response}")
    print(f"rdbi data: {bytes(response.service_data.values[0x0008]).decode('ascii')}")

    response = client.read_data_by_identifier([0x0001])
    print(f"Received: {response}")
    print(f"rdbi data: {response.service_data.values[0x0001]}")

    response = client.write_data_by_identifier(0x0001, 2)
    print(f"Received: {response}")
    
    response = client.read_data_by_identifier([0x0001])
    print(f"Received: {response}")
    print(f"rdbi data: {response.service_data.values[0x0001]}")

    response = client.write_data_by_identifier(0x0001, 0)
    print(f"Received: {response}")

    response = client.read_data_by_identifier([0x0001])
    print(f"Received: {response}")
    print(f"rdbi data: {response.service_data.values[0x0001]}")