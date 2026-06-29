#!/usr/bin/env python3
"""
WebSocket 测试服务器
用于测试 Go 客户端的请求-响应通信格式

安装依赖: pip install websockets
"""

import asyncio
import json
import logging
import uuid
import time
from datetime import datetime

import websockets

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='[Server] %(asctime)s %(message)s',
    datefmt='%Y/%m/%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class WebSocketTestServer:
    """WebSocket 测试服务器"""
    
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.session_id = None
        self.request_counter = 0
        
    def generate_request(self, action: str, session_id: str, payload: dict = None) -> dict:
        """生成请求消息"""
        self.request_counter += 1
        return {
            "version": "1.0",
            "id": str(10000 + self.request_counter),
            "session-id": session_id,
            "type": "request",
            "action": action,
            "timestamp": int(time.time()),
            "payload": payload or {}
        }
    
    def pretty_print_json(self, data: dict, prefix: str = ""):
        """格式化打印 JSON"""
        formatted = json.dumps(data, indent=2, ensure_ascii=False)
        for line in formatted.split('\n'):
            logger.info(f"{prefix}{line}")
    
    async def handle_client(self, websocket):
        """处理客户端连接"""
        client_info = f"{websocket.remote_address}"
        logger.info(f"========================================")
        logger.info(f"客户端已连接: {client_info}")
        
        # 生成 session ID
        self.session_id = str(uuid.uuid4())
        logger.info(f"Session ID: {self.session_id}")
        
        try:
            # 测试 1: 发送 collect 请求
            await self.send_collect_request(websocket)
            
            # 测试 2: 发送 ping 请求
            # await self.send_ping_request(websocket)
            
            # 测试 3: 发送带自定义 payload 的请求
            # await self.send_custom_request(websocket)
            
            # 测试 4: 发送 get_status 请求
            # await self.send_get_status_request(websocket)
            
            logger.info(f"========================================")
            logger.info("所有测试请求已完成")
            logger.info(f"========================================")
            
            # 保持连接，等待客户端断开
            await asyncio.sleep(3)
            
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"客户端已断开: {client_info}")
        except Exception as e:
            logger.error(f"处理客户端时出错: {e}")
        finally:
            logger.info(f"关闭客户端连接: {client_info}")
    
    async def send_collect_request(self, websocket):
        """发送 collect 请求并接收响应"""
        logger.info("----------------------------------------")
        logger.info("测试 1: 发送 collect 请求")
        
        pyload={
            "collector": "dmesg",
        }

        request = self.generate_request("collect", self.session_id, pyload)
        
        # 发送请求
        logger.info("发送请求:")
        self.pretty_print_json(request, "  ")
        await websocket.send(json.dumps(request))
        
        # 接收响应
        response_raw = await websocket.recv()
        response = json.loads(response_raw)
        
        logger.info("收到响应:")
        self.pretty_print_json(response, "  ")
        
        # 验证响应格式
        self.validate_response(response, request["id"])
        
        await asyncio.sleep(1)
    
    async def send_ping_request(self, websocket):
        """发送 ping 请求并接收响应"""
        logger.info("----------------------------------------")
        logger.info("测试 2: 发送 ping 请求")
        
        request = self.generate_request("ping", self.session_id)
        
        logger.info("发送请求:")
        self.pretty_print_json(request, "  ")
        await websocket.send(json.dumps(request))
        
        response_raw = await websocket.recv()
        response = json.loads(response_raw)
        
        logger.info("收到响应:")
        self.pretty_print_json(response, "  ")
        
        self.validate_response(response, request["id"])
        
        await asyncio.sleep(1)
    
    async def send_custom_request(self, websocket):
        """发送自定义请求（带 payload）"""
        logger.info("----------------------------------------")
        logger.info("测试 3: 发送带 payload 的自定义请求")
        
        custom_payload = {
            "command": "get_metrics",
            "parameters": {
                "cpu": True,
                "memory": True,
                "disk": True,
                "network": False
            },
            "format": "json",
            "interval": 5
        }
        
        request = self.generate_request("custom_action", self.session_id, custom_payload)
        
        logger.info("发送请求:")
        self.pretty_print_json(request, "  ")
        await websocket.send(json.dumps(request))
        
        response_raw = await websocket.recv()
        response = json.loads(response_raw)
        
        logger.info("收到响应:")
        self.pretty_print_json(response, "  ")
        
        self.validate_response(response, request["id"])
        
        await asyncio.sleep(1)
    
    async def send_get_status_request(self, websocket):
        """发送 get_status 请求"""
        logger.info("----------------------------------------")
        logger.info("测试 4: 发送 get_status 请求")
        
        request = self.generate_request("get_status", self.session_id)
        
        logger.info("发送请求:")
        self.pretty_print_json(request, "  ")
        await websocket.send(json.dumps(request))
        
        response_raw = await websocket.recv()
        response = json.loads(response_raw)
        
        logger.info("收到响应:")
        self.pretty_print_json(response, "  ")
        
        self.validate_response(response, request["id"])
        
        await asyncio.sleep(1)
    
    def validate_response(self, response: dict, request_id: str):
        """验证响应格式"""
        required_fields = ["version", "id", "session-id", "type", "status", "payload"]
        
        # 检查必要字段
        missing_fields = [f for f in required_fields if f not in response]
        if missing_fields:
            logger.warning(f"响应缺少字段: {missing_fields}")
        else:
            logger.info(f"✓ 响应格式验证通过")
        
        # 验证具体值
        checks = []
        
        if response.get("version") == "1.0":
            checks.append("✓ version")
        else:
            checks.append("✗ version")
        
        if response.get("id") == request_id:
            checks.append("✓ id")
        else:
            checks.append("✗ id")
        
        if response.get("type") == "response":
            checks.append("✓ type")
        else:
            checks.append("✗ type")
        
        if response.get("status") in ["ok", "error"]:
            checks.append("✓ status")
        else:
            checks.append("✗ status")
        
        if response.get("session-id") == self.session_id:
            checks.append("✓ session-id")
        else:
            checks.append("✗ session-id")
        
        logger.info(f"响应验证: {' | '.join(checks)}")
    
    async def start(self):
        """启动服务器"""
        logger.info(f"========================================")
        logger.info(f"WebSocket 测试服务器启动")
        logger.info(f"监听地址: ws://{self.host}:{self.port}")
        logger.info(f"等待客户端连接...")
        logger.info(f"========================================")
        
        async with websockets.serve(
            self.handle_client, 
            self.host, 
            self.port,
            ping_interval=20,
            ping_timeout=10
        ):
            await asyncio.Future()  # 永久运行


