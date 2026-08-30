# Onboarding Notes

## Install bazel

Project requires `bazel` 8.1.1 which can be installed via

```bash
sudo apt update && sudo apt install bazel-8.1.1
```

If version is not supported, `bazel` must be fetched from a different location, see [Bazel Home](https://bazel.build/install/ubuntu#install-on-ubuntu)

```bash
sudo apt install apt-transport-https curl gnupg -y
curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel-archive-keyring.gpg
sudo mv bazel-archive-keyring.gpg /usr/share/keyrings
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
```

## Install required tools

Install required Python modules

```bash
pip3 install -r requirements.txt
```

For the build this is not sufficient - you need to install also `codechecker`

```bash
pip3 install codechecker
```

Then you should be able to run `make` in the root dir

```bash
make
```

## Return values of TP functions

- recv:
  - 0: response pending
  - < 0: error
  - n: message received

- send:
  - 0: send pending
  - < 0: error
  - n: message sent

- poll: one of
    UDS_TP_IDLE = 0x0000, // ready to send/recv
    UDS_TP_SEND_IN_PROGRESS = 0x0001,// sending not complete

    UDS_TP_RECV_COMPLETE = 0x0002,// recv finished (not used/needed?)
    UDS_TP_ERR = 0x0004,


```C
tp_status = UDSTpPoll(client->tp);
err = UDS_OK;
switch (client->state) {
    case STATE_IDLE:
        // reset options...
        break;
    case STATE_SENDING: {
        // tp_recv must return 0
        // rx buffer cleared
        // tp_send must return either
        // > 0:
        //  number of sent bytes is
        //   a) equal to expected bytes -> STATE_AWAIT_SEND_COMPLETE
        //.  b) not equal to expected bytes -> err = UDS_ERR_BUFSIZ
        // 0: send pending
        // < 0: ERROR = UDS_ERR_TPORT
        break;
    }
    case STATE_AWAIT_SEND_COMPLETE: {
        // tp_status = UDS_TP_SEND_IN_PROGRESS  -> exit UDS_OK
        // -> STATE_AWAIT_RESPONSE
        // start p2 timer
        break;
    }
    case STATE_AWAIT_RESPONSE: {
        // tp_recv must return either
        // < 0: error = UDS_ERR_TPORT, -> STATE_IDLE
        // = 0: continue receive unless p2 expired
        // > 0: number of received bytes, server checks response
        //  a) OK -> forward to client app
        //  b) exit with error
        break;
    }
