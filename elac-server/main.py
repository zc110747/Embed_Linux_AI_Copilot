import os
import json
import time
from datetime import datetime
from typing import Optional, List, Dict
import requests
from colorama import init, Fore, Style
from cmd_handler import ATCommandHandler

# 初始化colorama
init(autoreset=True)

def load_skills(file_path: str) -> str:
    """
    加载skills文件内容
    
    Args:
        file_path: skills文件路径
        
    Returns:
        skills文件内容字符串，加载失败返回空字符串
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        print(Fore.GREEN + f"✓ 成功加载skills文件: {file_path}")
        return content
    except FileNotFoundError:
        print(Fore.YELLOW + f"⚠ skills文件未找到: {file_path}")
        return ""
    except Exception as e:
        print(Fore.RED + f"✗ 加载skills文件失败: {str(e)}")
        return ""


class DeepSeekChat:
    """DeepSeek API 聊天客户端"""
    
    def __init__(self, api_key: str, base_url: str = "https://api.deepseek.com/v1", 
                 skills_file: str = "skills/hw_skills.md"):
        """
        初始化DeepSeek客户端
        
        Args:
            api_key: DeepSeek API密钥
            base_url: API基础URL
            skills_file: skills文件路径，用于加载系统提示词
        """
        self.api_key = api_key
        self.base_url = base_url
        self.conversation_history: List[Dict] = []
        self.headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # 加载skills文件作为系统提示词
        self.system_prompt = load_skills(skills_file)
        if self.system_prompt:
            self.send_message(self.system_prompt, add_history=True)
    
    def clear_history(self) -> None:
        """清空对话历史"""
        self.conversation_history = []
        print(Fore.YELLOW + "✓ 对话历史已清空")
    
    def send_message(self, user_input: str, add_history: bool = False, stream: bool = False, 
                     temperature: float = 0.7, max_tokens: int = 2000) -> Optional[str]:
        """
        发送消息到DeepSeek API并获取回复
        
        Args:
            user_input: 用户输入的消息
            stream: 是否使用流式输出
            temperature: 温度参数(0-1)，控制随机性
            max_tokens: 最大token数
            
        Returns:
            API的回复内容，失败返回None
        """

        # 添加用户消息到历史
        # 只有skills文件作为系统提示词时才添加，普通消息不添加
        messages = self.conversation_history[:]
        messages.append({"role": "user", "content": user_input})
        payload = {
            "model": "deepseek-chat",
            "messages": messages,
            "temperature": temperature,
            "max_tokens": max_tokens,
            "stream": stream
        }
        if add_history:
            self.conversation_history = messages

        try:
            if stream:
                return self._send_stream_request(payload)
            else:
                return self._send_normal_request(payload)
        except Exception as e:
            print(Fore.RED + f"✗ 发送消息失败: {str(e)}")
            return None
    
    def _send_normal_request(self, payload: Dict) -> Optional[str]:
        """发送普通请求（非流式）"""
        response = requests.post(
            f"{self.base_url}/chat/completions",
            headers=self.headers,
            json=payload,
            timeout=30
        )
        
        if response.status_code == 200:
            assistant_message = response.json()["choices"][0]["message"]["content"]
            return assistant_message
        else:
            print(Fore.RED + f"API错误: {response.status_code} - {response.text}")
            return None
    
    def _send_stream_request(self, payload: Dict) -> Optional[str]:
        """发送流式请求"""

        print(Fore.YELLOW + "正在发送流式请求...")

        response = requests.post(
            f"{self.base_url}/chat/completions",
            headers=self.headers,
            json=payload,
            stream=True,
            timeout=30
        )
        
        if response.status_code == 200:
            full_response = ""
            print(Fore.CYAN + "助手: ", end="", flush=True)
            
            for line in response.iter_lines():
                if line:
                    line = line.decode('utf-8')
                    if line.startswith('data: '):
                        data = line[6:]
                        if data != '[DONE]':
                            try:
                                chunk = json.loads(data)
                                if 'choices' in chunk and len(chunk['choices']) > 0:
                                    delta = chunk['choices'][0].get('delta', {})
                                    content = delta.get('content', '')
                                    if content:
                                        print(content, end="", flush=True)
                                        full_response += content
                            except json.JSONDecodeError:
                                continue
            
            print()  # 换行
            return full_response
        else:
            print(Fore.RED + f"API错误: {response.status_code}")
            return None
    
    def get_history_summary(self) -> None:
        """显示对话历史摘要"""
        if not self.conversation_history:
            print(Fore.YELLOW + "暂无对话历史")
            return
        
        print(Fore.GREEN + "\n" + "="*50)
        print(Fore.GREEN + "对话历史摘要")
        print(Fore.GREEN + "="*50)
        
        for i, msg in enumerate(self.conversation_history, 1):
            role = "用户" if msg["role"] == "user" else "助手"
            content_preview = msg["content"][:50] + "..." if len(msg["content"]) > 50 else msg["content"]
            print(f"{i}. {role}: {content_preview}")
        
        print(Fore.GREEN + "="*50 + "\n")


class InteractiveChatApp:
    """交互式聊天应用"""
    
    def __init__(self):
        self.client: Optional[DeepSeekChat] = None
        self.running = True
        self.cmd_handler = ATCommandHandler()
        
    def print_banner(self) -> None:
        """打印应用横幅"""
        banner = f"""
{Fore.CYAN}{'='*60}
{Fore.GREEN}   DeepSeek API 智能对话助手
{Fore.YELLOW}   支持周期性输入，实时返回响应
{Fore.CYAN}{'='*60}
{Fore.WHITE}
命令说明:
  /quit 或 /exit  - 退出程序
  /clear          - 清空对话历史
  /history        - 查看对话历史
  /stream on/off  - 开启/关闭流式输出
  /temp <value>   - 设置温度参数(0-1)
  /help           - 显示帮助信息
{Fore.CYAN}{'='*60}{Style.RESET_ALL}
        """
        print(banner)
    
    def setup_api_key(self) -> bool:
        """
        设置API密钥
        
        Returns:
            是否成功设置API密钥
        """
        print(Fore.YELLOW + "\n请设置DeepSeek API密钥")
        print(Fore.WHITE + "获取密钥: https://platform.deepseek.com/api_keys")
        
        # 优先从环境变量读取
        api_key = os.getenv("DEEPSEEK_API_KEY")
        
        if not api_key:
            api_key = input(Fore.WHITE + "\n请输入API密钥: ").strip()
        
        if not api_key:
            print(Fore.RED + "错误: API密钥不能为空")
            return False
        
        self.client = DeepSeekChat(api_key)
        print(Fore.GREEN + "✓ API密钥设置成功\n")
        return True
    
    def run(self) -> None:
        """运行聊天应用"""
        self.print_banner()
        
        if not self.setup_api_key():
            return
        
        # 配置参数
        use_stream = True
        temperature = 0.7
        
        print(Fore.GREEN + "进入对话模式，输入 /help 查看命令")
        print(Fore.CYAN + "-"*60 + "\n")
        
        while self.running:
            try:
                # 获取用户输入
                user_input = input(Fore.YELLOW + "你: " + Style.RESET_ALL).strip()
                
                if not user_input:
                    continue
                
                # 处理命令
                if user_input.startswith("/"):
                    use_stream, temperature = self.handle_command(user_input, use_stream, temperature)
                    continue
                
                # 发送消息并获取回复
                print(Fore.CYAN + "助手: " + Style.RESET_ALL, end="")
                response = self.client.send_message(
                    user_input, 
                    stream=use_stream,
                    temperature=temperature
                )
                
                if not response:
                    print(Fore.RED + "\n获取回复失败，请重试")
                
                if not use_stream:
                    print(response)
                
                
                self.cmd_handler.execute_command(response)

            except KeyboardInterrupt:
                print(Fore.YELLOW + "\n\n检测到中断信号，正在退出...")
                break
            except Exception as e:
                print(Fore.RED + f"发生错误: {str(e)}")
        
        print(Fore.GREEN + "\n感谢使用DeepSeek对话助手，再见！")
    
    def handle_command(self, command: str, use_stream: bool, temperature: float) -> tuple:
        """处理用户命令"""
        cmd_parts = command.lower().split()
        cmd = cmd_parts[0]
        
        if cmd in ["/quit", "/exit"]:
            self.running = False
            return use_stream, temperature
        
        elif cmd == "/clear":
            self.client.clear_history()
        
        elif cmd == "/history":
            self.client.get_history_summary()
        
        elif cmd == "/stream":
            if len(cmd_parts) > 1:
                if cmd_parts[1] == "on":
                    use_stream = True
                    print(Fore.GREEN + "✓ 流式输出已开启")
                elif cmd_parts[1] == "off":
                    use_stream = False
                    print(Fore.GREEN + "✓ 流式输出已关闭")
                else:
                    print(Fore.RED + "使用方法: /stream on/off")
            else:
                print(Fore.YELLOW + f"当前流式输出状态: {'开启' if use_stream else '关闭'}")
        
        elif cmd == "/temp":
            if len(cmd_parts) > 1:
                try:
                    new_temp = float(cmd_parts[1])
                    if 0 <= new_temp <= 1:
                        temperature = new_temp
                        print(Fore.GREEN + f"✓ 温度参数已设置为: {temperature}")
                    else:
                        print(Fore.RED + "温度参数必须在0-1之间")
                except ValueError:
                    print(Fore.RED + "请输入有效的数字")
            else:
                print(Fore.YELLOW + f"当前温度参数: {temperature}")
        
        elif cmd == "/help":
            self.print_help()
        
        else:
            print(Fore.RED + f"未知命令: {command}，输入 /help 查看可用命令")
        
        return use_stream, temperature
    
    def print_help(self) -> None:
        """打印帮助信息"""
        help_text = f"""
{Fore.GREEN}可用命令:
  /quit, /exit    - 退出程序
  /clear          - 清空当前对话历史
  /history        - 查看对话历史记录
  /stream on/off  - 开启或关闭流式输出模式
  /temp <value>   - 设置温度参数(0-1)，控制回复的创造性
  /help           - 显示此帮助信息

{Fore.GREEN}使用技巧:
  • 流式输出可以实时看到AI的回复生成过程
  • 温度参数越低，回复越确定；越高，回复越有创造性
  • 对话历史会自动保存，可以使用/clear清空重新开始
  • 支持多轮对话，AI会记住之前的上下文
        """
        print(help_text)


def main():
    """主函数"""
    # 检查requests库是否安装
    try:
        import requests
    except ImportError:
        print("请先安装requests库: pip install requests")
        return
    
    # 检查colorama库是否安装
    try:
        from colorama import init
    except ImportError:
        print("请先安装colorama库: pip install colorama")
        return
    
    # 运行应用
    app = InteractiveChatApp()
    app.run()


if __name__ == "__main__":
    main()


if __name__ == "__main__":
    main()


if __name__ == "__main__":
    main()