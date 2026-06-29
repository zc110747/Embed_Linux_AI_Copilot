# go编译方法

```shell
go mod init elac-board

go build cmd/main.go

go get github.com/gorilla/websocket

 go get gopkg.in/yaml.v3
```

## 软件格式

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
│   │   └── app.go            // 应用初始化，配置加载
│   │ 
│   ├── collector/
│   │   └── collector.go         // 支持的执行命令在此处处理
│   │
│   ├── protocol/
│   │   └── protocol.go
│   │
│   ├── runtime/
│   │   └── workflow.go          // 预留Workflow
│   │
│   ├── transport/
│   │   └── websocket.go
│   │
│   └── utils/
│       ├── logger.go  
│       └── json.go
│
├── docs/
│   ├── protocol.md
│   └── architecture.md
│
├── go.mod
└── README.md
```
