package main

import (
    "log"
    "os"
    "os/signal"
    "syscall"
    
    "ws-board-agent/client"
)

func main() {
    // 配置WebSocket服务器地址
    serverURL := "ws://localhost:8080/ws"
    
    // 创建客户端实例
    c := client.NewClient(serverURL)
    
    // 启动客户端
    if err := c.Start(); err != nil {
        log.Fatalf("Failed to start client: %v", err)
    }
    defer c.Close()
    
    log.Println("WebSocket client is running...")
    
    // 等待中断信号
    sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
    <-sigChan
    
    log.Println("Shutting down client...")
}
