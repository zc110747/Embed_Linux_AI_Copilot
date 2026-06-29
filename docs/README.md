# 开发和文档说明

agent collect dmesg
agent collect journal
agent collect meminfo
agent collect interrupts
agent collect cpuinfo
agent collect mounts
agent collect modules
agent collect ps
agent collect net
agent collect lsusb
agent collect lspci
agent collect i2c
agent collect spi
agent collect rtc
agent collect clk
agent collect regulator
agent collect thermal
agent collect gpio
agent collect device-tree
agent collect config
agent collect coredump

## 项目格式

目录结构

```c
// version 0.1
elac-board/
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
│   │   ├── app.go               // 应用初始化
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
│       ├── logger.go
│       └── json.go
│ 
├── go.mod
└── README.md

// version total
elac-board/
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