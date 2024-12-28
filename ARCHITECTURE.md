
Sending an Idle event serves as a trigger to go to the next state.

Is there a way to make it usable with or without the fn?
- callback fn is better for non-blocking systems because it enables test of fn
- nonblocking is better for threaded systems because it's simpler


Client events

- Guarantee that no more than one of the same type of event is raised per call
- errors should be returned before idle
- validating of response should happen 
    UDS_EVT_ResponseReceived should be renamed if it only fires when the response is valid
    - alternately, the data can contain a struct with NRC

What's better behavior for when the server returns an NRC?

```c
case UDS_EVT_ResponseReceived: {
    int nrc = *(int*)arg;
    switch (nrc) {
        case UDS_OK:
            ...
            break;
        default:
            break;
    }
}
```


```c
UDSClientPoll() {
  client->fn(UDS_EVT_POLL);
  if (client->state == kRequestStateIdle) {
    client->fn(UDS_EVT_Idle);
  } elif (client->state == kRequestStateSending) {
    ...
  } elif (client->state == kRequestStateAwaitSendComplete) {

  } elif (client->state == kRequestStateAwaitResponse) {

  } elif (client->state == kRequestState)
  
}
```

```plantuml
@startuml
@enduml

state Unconfigured
state Idle
state Sending
state AwaitSendComplete
state AwaitResponse
state ProcessResponse

[*] -> Unconfigured
Unconfigured -[#red]> Idle : UDSClientInit();
Idle->Sending: UDSClientSendXYZ() -> SendRequest();

```


# Design Docs


## ISO-TP interface

`iso14229` supports opaque transports. Use `Iso14229TpHandle_t` to wrap a transport. 


### PDU transmission complete

ISO14229-1 2013 6.1 describes a request-confirmation primitive to "indicate that the date passed in the service request primitive is successfully sent on the vehicle communication bus the diagnostic tester is connected to"


#### ISOTP-C
- **polling**
- `IsoTpLink.send_status` is either `IDLE`, `INPROGRESS`, or `ERROR`

#### PCAN-ISO-TP
- **polling**
 - https://www.peak-system.com/PCAN-ISO-TP-API.372.0.html
 - `PCAN-ISO-TP_2016.h` contains a function for reading the transport status which includes `PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_TX`

#### linux kernel isotp driver
- **blocking**
- https://github.com/hartkopp/can-isotp/issues/27 (Get status of transmission?)
- https://github.com/hartkopp/can-isotp/issues/51


If you're using the linux kernel driver, then you have threads and can use the excellent `python-udsoncan` to implement a client.

----
- "The Functional addressing is applied only to single frame transmission" -- Specification of Diagnostic Communication (Diagnostic on CAN - Network Layer)
- 

```plantuml
@startuml

@enduml
```

## Client State Machine

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
    UDSClientPoll(client);
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

## Server 0x78 requestCorrectlyReceived-ResponsePending

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
