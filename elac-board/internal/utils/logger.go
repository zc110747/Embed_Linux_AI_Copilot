package utils

import (
	"fmt"
	"io"
	"log"
	"os"
)

// LogLevel 日志级别
type LogLevel int

const (
	LevelDebug LogLevel = iota
	LevelInfo
	LevelWarn
	LevelError
)

// Logger 日志工具
type Logger struct {
	logger *log.Logger
	level  LogLevel
}

// NewLogger 创建日志器
func NewLogger(prefix string, level LogLevel) *Logger {
	return &Logger{
		logger: log.New(os.Stdout, prefix, log.LstdFlags|log.Lmsgprefix),
		level:  level,
	}
}

// NewLoggerWithWriter 创建带自定义输出的日志器
func NewLoggerWithWriter(w io.Writer, prefix string, level LogLevel) *Logger {
	return &Logger{
		logger: log.New(w, prefix, log.LstdFlags|log.Lmsgprefix),
		level:  level,
	}
}

// Debug 调试日志
func (l *Logger) Debug(format string, v ...interface{}) {
	if l.level <= LevelDebug {
		l.logger.Printf("[DEBUG] "+format, v...)
	}
}

// Info 信息日志
func (l *Logger) Info(format string, v ...interface{}) {
	if l.level <= LevelInfo {
		l.logger.Printf("[INFO] "+format, v...)
	}
}

// Warn 警告日志
func (l *Logger) Warn(format string, v ...interface{}) {
	if l.level <= LevelWarn {
		l.logger.Printf("[WARN] "+format, v...)
	}
}

// Error 错误日志
func (l *Logger) Error(format string, v ...interface{}) {
	if l.level <= LevelError {
		l.logger.Printf("[ERROR] "+format, v...)
	}
}

// Fatal 致命错误日志
func (l *Logger) Fatal(format string, v ...interface{}) {
	l.logger.Printf("[FATAL] "+format, v...)
	os.Exit(1)
}

// SetLevel 设置日志级别
func (l *Logger) SetLevel(level LogLevel) {
	l.level = level
}

// LevelString 获取级别字符串
func (l *Logger) LevelString() string {
	switch l.level {
	case LevelDebug:
		return "debug"
	case LevelInfo:
		return "info"
	case LevelWarn:
		return "warn"
	case LevelError:
		return "error"
	default:
		return "unknown"
	}
}

// Printf 底层格式化输出（兼容标准库）
func (l *Logger) Printf(format string, v ...interface{}) {
	l.logger.Printf(format, v...)
}

// Println 底层输出（兼容标准库）
func (l *Logger) Println(v ...interface{}) {
	l.logger.Println(v...)
}

// String 实现 fmt.Stringer 接口
func (l *Logger) String() string {
	return fmt.Sprintf("Logger{level=%s}", l.LevelString())
}
