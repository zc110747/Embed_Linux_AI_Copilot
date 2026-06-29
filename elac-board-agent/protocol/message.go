package protocol

import "encoding/json"

// ServerMessage 服务端发送的消息
type ServerMessage struct {
    ID   string `json:"id"`
    Type string `json:"type"`
    Cmd  string `json:"cmd"`
}

// ClientMessage 客户端返回的消息
type ClientMessage struct {
    ID     string `json:"id"`
    Code   int    `json:"code"`
    Stdout string `json:"stdout"`
    Stderr string `json:"stderr"`
}

// ParseServerMessage 解析服务端消息
func ParseServerMessage(data []byte) (*ServerMessage, error) {
    var msg ServerMessage
    err := json.Unmarshal(data, &msg)
    if err != nil {
        return nil, err
    }
    return &msg, nil
}

// ToJSON 将客户端消息序列化为JSON
func (m *ClientMessage) ToJSON() ([]byte, error) {
    return json.Marshal(m)
}
