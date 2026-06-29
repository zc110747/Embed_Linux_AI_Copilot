package protocol

import (
	"encoding/json"
	"fmt"
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

// ParseRequest 解析请求
func ParseRequest(data []byte) (*Request, error) {
	var req Request
	if err := json.Unmarshal(data, &req); err != nil {
		return nil, fmt.Errorf("JSON 解析错误: %w", err)
	}
	return &req, nil
}

// NewResponse 创建成功响应
func NewResponse(req *Request, status string, payload interface{}) *Response {
	return &Response{
		Version:   "1.0",
		ID:        req.ID,
		SessionID: req.SessionID,
		Type:      "response",
		Status:    status,
		Payload:   payload,
	}
}

// NewErrorResponse 创建错误响应
func NewErrorResponse(sessionID, errorCode, errorMessage string) *Response {
	return &Response{
		Version:   "1.0",
		ID:        "error",
		SessionID: sessionID,
		Type:      "response",
		Status:    "error",
		Payload: map[string]interface{}{
			"code":    errorCode,
			"message": errorMessage,
		},
	}
}
