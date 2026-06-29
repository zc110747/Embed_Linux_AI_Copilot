package collector

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"time"

	"elac-board/internal/protocol"
)

// =============================
// Collector DSL
// =============================

type Collector struct {
	Name    string            `json:"name"`
	Version string            `json:"version"`
	Params  map[string]any    `json:"params"`
	Steps   []Step            `json:"steps"`
}

type Step struct {
	Name  string `json:"name"`
	Type  string `json:"type"`  // exec | analyze
	Input string `json:"input"`
}


// =============================
// Step Result
// =============================

type StepResult struct {
	Name   string `json:"name"`
	Type   string `json:"type"`
	Status string `json:"status"`
	Output string `json:"output,omitempty"`
	Error  string `json:"error,omitempty"`
}


// =============================
// Final Result
// =============================

type Result struct {
	Collector string        `json:"collector"`
	Version   string        `json:"version"`
	RequestID string        `json:"request_id"`
	Steps     []StepResult  `json:"steps"`
	CreatedAt int64         `json:"created_at"`
	Analysis  string        `json:"analysis,omitempty"`
}


// =============================
// ⭐ 上层唯一入口
// =============================

func CollectHandler(req *protocol.Request) (interface{}, error) {

	var payload map[string]any

	err := json.Unmarshal(req.Payload, &payload)
	if err != nil {
		return nil, err
	}

	collector, err := loadCollector(payload)
	if err != nil {
		return nil, err
	}

	return executeCollector(payload, req.ID, collector), nil
}


// =============================
// Load Collector JSON
// =============================

func loadCollector(payload map[string]interface{}) (Collector, error) {

	name, ok := payload["collector"].(string)
	if !ok {
		return Collector{}, fmt.Errorf("collector not found in payload")
	}

	// 这里模拟：实际应从文件/registry加载
	if name == "i2c-diagnose" {
		c, err := loadI2CCollector()
		if err != nil {
			return Collector{}, err
		}
		return c, nil
	}

	return Collector{}, fmt.Errorf("unknown collector: %s", name)
}


// =============================
// Collector Execution Engine
// =============================

func executeCollector(payload map[string]interface{}, id string,  c Collector) Result {

	r := Result{
		Collector: c.Name,
		Version:   c.Version,
		RequestID: id,
		Steps:     []StepResult{},
		CreatedAt: time.Now().Unix(),
	}

	mergedParams := mergeParams(c.Params, payload)

	for _, step := range c.Steps {
		res := executeStep(step, mergedParams)
		r.Steps = append(r.Steps, res)
	}

	r.Analysis = simpleAnalyze(r.Steps)

	return r
}


// =============================
// Step Executor
// =============================

func executeStep(step Step, params map[string]any) StepResult {

	input := render(step.Input, params)

	switch step.Type {

	case "exec":
		out, err := execShell(input)
		if err != nil {
			return StepResult{
				Name: step.Name,
				Type: step.Type,
				Status: "fail",
				Error: err.Error(),
			}
		}
		return StepResult{
			Name: step.Name,
			Type: step.Type,
			Status: "ok",
			Output: out,
		}

	case "analyze":
		return StepResult{
			Name: step.Name,
			Type: step.Type,
			Status: "ok",
			Output: "analysis done",
		}

	default:
		return StepResult{
			Name: step.Name,
			Type: step.Type,
			Status: "unknown",
			Error: "unsupported step type",
		}
	}
}


// =============================
// Shell Executor
// =============================

func execShell(cmd string) (string, error) {

	c := exec.Command("sh", "-c", cmd)

	out, err := c.CombinedOutput()

	return strings.TrimSpace(string(out)), err
}


// =============================
// Template Engine {{bus}}
// =============================

func render(input string, params map[string]any) string {

	for k, v := range params {
		placeholder := "{{" + k + "}}"
		input = strings.ReplaceAll(input, placeholder, fmt.Sprintf("%v", v))
	}

	return input
}


// =============================
// Params Merge
// =============================

func mergeParams(a, b map[string]any) map[string]any {

	out := map[string]any{}

	for k, v := range a {
		out[k] = v
	}

	for k, v := range b {
		out[k] = v
	}

	return out
}


// =============================
// Simple Analyzer
// =============================

func simpleAnalyze(steps []StepResult) string {

	for _, s := range steps {
		if s.Status == "fail" {
			return "i2c diagnose failed: check bus or driver"
		}
	}

	return "i2c diagnose OK"
}


// =============================
// Collector Definition (i2c)
// =============================

func loadI2CCollector() (Collector, error) {

	filePath := "configs/features/i2c-diagnose.json"

	data, err := os.ReadFile(filePath)
	if err != nil {
		return Collector{}, fmt.Errorf("failed to read collector file: %w", err)
	}

	var c Collector
	if err := json.Unmarshal(data, &c); err != nil {
		return Collector{}, fmt.Errorf("invalid collector json: %w", err)
	}

	return c, nil
}