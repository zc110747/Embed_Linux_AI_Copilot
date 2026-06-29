package main

import (
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// Request 服务器请求消息结构
type Request struct {
	Version   string          `json:"version"`
	ID        string          `json:"id"`
	SessionID string          `json:"session-id"`
	Type      string          `json:"type"`
	Action    string          `json:"action"`
	Timestamp int64           `json:"timestamp"`
	Payload   json.RawMessage `json:"payload"`
}

// Response 客户端响应消息结构
type Response struct {
	Version   string      `json:"version"`
	ID        string      `json:"id"`
	SessionID string      `json:"session-id"`
	Type      string      `json:"type"`
	Status    string      `json:"status"`
	Payload   interface{} `json:"payload"`
}

// WebSocketClient WebSocket 客户端
type WebSocketClient struct {
	url       string
	conn      *websocket.Conn
	mu        sync.Mutex
	done      chan struct{}
	sessionID string
	logger    *log.Logger
}

// NewWebSocketClient 创建新的 WebSocket 客户端
func NewWebSocketClient(url string) *WebSocketClient {
	return &WebSocketClient{
		url:    url,
		done:   make(chan struct{}),
		logger: log.New(os.Stdout, "[WS-Client] ", log.LstdFlags|log.Lmsgprefix),
	}
}

// Connect 连接 WebSocket 服务器
func (c *WebSocketClient) Connect() error {
	// 配置 TLS，跳过证书验证（生产环境请使用有效证书）
	dialer := websocket.Dialer{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true,
		},
		HandshakeTimeout: 10 * time.Second,
	}

	c.logger.Printf("正在连接 WebSocket 服务器: %s", c.url)

	conn, _, err := dialer.Dial(c.url, http.Header{})
	if err != nil {
		return fmt.Errorf("连接失败: %w", err)
	}

	c.conn = conn
	c.logger.Println("WebSocket 连接成功")
	return nil
}

// Listen 监听服务器消息
func (c *WebSocketClient) Listen() {
	defer close(c.done)

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				c.logger.Println("服务器正常关闭连接")
			} else {
				c.logger.Printf("读取消息错误: %v", err)
			}
			return
		}

		// 格式化打印原始 JSON
		c.logger.Println("========================================")
		c.logger.Println("收到服务器请求:")
		c.logger.Printf("%s", c.prettyJSON(message))

		// 处理请求
		response, err := c.handleRequest(message)
		if err != nil {
			c.logger.Printf("处理请求错误: %v", err)
			c.sendError("processing_error", err.Error())
			continue
		}

		// 发送响应
		if err := c.SendResponse(response); err != nil {
			c.logger.Printf("发送响应错误: %v", err)
			return
		}
	}
}

// handleRequest 处理服务器请求并生成响应
func (c *WebSocketClient) handleRequest(message []byte) (*Response, error) {
	var req Request
	if err := json.Unmarshal(message, &req); err != nil {
		return nil, fmt.Errorf("JSON 解析错误: %w", err)
	}

	// 更新 session ID
	if req.SessionID != "" {
		c.sessionID = req.SessionID
	}

	c.logger.Printf("请求类型: %s", req.Type)
	c.logger.Printf("请求动作: %s", req.Action)
	c.logger.Printf("额外信息: %s", req.Payload)
	c.logger.Printf("请求 ID: %s", req.ID)
	c.logger.Printf("Session ID: %s", req.SessionID)

	// 根据 action 处理不同的请求
	switch req.Action {
	case "collect":
		return c.handleCollect(&req)
	case "ping":
		return c.handlePing(&req)
	default:
		return c.handleDefault(&req)
	}
}

// handleCollect 处理 collect 请求
func (c *WebSocketClient) handleCollect(req *Request) (*Response, error) {
	c.logger.Println("处理 collect 请求...")

	// 模拟数据采集
	payload := map[string]interface{}{
		"cpu_usage":    45.2,
		"memory_usage": 62.8,
		"disk_usage":   73.1,
		"uptime":       86400,
		"collected_at": time.Now().Unix(),
	}

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: c.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload:   payload,
	}, nil
}

