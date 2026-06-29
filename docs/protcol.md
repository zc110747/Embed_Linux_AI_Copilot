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

目录结构

```c
board-agent/
│
├── cmd/
│   └── main.go                  // 程序入口
│
├── configs/
│   └── board.yaml
│
├── internal/
│
│   ├── app/
│   │   ├── app.go               // Agent初始化
│   │   └── config.go            // 配置加载
│   │
│   ├── transport/
│   │   ├── websocket.go         // WebSocket客户端
│   │   └── heartbeat.go
│   │
│   ├── protocol/
│   │   ├── message.go
│   │   ├── request.go
│   │   ├── response.go
│   │   └── error.go
│   │
│   ├── runtime/
│   │   ├── dispatcher.go        // 消息分发
│   │   ├── registry.go          // Collector注册
│   │   └── workflow.go          // 预留Workflow
│   │
│   ├── collector/
│   │   ├── collector.go
│   │   ├── dmesg.go
│   │   ├── meminfo.go
│   │   ├── interrupt.go
│   │   ├── network.go
│   │   ├── version.go
│   │   └── shell.go             // 临时放这里
│   │
│   └── utils/
│       ├── exec.go
│       ├── file.go
│       └── json.go
│
├── docs/
│   ├── protocol.md
│   └── architecture.md
│
├── go.mod
└── README.md
```