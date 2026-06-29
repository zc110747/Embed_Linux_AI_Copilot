package executor

import (
    "bytes"
    "context"
    "os/exec"
    "time"
)

// CommandResult 命令执行结果
type CommandResult struct {
    Stdout   string
    Stderr   string
    ExitCode int
}

// ExecuteCommand 执行shell命令
func ExecuteCommand(command string, timeout time.Duration) *CommandResult {
    result := &CommandResult{}
    
    // 创建带超时的context
    ctx, cancel := context.WithTimeout(context.Background(), timeout)
    defer cancel()
    
    // 创建命令
    cmd := exec.CommandContext(ctx, "sh", "-c", command)
    
    // 捕获标准输出和标准错误
    var stdout, stderr bytes.Buffer
    cmd.Stdout = &stdout
    cmd.Stderr = &stderr
    
    // 执行命令
    err := cmd.Run()
    
    result.Stdout = stdout.String()
    result.Stderr = stderr.String()
    
    if err != nil {
        // 检查是否是超时错误
        if ctx.Err() == context.DeadlineExceeded {
            result.ExitCode = -1
            result.Stderr = "Command execution timeout: " + err.Error()
        } else if exitError, ok := err.(*exec.ExitError); ok {
            result.ExitCode = exitError.ExitCode()
        } else {
            result.ExitCode = -1
        }
    } else {
        result.ExitCode = 0
    }
    
    return result
}