// handlePing 处理 ping 请求
func (c *WebSocketClient) handlePing(req *Request) (*Response, error) {
	c.logger.Println("处理 ping 请求...")

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: c.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload: map[string]interface{}{
			"message": "pong",
			"time":    time.Now().Unix(),
		},
	}, nil
}

// handleDefault 处理未知请求
func (c *WebSocketClient) handleDefault(req *Request) (*Response, error) {
	c.logger.Printf("未知请求动作: %s，返回默认响应", req.Action)

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: c.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload: map[string]interface{}{
			"message": fmt.Sprintf("action '%s' processed", req.Action),
		},
	}, nil
}

// sendError 发送错误响应
func (c *WebSocketClient) sendError(errorCode, errorMessage string) {
	resp := &Response{
		Version:   "1.0",
		ID:        "error",
		SessionID: c.sessionID,
		Type:      "response",
		Status:    "error",
		Payload: map[string]interface{}{
			"code":    errorCode,
			"message": errorMessage,
		},
	}
	c.SendResponse(resp)
}

// SendResponse 发送响应消息
func (c *WebSocketClient) SendResponse(resp *Response) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	data, err := json.Marshal(resp)
	if err != nil {
		return fmt.Errorf("JSON 序列化错误: %w", err)
	}

	c.logger.Println("----------------------------------------")
	c.logger.Println("发送响应:")
	c.logger.Printf("%s", c.prettyJSON(data))

	if err := c.conn.WriteMessage(websocket.TextMessage, data); err != nil {
		return fmt.Errorf("发送消息错误: %w", err)
	}

	return nil
}

// prettyJSON 格式化 JSON 字符串
func (c *WebSocketClient) prettyJSON(data []byte) string {
	var obj interface{}
	if err := json.Unmarshal(data, &obj); err != nil {
		return string(data)
	}

	pretty, err := json.MarshalIndent(obj, "", "  ")
	if err != nil {
		return string(data)
	}

	return string(pretty)
}

// Close 关闭连接
func (c *WebSocketClient) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		c.logger.Println("正在关闭 WebSocket 连接...")
		// 发送关闭消息
		closeMsg := websocket.FormatCloseMessage(websocket.CloseNormalClosure, "client closing")
		c.conn.WriteMessage(websocket.CloseMessage, closeMsg)
		c.conn.Close()
		c.logger.Println("WebSocket 连接已关闭")
	}
}

// Reconnect 重连机制
func (c *WebSocketClient) Reconnect(maxRetries int, retryInterval time.Duration) error {
	for i := 0; i < maxRetries; i++ {
		c.logger.Printf("尝试重连 (%d/%d)...", i+1, maxRetries)

		if err := c.Connect(); err != nil {
			c.logger.Printf("重连失败: %v", err)
			time.Sleep(retryInterval)
			continue
		}

		c.logger.Println("重连成功")
		return nil
	}

	return fmt.Errorf("重连失败，已达到最大重试次数 %d", maxRetries)
}

func main() {
	// WebSocket 服务器地址
	// 示例: ws://localhost:8080/ws 或 wss://example.com/ws
	serverURL := "ws://localhost:8080/ws"

	// 创建客户端
	client := NewWebSocketClient(serverURL)

    log.Println("start client...")

	// 连接服务器
	if err := client.Connect(); err != nil {
		log.Fatalf("连接失败: %v", err)
	}
	defer client.Close()

	// 捕获中断信号
	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	// 启动监听
	go client.Listen()

	log.Println("========================================")
	log.Println("WebSocket 客户端已启动，按 Ctrl+C 退出")
	log.Println("========================================")

	// 等待中断信号或连接关闭
	select {
	case <-interrupt:
		log.Println("收到中断信号，正在退出...")
	case <-client.done:
		log.Println("WebSocket 连接已断开")

		// 尝试重连
		log.Println("尝试重新连接...")
		if err := client.Reconnect(3, 2*time.Second); err != nil {
			log.Printf("重连失败: %v", err)
			return
		}

		// 重新开始监听
		go client.Listen()

		// 再次等待中断信号
		<-interrupt
		log.Println("收到中断信号，正在退出...")
	}
}
