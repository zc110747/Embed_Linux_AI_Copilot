package collector

import (
	"fmt"
	"os/exec"
	"runtime"
	"strings"
	"time"
)

// CollectSystemInfo 采集系统信息
func CollectSystemInfo() (interface{}, error) {
	var memStats runtime.MemStats
	runtime.ReadMemStats(&memStats)

	info := map[string]interface{}{
		"go_version":    runtime.Version(),
		"num_cpu":       runtime.NumCPU(),
		"num_goroutine": runtime.NumGoroutine(),
		"memory_mb":     memStats.Alloc / 1024 / 1024,
		"collected_at":  time.Now().Unix(),
	}

	// 采集主机名
	if hostname, err := execCommand("hostname"); err == nil {
		info["hostname"] = hostname
	}

	// 采集 uptime
	if uptime, err := execCommand("uptime"); err == nil {
		info["uptime"] = uptime
	}

	// 采集内核版本
	if kernel, err := execCommand("uname", "-r"); err == nil {
		info["kernel"] = kernel
	}

	return info, nil
}

// ExecuteCommand 执行指定的命令
func ExecuteCommand(command string, args ...string) (string, error) {
	return execCommand(command, args...)
}

// execCommand 执行命令的内部实现
func execCommand(command string, args ...string) (string, error) {
	cmd := exec.Command(command, args...)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("执行命令 '%s' 失败: %w", command, err)
	}
	return strings.TrimSpace(string(output)), nil
}