class InteractiveTestServer(WebSocketTestServer):
    """交互式测试服务器，支持手动输入请求"""
    
    async def handle_client(self, websocket):
        """处理客户端连接（交互模式）"""
        client_info = f"{websocket.remote_address}"
        logger.info(f"========================================")
        logger.info(f"客户端已连接: {client_info}")
        
        self.session_id = str(uuid.uuid4())
        logger.info(f"Session ID: {self.session_id}")
        logger.info(f"输入 'quit' 断开连接，输入 'help' 查看可用命令")
        
        try:
            while True:
                # 等待用户输入
                action = await self.get_user_input()
                
                if action is None:
                    break
                if action.lower() == 'quit':
                    logger.info("用户请求断开连接")
                    break
                if action.lower() == 'help':
                    self.show_help()
                    continue
                
                # 可选: 输入自定义 payload
                payload_str = await self.get_payload_input()
                payload = {}
                if payload_str:
                    try:
                        payload = json.loads(payload_str)
                    except json.JSONDecodeError:
                        logger.warning("无效的 JSON payload，使用空 payload")
                
                # 生成并发送请求
                request = self.generate_request(action, self.session_id, payload)
                
                logger.info("发送请求:")
                self.pretty_print_json(request, "  ")
                await websocket.send(json.dumps(request))
                
                # 接收响应
                response_raw = await websocket.recv()
                response = json.loads(response_raw)
                
                logger.info("收到响应:")
                self.pretty_print_json(response, "  ")
                self.validate_response(response, request["id"])
        
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"客户端已断开: {client_info}")
        except Exception as e:
            logger.error(f"处理客户端时出错: {e}")
        finally:
            logger.info(f"关闭客户端连接: {client_info}")
    
    async def get_user_input(self) -> str:
        """获取用户输入（异步版本）"""
        print("\n输入请求动作 (collect/ping/get_status/quit/help): ", end="", flush=True)
        try:
            return await asyncio.get_event_loop().run_in_executor(None, input)
        except EOFError:
            return None
    
    async def get_payload_input(self) -> str:
        """获取 payload 输入"""
        print("输入 payload JSON (直接回车跳过): ", end="", flush=True)
        try:
            return await asyncio.get_event_loop().run_in_executor(None, input)
        except EOFError:
            return ""
    
    def show_help(self):
        """显示帮助信息"""
        logger.info("可用命令:")
        logger.info("  collect     - 采集系统指标")
        logger.info("  ping        - 心跳检测")
        logger.info("  get_status  - 获取状态")
        logger.info("  <任意文本>   - 自定义 action")
        logger.info("  quit        - 断开连接")
        logger.info("  help        - 显示此帮助")


async def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description="WebSocket 测试服务器")
    parser.add_argument(
        "--host", 
        default="localhost", 
        help="监听地址 (默认: localhost)"
    )
    parser.add_argument(
        "--port", 
        type=int, 
        default=8080, 
        help="监听端口 (默认: 8080)"
    )
    parser.add_argument(
        "--interactive", 
        "-i", 
        action="store_true", 
        help="交互模式，手动输入请求"
    )
    
    args = parser.parse_args()
    
    if args.interactive:
        server = InteractiveTestServer(args.host, args.port)
    else:
        server = WebSocketTestServer(args.host, args.port)
    
    try:
        await server.start()
    except KeyboardInterrupt:
        logger.info("\n服务器正在关闭...")
    except asyncio.CancelledError:
        pass
    finally:
        logger.info("服务器已关闭")


if __name__ == "__main__":
    # 安装依赖提示
    try:
        import websockets
    except ImportError:
        print("请先安装 websockets 库:")
        print("  pip install websockets")
        exit(1)
    
    asyncio.run(main())
    