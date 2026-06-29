# protocol

server(request):

```c
// 格式
{
    "version": "1.0",
    "id": "10001",
    "session-id": "uuid",
    "type": "request",
    "action": "collect",
    "timestamp": 1751200000,
    "payload": {}
}
```

client(response):

```c
{
    "version":"1.0",
    "id":"1003",
    "session-id": "uuid",
    "type":"response",
    "status":"ok",
    "payload":{
        ...
    }
}
```

