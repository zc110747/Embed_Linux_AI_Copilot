package utils

import "encoding/json"

// PrettyJSON 格式化 JSON 字符串
func PrettyJSON(data []byte) string {
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

// MarshalJSON 序列化为 JSON
func MarshalJSON(v interface{}) ([]byte, error) {
	return json.Marshal(v)
}

// UnmarshalJSON 反序列化 JSON
func UnmarshalJSON(data []byte, v interface{}) error {
	return json.Unmarshal(data, v)
}
