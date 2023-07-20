#!/usr/bin/env python3

from can.interfaces.socketcan import SocketcanBus

import isotp

from udsoncan.client import Client
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.services import *

SRV_SEND_ID = 0x7E8
SRV_PHYS_RECV_ID = 0x7E0
SRV_FUNC_RECV_ID = 0x7DF


def security_algo(level, seed, params):
    print(f"level: {level}, seed: {seed}, params: {params}")
    return bytes([0])


UDSONCAN_CLIENT_CONFIG = {
    "exception_on_negative_response": True,
    "exception_on_invalid_response": True,
    "exception_on_unexpected_response": True,
    "security_algo": security_algo,
    "security_algo_params": None,
    "tolerate_zero_padding": True,
    "ignore_all_zero_dtc": True,
    "dtc_snapshot_did_size": 2,  # Not specified in standard. 2 bytes matches other services format.
    "server_address_format": None,  # 8,16,24,32,40
    "server_memorysize_format": None,  # 8,16,24,32,40
    "data_identifiers": {
        0x0000: "B",
        0x0001: "b",
        0x0002: "H",
        0x0003: "h",
        0x0004: "I",
        0x0005: "i",
        0x0006: "Q",
        0x0007: "q",
        0x0008: "20B",
    },
    "input_output": {},
    "request_timeout": 5,
    "p2_timeout": 1.5,
    "p2_star_timeout": 5,
    "standard_version": 2013,  # 2006, 2013, 2020
    "use_server_timing": False,
    "exception_on_negative_response": False,
}

if __name__ == "__main__":
    with Client(
        conn=PythonIsoTpConnection(
            isotp_layer=isotp.CanStack(
                bus=(SocketcanBus(channel="vcan0")),
                address=isotp.Address(rxid=SRV_SEND_ID, txid=SRV_PHYS_RECV_ID),
                params={
                    "tx_data_min_length": 8,
                    "blocksize": 0,
                    "squash_stmin_requirement": True,
                },
            )
        ),
        config=UDSONCAN_CLIENT_CONFIG,
    ) as client:

        response = client.ecu_reset(ECUReset.ResetType.hardReset)
        print(f"Received: {response}")

        response = client.change_session(
            DiagnosticSessionControl.Session.extendedDiagnosticSession
        )
        print(f"Received: {response}")

        response = client.unlock_security_access(1)
        print(f"Received: {response}")

        response = client.change_session(
            DiagnosticSessionControl.Session.extendedDiagnosticSession
        )
        print(f"Received: {response}")

        response = client.read_data_by_identifier([0x0008])
        print(f"Received: {response}")
        print(
            f"rdbi data: {bytes(response.service_data.values[0x0008]).decode('ascii')}"
        )

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
