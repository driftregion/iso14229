# iso14229

iso14229是个针对嵌入式系统的UDS(ISO14229-1:2013)服务器和客户端执行。

iso14229 is a UDS server and client implementation (ISO14229-1:2013) targeting embedded systems. It embeds the [`isotp-c`](https://github.com/lishen2/isotp-c) transport layer.

**Stability: Unstable**

- callback functions give you complete control
- statically allocated
- aims to be toolchain-independent: no toolchain-specific extensions
    - tested: gcc
- aims to be architecture-independent
    - tested: arm, x86-64, ppc
    - tests run under qemu 

# iso14229 文档 / Documentation

##  支持服务(服务器和客户端) / supported functions (server and client )

| SID | name | supported |
| - | - | - |
| 0x10 | diagnostic session control | ✅ |
| 0x11 | ECU reset | ✅ |
| 0x14 | clear diagnostic information | ❌ |
| 0x19 | read DTC information | ❌ |
| 0x22 | read data by identifier | ✅ |
| 0x23 | read memory by address | ❌ |
| 0x24 | read scaling data by identifier | ❌ |
| 0x27 | security access | ✅ |
| 0x28 | communication control | ✅ |
| 0x2A | read periodic data by identifier | ❌ |
| 0x2C | dynamically define data identifier | ❌ |
| 0x2E | write data by identifier | ✅ |
| 0x2F | input control by identifier | ❌ |
| 0x31 | routine control | ✅ |
| 0x34 | request download | ✅ |
| 0x35 | request upload | ❌ |
| 0x36 | transfer data | ✅ |
| 0x37 | request transfer exit | ✅ |
| 0x38 | request file transfer | ❌ |
| 0x3D | write memory by address | ❌ |
| 0x3E | tester present | ✅ |
| 0x83 | access timing parameter | ❌ |
| 0x84 | secured data transmission | ❌ |
| 0x85 | control DTC setting | ✅ |
| 0x86 | response on event | ❌ |


## 服务器：例子 / Server: Example (linux)

See [example/server.c](/example/server.c) for a simple server with socketCAN bindings

```sh
# 设置虚拟socketCAN接口
# setup a virtual socketCAN interface
sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up

# 构建例子服务器
# build the example server
make example/linux

# 在vcan0接口上运行例子服务器
# run the example server on vcan0
./example/linux vcan0
```

```sh
# （可选）在另外一个终端，看看虚拟CAN母线上的数据
# (Optional) In a another shell, monitor the virtual link
candump vcan0
```

```sh
# 在另外一个终端，安装python依赖性
# In another shell, install the required python packages
pip3 install -r example/requirements.txt

# 然后运行客户端
# then run the client
./example/client.py vcan0
```

## 客户端：怎么用 / Client: Basic Usage

Currently undocumented. See `test_iso14229.c` for usage examples

## 贡献/contributing

欢迎来贡献/contributions are welcome


# 感谢 / Acknowledgements

- [`isotp`](https://github.com/lishen2/isotp-c) which this project embeds

# License

MIT

# 变更记录 / Changelog


## 0.0.0
- initial release

## 0.1.0
- Add client
- Add server SID 0x27 SecurityAccess
- API changes

## 0.2.0
- removed all instances of `__attribute__((packed))`
- refactored server download functional unit API to simplify testing
- refactored tests:
    - ordered by service
    - documented macros
- removed middleware because it made no sense after the above refactoring
- simplified server routine control API
- removed redundant function iso14229ServerEnableService: Services are enabled by registering user handlers.
- updated example


# iso14229开发文档 / design docs

## Running Tests

```sh
CC=powerpc-linux-gnu-gcc make test_bin
qemu-ppc -L /usr/powerpc-linux-gnu test_bin
```

```sh
bazel test --compilation_mode=dbg //...
```


## 客户端请求状态机

```plantuml
@startuml
title 客户端请求状态机
note as N1
enum {
    kNoError=0,
    kErrBadRequest,
    kErrP2Timeout,
} ClientErr;

static inline bool isRequestComplete() {return state==Idle;}

while (Idle != client->state) {
    receiveCAN(client);
    Iso14229ClientPoll(client);
}
end note

state Idle
state Sending
state Sent
state SentAwaitResponse
state ProcessResponse
Idle: if (ISOTP_RET_OK == isotp_receive(...)) // Error
ProcessResponse: isotp_receive()
ProcessResponse: _ClientValidateResponse(...)
ProcessResponse: _ClientHandleResponse(...)

Sending --> Sent: 传输层完成传输 

Sent --> Idle : suppressPositiveResponse
Sending --> SentAwaitResponse: !suppressPositiveResponse
SentAwaitResponse -> Idle: 响应收到了 ||\np2 超时
SentAwaitResponse --> ProcessResponse : ISOTP_RECEIVE_STATUS_FULL == link->receive_status
ProcessResponse --> Idle

[*] -> Idle
Idle -> Sending : _SendRequest()

@enduml
```

```plantuml
@startuml
title Request Lifecycle
alt normal
    alt positive response
        client --> client: Sending
        client -> server : *Any* Service
        client --> client: SentAwaitResponse: set p2
        alt 0x78 requestCorrectlyReceived-ResponsePending
            server -> client : 0x3F 0x78 
            client -->server : txLink  idle
            client --> client: SentAwaitResponse: set p2star
        end
        server -> client : Positive Service Response
        client --> client: Idle 
    else negative response
        server -> client !! : Negative Service Response
        client --> client: Idle: RequestErrorNegativeResponse
    else SID mismatch
        server -> client !! : Mismatched Service Response
        client --> client: Idle: RequestErrorResponseSIDMismatch
    end
else unexpected response
    server -> client !! : Unexpected Response
    client --> client: Idle: RequestErrorUnsolicitedResponse
end
@enduml
```


```plantuml
@startuml
' !pragma useVerticalIf on
title 客户端请求流程
start

:clientSendRequest();
if (验证参数) then (对)
:ok;
else (不对)
:foo;
detach
endif

:clearRequestContext();
if (等待UDS访问) then (访问接收了,进入UDS会话)
else (时间超过<b>20ms)
@enduml
```

## 服务器 0x78 requestCorrectlyReceived-ResponsePending


```plantuml
@startuml
client -> server : *Any* Service
server -> userServiceHandler: handler(args)
note right: Doing this will take a long time\nso I return 0x78
userServiceHandler -> server: 0x78
server -> client : 0x3F 0x78 
client -->server : txLink  idle
server -> userServiceHandler: handler(args)
note right: actually call the long-running service
... p2* > t > p2 ... 
userServiceHandler -> server : Service Response
server -> client : Service Response
@enduml
```

```plantuml
@startuml
' !pragma useVerticalIf on
title 0x78流程(写flash)
start

:BufferedWriterWrite(BufferedWriter *self, const uint8_t *ibuf, uint32_t size, bool RCRRP);

if (RCRRP) then (true)
:write to flash;
else (false)
endif
if (iBufIdx == size) then (true)
    :write to pageBuffer;
    :iBufIdx = 0;
    :return kBufferedWriterWritePending;
    :0x78 RCRRP;
    detach;
else (false)
    :memmove(pageBuffer + pageBufIdx, iBuf + iBufIdx, size - iBufIdx);
    :write to pageBuffer;
    :iBufIdx += size;
    :0x01 PositiveResponse;
    :0x78 RCRRP;
    detach
endif

@enduml
```
