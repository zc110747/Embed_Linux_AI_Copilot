package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"time"
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

// Protocol 协议处理器
type Protocol struct {
	transport *Transport
	sessionID string
	logger    *log.Logger
	handlers  map[string]ActionHandler
}

// ActionHandler 动作处理函数类型
type ActionHandler func(req *Request) (*Response, error)

// NewProtocol 创建协议处理器
func NewProtocol(transport *Transport) *Protocol {
	p := &Protocol{
		transport: transport,
		logger:    log.New(os.Stdout, "[Protocol] ", log.LstdFlags|log.Lmsgprefix),
		handlers:  make(map[string]ActionHandler),
	}

	// 注册默认处理器
	p.RegisterHandler("collect", p.handleCollect)
	p.RegisterHandler("ping", p.handlePing)
	p.RegisterHandler("get_status", p.handleGetStatus)

	return p
}

// RegisterHandler 注册动作处理器
func (p *Protocol) RegisterHandler(action string, handler ActionHandler) {
	p.handlers[action] = handler
	p.logger.Printf("注册处理器: %s", action)
}

// ProcessMessage 处理接收到的消息
func (p *Protocol) ProcessMessage(message []byte) (*Response, error) {
	var req Request
	if err := json.Unmarshal(message, &req); err != nil {
		return nil, fmt.Errorf("JSON 解析错误: %w", err)
	}

	// 更新 session ID
	if req.SessionID != "" {
		p.sessionID = req.SessionID
	}

	p.logger.Printf("处理请求 - 类型: %s, 动作: %s, ID: %s, Session: %s",
		req.Type, req.Action, req.ID, req.SessionID)

	// 查找并执行处理器
	handler, exists := p.handlers[req.Action]
	if !exists {
		p.logger.Printf("未找到处理器: %s，使用默认处理器", req.Action)
		return p.handleDefault(&req)
	}

	return handler(&req)
}

// SendResponse 发送响应消息
func (p *Protocol) SendResponse(resp *Response) error {
	data, err := json.Marshal(resp)
	if err != nil {
		return fmt.Errorf("JSON 序列化错误: %w", err)
	}

	return p.transport.WriteMessage(data)
}

// CreateErrorResponse 创建错误响应
func (p *Protocol) CreateErrorResponse(requestID, errorCode, errorMessage string) *Response {
	return &Response{
		Version:   "1.0",
		ID:        requestID,
		SessionID: p.sessionID,
		Type:      "response",
		Status:    "error",
		Payload: map[string]interface{}{
			"code":    errorCode,
			"message": errorMessage,
			"time":    time.Now().Unix(),
		},
	}
}

// handleCollect 处理 collect 请求
func (p *Protocol) handleCollect(req *Request) (*Response, error) {
	p.logger.Println("执行 collect 处理器")

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
		SessionID: p.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload:   payload,
	}, nil
}

// handlePing 处理 ping 请求
func (p *Protocol) handlePing(req *Request) (*Response, error) {
	p.logger.Println("执行 ping 处理器")

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: p.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload: map[string]interface{}{
			"message": "pong",
			"time":    time.Now().Unix(),
		},
	}, nil
}

// handleGetStatus 处理状态查询请求
func (p *Protocol) handleGetStatus(req *Request) (*Response, error) {
	p.logger.Println("执行 get_status 处理器")

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: p.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload: map[string]interface{}{
			"status":    "running",
			"uptime":    "2h 30m",
			"version":   "1.0.0",
			"connected": p.transport.IsConnected(),
		},
	}, nil
}

// handleDefault 处理未知请求
func (p *Protocol) handleDefault(req *Request) (*Response, error) {
	p.logger.Printf("执行默认处理器 (action: %s)", req.Action)

	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: p.sessionID,
		Type:      "response",
		Status:    "ok",
		Payload: map[string]interface{}{
			"message": fmt.Sprintf("action '%s' processed successfully", req.Action),
			"time":    time.Now().Unix(),
		},
	}, nil
}
