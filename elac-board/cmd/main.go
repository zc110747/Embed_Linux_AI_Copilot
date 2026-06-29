package main

import (
	"os"
	"os/signal"

	"elac-board/internal/app"
	"elac-board/internal/utils"
)

func main() {
	logger := utils.NewLogger("[MAIN] ", utils.LevelInfo)

	application, err := app.New("configs/board.yaml")
	if err != nil {
		logger.Fatal("应用初始化失败: %v", err)
	}

	logger.Info("启动客户端...")

	if err := application.Start(); err != nil {
		logger.Fatal("启动失败: %v", err)
	}
	defer application.Stop()

	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	logger.Info("========================================")
	logger.Info("ELAC Board 客户端已启动，按 Ctrl+C 退出")
	logger.Info("========================================")

	select {
	case <-interrupt:
		logger.Info("收到中断信号，正在退出...")
	case <-application.Done():
		logger.Warn("WebSocket 连接已断开")
		logger.Info("尝试重新连接...")
		if err := application.Reconnect(); err != nil {
			logger.Error("重连失败: %v", err)
			return
		}
		<-interrupt
		logger.Info("收到中断信号，正在退出...")
	}
}
