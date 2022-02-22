#!/usr/bin/env python3

import sys

from can.interfaces.socketcan import SocketcanBus

import isotp

import udsoncan
from udsoncan.client import Client
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.services import *

def security_algo(level, seed, params):
    print(f"level: {level}, seed: {seed}, params: {params}")
    return bytes([0])

UDSONCAN_CLIENT_CONFIG = {
        'exception_on_negative_response'	: True,	
        'exception_on_invalid_response'		: True,
        'exception_on_unexpected_response'	: True,
        'security_algo'				: security_algo,
        'security_algo_params'		: None,
        'tolerate_zero_padding' 	: True,
        'ignore_all_zero_dtc' 		: True,
        'dtc_snapshot_did_size' 	: 2,		# Not specified in standard. 2 bytes matches other services format.
        'server_address_format'		: None,		# 8,16,24,32,40
        'server_memorysize_format'	: None,		# 8,16,24,32,40

        'data_identifiers' 		:  {
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
        'input_output' 			: {},
        'request_timeout'		: 5,
        'p2_timeout'			: 1.5, 
        'p2_star_timeout'		: 5,
        'standard_version'              : 2013,  # 2006, 2013, 2020
        'use_server_timing'             : False
}


if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print(f"usage: {sys.argv[0]} [socketCAN link]")
        exit(-1)

    with Client(
            conn=PythonIsoTpConnection(
                isotp_layer=isotp.CanStack(
                    bus=(SocketcanBus(channel=sys.argv[1])),
                    address=isotp.Address(rxid=0x7A8, txid=0x7A0),
                    params = {
                        "tx_data_min_length": 8,
                        "blocksize": 0,
                        "squash_stmin_requirement": True
                    }
                )
            ),
            config=UDSONCAN_CLIENT_CONFIG,
        ) as client:

        print("Sending ECU reset. . .")
        response = client.ecu_reset(ECUReset.ResetType.hardReset)
        print(f"Received: {response}")
