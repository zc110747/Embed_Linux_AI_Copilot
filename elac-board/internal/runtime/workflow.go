package runtime

import "elac-board/internal/utils"

// Workflow 工作流（预留）
type Workflow struct {
	Name  string
	Steps []string
}

// NewWorkflow 创建工作流（预留）
func NewWorkflow(name string, logger *utils.Logger) *Workflow {
	logger.Debug("创建工作流: %s", name)
	return &Workflow{
		Name:  name,
		Steps: make([]string, 0),
	}
}
