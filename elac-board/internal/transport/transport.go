package transport

import (
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// Transport WebSocket 传输层，负责底层连接管理
type Transport struct {
	url    string
	conn   *websocket.Conn
	mu     sync.Mutex
	logger *log.Logger
}

// NewTransport 创建新的传输层实例
func NewTransport(url string) *Transport {
	return &Transport{
		url:    url,
		logger: log.New(os.Stdout, "[Transport] ", log.LstdFlags|log.Lmsgprefix),
	}
}

// Connect 建立 WebSocket 连接
func (t *Transport) Connect() error {
	dialer := websocket.Dialer{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true, // 生产环境请使用有效证书
		},
		HandshakeTimeout: 10 * time.Second,
	}

	t.logger.Printf("正在连接 WebSocket 服务器: %s", t.url)

	conn, _, err := dialer.Dial(t.url, http.Header{})
	if err != nil {
		return fmt.Errorf("连接失败: %w", err)
	}

	t.conn = conn
	t.logger.Println("WebSocket 连接成功")
	return nil
}

// ReadMessage 读取消息
func (t *Transport) ReadMessage() ([]byte, error) {
	_, message, err := t.conn.ReadMessage()
	if err != nil {
		if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
			t.logger.Println("服务器正常关闭连接")
		} else {
			t.logger.Printf("读取消息错误: %v", err)
		}
		return nil, err
	}

	t.logger.Println("========================================")
	t.logger.Println("收到消息:")
	t.logger.Printf("%s", t.PrettyJSON(message))

	return message, nil
}

// WriteMessage 发送消息
func (t *Transport) WriteMessage(data []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	t.logger.Println("----------------------------------------")
	t.logger.Println("发送消息:")
	t.logger.Printf("%s", t.PrettyJSON(data))

	if err := t.conn.WriteMessage(websocket.TextMessage, data); err != nil {
		return fmt.Errorf("发送消息错误: %w", err)
	}

	return nil
}

// Close 关闭连接
func (t *Transport) Close() error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.conn != nil {
		t.logger.Println("正在关闭 WebSocket 连接...")
		
		// 发送关闭消息
		closeMsg := websocket.FormatCloseMessage(websocket.CloseNormalClosure, "client closing")
		if err := t.conn.WriteMessage(websocket.CloseMessage, closeMsg); err != nil {
			t.logger.Printf("发送关闭消息失败: %v", err)
		}
		
		if err := t.conn.Close(); err != nil {
			return fmt.Errorf("关闭连接失败: %w", err)
		}
		
		t.logger.Println("WebSocket 连接已关闭")
	}

	return nil
}

// IsConnected 检查连接状态
func (t *Transport) IsConnected() bool {
	return t.conn != nil
}

// PrettyJSON 格式化 JSON 字符串
func (t *Transport) PrettyJSON(data []byte) string {
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
