package transport

import (
	"crypto/tls"
	"fmt"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"

	"elac-board/internal/utils"
)

// TLSConfig TLS 配置
type TLSConfig struct {
	InsecureSkipVerify bool
	HandshakeTimeout   time.Duration
}

// MessageHandler 消息处理函数类型
type MessageHandler func(message []byte) (interface{}, error)

// WebSocketClient WebSocket 客户端
type WebSocketClient struct {
	url       string
	tlsConfig TLSConfig
	conn      *websocket.Conn
	mu        sync.Mutex
	done      chan struct{}
	handler   MessageHandler
	logger    *utils.Logger
}

// NewWebSocketClient 创建客户端
func NewWebSocketClient(url string, tlsConfig TLSConfig, logger *utils.Logger) *WebSocketClient {
	return &WebSocketClient{
		url:       url,
		tlsConfig: tlsConfig,
		done:      make(chan struct{}),
		logger:    logger,
	}
}

// SetMessageHandler 设置消息处理器
func (c *WebSocketClient) SetMessageHandler(handler MessageHandler) {
	c.handler = handler
}

// Connect 连接
func (c *WebSocketClient) Connect() error {
	dialer := websocket.Dialer{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: c.tlsConfig.InsecureSkipVerify,
		},
		HandshakeTimeout: c.tlsConfig.HandshakeTimeout,
	}

	c.logger.Info("正在连接 WebSocket 服务器: %s", c.url)

	conn, _, err := dialer.Dial(c.url, http.Header{})
	if err != nil {
		return fmt.Errorf("连接失败: %w", err)
	}

	c.conn = conn
	c.logger.Info("WebSocket 连接成功")
	return nil
}

// Listen 监听
func (c *WebSocketClient) Listen() {
	defer close(c.done)

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				c.logger.Info("服务器正常关闭连接")
			} else {
				c.logger.Error("读取消息错误: %v", err)
			}
			return
		}

		if c.handler == nil {
			c.logger.Warn("消息处理器未设置，跳过消息处理")
			continue
		}

		response, err := c.handler(message)
		if err != nil {
			c.logger.Error("处理消息错误: %v", err)
			continue
		}

		if response != nil {
			if err := c.send(response); err != nil {
				c.logger.Error("发送响应错误: %v", err)
				return
			}
		}
	}
}

// send 发送消息
func (c *WebSocketClient) send(message interface{}) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	data, err := utils.MarshalJSON(message)
	if err != nil {
		return err
	}

	c.logger.Debug("发送响应:\n%s", utils.PrettyJSON(data))

	if err := c.conn.WriteMessage(websocket.TextMessage, data); err != nil {
		return fmt.Errorf("发送消息错误: %w", err)
	}
	return nil
}

// Close 关闭
func (c *WebSocketClient) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		c.logger.Info("正在关闭 WebSocket 连接...")
		closeMsg := websocket.FormatCloseMessage(websocket.CloseNormalClosure, "client closing")
		c.conn.WriteMessage(websocket.CloseMessage, closeMsg)
		c.conn.Close()
		c.logger.Info("WebSocket 连接已关闭")
	}
}

// Reconnect 重连
func (c *WebSocketClient) Reconnect(maxRetries int, retryInterval time.Duration) error {
	for i := 0; i < maxRetries; i++ {
		c.logger.Info("尝试重连 (%d/%d)...", i+1, maxRetries)
		if err := c.Connect(); err != nil {
			c.logger.Error("重连失败: %v", err)
			time.Sleep(retryInterval)
			continue
		}
		c.logger.Info("重连成功")
		return nil
	}
	return fmt.Errorf("重连失败，已达到最大重试次数 %d", maxRetries)
}
