# iso14229

iso14229是个针对嵌入式系统的UDS(ISO14229-1:2013)服务器和客户端执行。

iso14229 is a UDS server and client implementation (ISO14229-1:2013) for embedded systems.

**Stability: Usable**

# iso14229 文档 / Documentation

## 服务器：怎么用 / Server: Usage

See [example](/example) for a simple server with socketCAN bindings

```sh
# 设置虚拟socketCAN接口
# setup a virtual socketCAN interface
sudo ip link add name can9 type vcan
sudo ip link set can9 up

# 构建例子服务器
# build the example server
make example/linux

# 在can9接口上运行例子服务器
# run the example server on can9
./example/linux can9
```

```sh
# （可选）在另外一个终端，看看虚拟CAN母线上的数据
# (Optional) In a another shell, monitor the virtual link
candump can9
```

```sh
# 在另外一个终端，安装python依赖性
# In another shell, install the required python packages
pip3 install -r example/requirements.txt

# 然后运行客户端
# then run the client
./example/client.py can9
```

## 服务器：自定服务回调函数 / Server: Custom Service Handlers

| Service | `iso14229` Function |
| - | - |
| 0x11 ECUReset | `void userHardReset()` |
| 0x22 ReadDataByIdentifier | `enum Iso14229ResponseCode userRDBIHandler(uint16_t dataId, const uint8_t *data, uint16_t *len)` |
| 0x28 CommunicationControl | `enum Iso14229ResponseCode userCommunicationControlHandler(uint8_t controlType, uint8_t communicationType)` |
| 0x2E WriteDataByIdentifier | `enum Iso14229ResponseCode userWDBIHandler(uint16_t dataId, const uint8_t *data, uint16_t len)` |
| 0x31 RoutineControl | `int Iso14229ServerRegisterRoutine(Iso14229Server* self, const Iso14229Routine *routine);` |
| 0x34 RequestDownload, 0x36 TransferData, 0x37 RequestTransferExit | `int iso14229ServerRegisterDownloadHandler(Iso14229Server* self, Iso14229DownloadHandlerConfig *handler);` |

## 服务器：应用/启动软件（中间件） / Server: Application / Boot Software (Middleware)

用户自定的服务器逻辑（比如ISO-14229规范指定的”Application Software"和"Boot Software"）可以用中间件来实现。
User-defined server behavior such as the "Application Software" and "Boot Software" described in ISO-14229 can be implemented through middleware.

```c
struct Iso14229UserMiddleware;
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

# iso14229开发文档 / design docs

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
