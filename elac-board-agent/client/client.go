package client

import (
    "fmt"
    "log"
    "time"
    
    "ws-board-agent/executor"
    "ws-board-agent/protocol"
    
    "github.com/gorilla/websocket"
)

// Client WebSocket客户端
type Client struct {
    url            string
    conn           *websocket.Conn
    reconnectDelay time.Duration
    maxRetries     int
    done           chan struct{}
    cmdTimeout     time.Duration
}

// NewClient 创建新的客户端实例
func NewClient(url string) *Client {
    return &Client{
        url:            url,
        reconnectDelay: 5 * time.Second,
        maxRetries:     10,
        done:           make(chan struct{}),
        cmdTimeout:     30 * time.Second, // 命令执行超时时间
    }
}

// Connect 连接到WebSocket服务器
func (c *Client) Connect() error {
    var err error
    retries := 0
    
    for retries < c.maxRetries {
        log.Printf("Attempting to connect to %s (attempt %d/%d)", 
            c.url, retries+1, c.maxRetries)
        
        c.conn, _, err = websocket.DefaultDialer.Dial(c.url, nil)
        if err == nil {
            log.Printf("Successfully connected to %s", c.url)
            return nil
        }
        
        retries++
        log.Printf("Connection failed: %v. Retrying in %v...", 
            err, c.reconnectDelay)
        time.Sleep(c.reconnectDelay)
    }
    
    return fmt.Errorf("failed to connect after %d attempts: %v", 
        c.maxRetries, err)
}

// Start 开始监听消息
func (c *Client) Start() error {
    if err := c.Connect(); err != nil {
        return err
    }
    
    go c.readPump()
    go c.keepAlive()
    
    return nil
}

// readPump 读取消息的主循环
func (c *Client) readPump() {
    defer close(c.done)
    
    for {
        _, message, err := c.conn.ReadMessage()
        if err != nil {
            if websocket.IsUnexpectedCloseError(err, 
                websocket.CloseGoingAway, 
                websocket.CloseNormalClosure) {
                log.Printf("Connection error: %v", err)
            }
            // 尝试重连
            if err := c.reconnect(); err != nil {
                log.Printf("Reconnection failed: %v", err)
                return
            }
            continue
        }
        
        // 处理接收到的消息
        go c.handleMessage(message)
    }
}

// handleMessage 处理服务端消息
func (c *Client) handleMessage(data []byte) {
    // 解析服务端消息
    serverMsg, err := protocol.ParseServerMessage(data)
    if err != nil {
        log.Printf("Failed to parse server message: %v", err)
        c.sendError(serverMsg.ID, fmt.Sprintf("Invalid message format: %v", err))
        return
    }
    
    log.Printf("Received command: ID=%s, Type=%s, Cmd=%s", 
        serverMsg.ID, serverMsg.Type, serverMsg.Cmd)
    
    // 根据消息类型处理
    switch serverMsg.Type {
    case "shell":
        c.executeShell(serverMsg)
    default:
        c.sendError(serverMsg.ID, 
            fmt.Sprintf("Unknown command type: %s", serverMsg.Type))
    }
}

// executeShell 执行shell命令并返回结果
func (c *Client) executeShell(msg *protocol.ServerMessage) {
    // 执行命令
    result := executor.ExecuteCommand(msg.Cmd, c.cmdTimeout)
    
    // 构建响应消息
    response := &protocol.ClientMessage{
        ID:     msg.ID,
        Code:   result.ExitCode,
        Stdout: result.Stdout,
        Stderr: result.Stderr,
    }
    
    // 发送响应
    c.sendResponse(response)
}

// sendResponse 发送响应消息
func (c *Client) sendResponse(msg *protocol.ClientMessage) {
    data, err := msg.ToJSON()
    if err != nil {
        log.Printf("Failed to marshal response: %v", err)
        return
    }
    
    if err := c.conn.WriteMessage(websocket.TextMessage, data); err != nil {
        log.Printf("Failed to send response: %v", err)
    } else {
        log.Printf("Sent response for command ID=%s, Code=%d", 
            msg.ID, msg.Code)
    }
}

// sendError 发送错误消息
func (c *Client) sendError(id string, errMsg string) {
    response := &protocol.ClientMessage{
        ID:     id,
        Code:   -1,
        Stderr: errMsg,
    }
    c.sendResponse(response)
}

// reconnect 重连机制
func (c *Client) reconnect() error {
    log.Println("Attempting to reconnect...")
    
    for {
        select {
        case <-c.done:
            return nil
        default:
            if err := c.Connect(); err == nil {
                log.Println("Reconnected successfully")
                return nil
            }
            log.Printf("Reconnection failed, retrying in %v...", 
                c.reconnectDelay)
            time.Sleep(c.reconnectDelay)
        }
    }
}

// keepAlive 保持连接活跃
func (c *Client) keepAlive() {
    ticker := time.NewTicker(30 * time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-c.done:
            return
        case <-ticker.C:
            if err := c.conn.WriteMessage(
                websocket.PingMessage, nil); err != nil {
                log.Printf("Ping failed: %v", err)
                return
            }
        }
    }
}

// Close 关闭客户端连接
func (c *Client) Close() error {
    if c.conn != nil {
        return c.conn.Close()
    }
    return nil
}
