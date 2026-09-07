#!/usr/bin/env python

import sys
import isotp
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from udsoncan import (
    CommunicationType,
    MemoryLocation,
)
from udsoncan.services import (
    DiagnosticSessionControl,
    CommunicationControl,
    ControlDTCSetting,
    RoutineControl,
    ECUReset,
)
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection

ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES = 0xFF01
SECURITY_ACCESS_LEVEL = 1

with open("security_access.pem", "rb") as f:
    private_key = serialization.load_pem_private_key(f.read(), password=None)


def sign_seed(seed, level=None, params=None):
    return private_key.sign(seed, padding.PKCS1v15(), hashes.SHA256())

image_path = sys.argv[1] if len(sys.argv) > 1 else "build/nucleo/fwupdate/zephyr/zephyr.signed.bin"
with open(image_path, "rb") as f:
    image = f.read()

conn = IsoTPSocketConnection('can0', isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=0x7E8, txid=0x7E0))

with Client(conn) as client:
    print(f"downloading {image_path}")
    client.config['security_algo'] = sign_seed
    client.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    client.control_dtc_setting(ControlDTCSetting.SettingType.off)
    client.communication_control(
        CommunicationType(normal_msg=True, subnet=False).get_byte_as_int(),
        CommunicationControl.ControlType.disableRxAndTx
    )
    client.change_session(DiagnosticSessionControl.Session.programmingSession)
    client.unlock_security_access(SECURITY_ACCESS_LEVEL)
    response = client.request_download(MemoryLocation(0, len(image), address_format=8))
    block_size = response.service_data.max_length - 2

    seq = 1
    for offset in range(0, len(image), block_size):
        client.transfer_data(seq, image[offset:offset + block_size])
        seq = (seq + 1) % 256
        print(".", end="", flush=True)
    print("")

    client.request_transfer_exit()
    client.routine_control(ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES, RoutineControl.ControlType.startRoutine)
    client.ecu_reset(ECUReset.ResetType.hardReset)

    print(f"transferred {len(image)} bytes from {image_path}")
