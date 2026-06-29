package app

import (
	"fmt"
	"os"
	"time"

	"gopkg.in/yaml.v3"

	"elac-board/internal/collector"
	"elac-board/internal/protocol"
	"elac-board/internal/transport"
	"elac-board/internal/utils"
)

// App 应用主结构
type App struct {
	config   *Config
	wsClient *transport.WebSocketClient
	logger   *utils.Logger
	done     chan struct{}
}

// Config 应用配置
type Config struct {
	Server struct {
		URL                 string `yaml:"url"`
		ReconnectMaxRetries int    `yaml:"reconnect_max_retries"`
		ReconnectInterval   int    `yaml:"reconnect_interval"`
	} `yaml:"server"`
	TLS struct {
		InsecureSkipVerify bool `yaml:"insecure_skip_verify"`
		HandshakeTimeout   int  `yaml:"handshake_timeout"`
	} `yaml:"tls"`
	Logging struct {
		Prefix string `yaml:"prefix"`
		Level  string `yaml:"level"`
	} `yaml:"logging"`
}

// New 创建应用
func New(configPath string) (*App, error) {
	cfg, err := loadConfig(configPath)
	if err != nil {
		return nil, err
	}

	level := parseLevel(cfg.Logging.Level)
	logger := utils.NewLogger(cfg.Logging.Prefix, level)

	tlsCfg := transport.TLSConfig{
		InsecureSkipVerify: cfg.TLS.InsecureSkipVerify,
		HandshakeTimeout:   time.Duration(cfg.TLS.HandshakeTimeout) * time.Second,
	}

	wsClient := transport.NewWebSocketClient(cfg.Server.URL, tlsCfg, logger)

	wsClient.SetMessageHandler(func(raw []byte) (interface{}, error) {
		return handleMessage(raw, logger)
	})

	return &App{
		config:   cfg,
		wsClient: wsClient,
		logger:   logger,
		done:     make(chan struct{}),
	}, nil
}

// Start 启动
func (a *App) Start() error {
	if err := a.wsClient.Connect(); err != nil {
		return err
	}
	go func() {
		defer close(a.done)
		a.wsClient.Listen()
	}()
	return nil
}

// Stop 停止
func (a *App) Stop() {
	a.wsClient.Close()
}

// Done 完成信号
func (a *App) Done() <-chan struct{} {
	return a.done
}

// Reconnect 重连
func (a *App) Reconnect() error {
	interval := time.Duration(a.config.Server.ReconnectInterval) * time.Second
	if err := a.wsClient.Reconnect(a.config.Server.ReconnectMaxRetries, interval); err != nil {
		return err
	}
	a.done = make(chan struct{})
	go func() {
		defer close(a.done)
		a.wsClient.Listen()
	}()
	return nil
}

// handleMessage 消息处理
func handleMessage(raw []byte, logger *utils.Logger) (interface{}, error) {
	logger.Info("收到服务器请求:")
	logger.Info("%s", utils.PrettyJSON(raw))

	req, err := protocol.ParseRequest(raw)
	if err != nil {
		logger.Error("解析请求失败: %v", err)
		return protocol.NewErrorResponse("", "parse_error", err.Error()), nil
	}

	logger.Info("请求类型: %s, 动作: %s, ID: %s, Pyload:%s", req.Type, req.Action, req.ID, utils.PrettyJSON(req.Payload))

	switch req.Action {
	case "ping":
		logger.Debug("处理 ping 请求")
		return protocol.NewResponse(req, "ok", map[string]interface{}{
			"message": "pong",
			"time":    time.Now().Unix(),
		}), nil

	case "collect":
		logger.Debug("处理 collect 请求")
		data, err := collector.CollectHandler(req)
		if err != nil {
			logger.Error("采集失败: %v", err)
			return protocol.NewErrorResponse(req.SessionID, "collect_error", err.Error()), nil
		}
		return protocol.NewResponse(req, "ok", data), nil

	default:
		logger.Warn("未知动作: %s", req.Action)
		return protocol.NewResponse(req, "ok", map[string]interface{}{
			"message": fmt.Sprintf("action '%s' processed", req.Action),
		}), nil
	}
}

// loadConfig 加载配置
func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("读取配置失败: %w", err)
	}

	cfg := &Config{}
	cfg.Server.URL = "ws://localhost:8080/ws"
	cfg.Server.ReconnectMaxRetries = 3
	cfg.Server.ReconnectInterval = 2
	cfg.TLS.InsecureSkipVerify = true
	cfg.TLS.HandshakeTimeout = 10
	cfg.Logging.Prefix = "[ELAC-Board] "
	cfg.Logging.Level = "info"

	if err := yaml.Unmarshal(data, cfg); err != nil {
		return nil, fmt.Errorf("解析配置失败: %w", err)
	}
	return cfg, nil
}

// parseLevel 解析日志级别
func parseLevel(level string) utils.LogLevel {
	switch level {
	case "debug":
		return utils.LevelDebug
	case "info":
		return utils.LevelInfo
	case "warn":
		return utils.LevelWarn
	case "error":
		return utils.LevelError
	default:
		return utils.LevelInfo
	}
}
