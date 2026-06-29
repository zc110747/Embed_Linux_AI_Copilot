#!/usr/bin/env python3
"""
增强版WebSocket测试服务端
- 支持Web界面
- 支持批量命令
- 命令历史记录
- 统计分析
"""

import asyncio
import json
import uuid
import logging
import time
from datetime import datetime
from collections import defaultdict
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading
import websockets

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class CommandStats:
    """命令统计"""
    def __init__(self):
        self.total_commands = 0
        self.successful = 0
        self.failed = 0
        self.commands_history = []
        self.response_times = []
        
    def add_command(self, cmd_id, command, code, response_time):
        self.total_commands += 1
        if code == 0:
            self.successful += 1
        else:
            self.failed += 1
        
        self.commands_history.append({
            "id": cmd_id,
            "command": command,
            "code": code,
            "time": response_time,
            "timestamp": datetime.now().isoformat()
        })
        
        self.response_times.append(response_time)
        if len(self.response_times) > 100:
            self.response_times.pop(0)
    
    def get_average_time(self):
        if not self.response_times:
            return 0
        return sum(self.response_times) / len(self.response_times)

class WebSocketServer:
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.clients = {}
        self.stats = CommandStats()
        self.web_port = 8081
        
    async def handle_client(self, websocket, path):
        """处理客户端连接"""
        client_id = str(uuid.uuid4())[:8]
        client_info = {
            "id": client_id,
            "websocket": websocket,
            "connected_at": datetime.now(),
            "ip": websocket.remote_address[0],
            "commands_executed": 0
        }
        self.clients[client_id] = client_info
        
        logger.info(f"✅ Client connected: {client_id} from {client_info['ip']}")
        self.print_client_stats()
        
        try:
            # 发送测试命令序列
            await self.send_test_sequence(websocket, client_id)
            
            # 启动命令输入和消息接收
            send_task = asyncio.create_task(self.command_input_loop(websocket))
            recv_task = asyncio.create_task(self.message_receiver(websocket, client_id))
            
            done, pending = await asyncio.wait(
                [send_task, recv_task],
                return_when=asyncio.FIRST_COMPLETED
            )
            
            for task in pending:
                task.cancel()
                
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"🔌 Client {client_id} disconnected")
        except Exception as e:
            logger.error(f"❌ Error with client {client_id}: {e}")
        finally:
            del self.clients[client_id]
            self.print_client_stats()
    
    async def send_test_sequence(self, websocket, client_id):
        """发送测试命令序列"""
        test_commands = [
            "uname -a",
            "whoami",
            "pwd",
            "date"
        ]
        
        logger.info(f"📤 Sending test sequence to {client_id}...")
        for cmd in test_commands:
            cmd_id = str(uuid.uuid4())[:8]
            message = {
                "id": cmd_id,
                "type": "shell",
                "cmd": cmd
            }
            await websocket.send(json.dumps(message))
            logger.info(f"   Sent [{cmd_id}]: {cmd}")
            await asyncio.sleep(0.5)
        
        logger.info("✅ Test sequence complete")
        logger.info("-" * 50)
    
    async def command_input_loop(self, websocket):
        """命令输入循环"""
        logger.info("\n📝 Interactive command mode:")
        logger.info("   Type commands to execute on clients")
        logger.info("   Special commands:")
        logger.info("     /clients  - List connected clients")
        logger.info("     /stats    - Show statistics")
        logger.info("     /batch    - Send batch commands")
        logger.info("     /quit     - Exit")
        logger.info("-" * 50)
        
        loop = asyncio.get_event_loop()
        
        while True:
            try:
                command = await loop.run_in_executor(
                    None, input, "\n💻 Command> "
                )
                
                if not command.strip():
                    continue
                
                # 处理特殊命令
                if command.startswith('/'):
                    await self.handle_special_command(command, websocket)
                    continue
                
                # 发送普通命令
                cmd_id = str(uuid.uuid4())[:8]
                message = {
                    "id": cmd_id,
                    "type": "shell",
                    "cmd": command
                }
                
                await websocket.send(json.dumps(message))
                logger.info(f"📤 Sent [{cmd_id}]: {command}")
                
            except EOFError:
                break
            except Exception as e:
                logger.error(f"❌ Error: {e}")
                break
    
    async def handle_special_command(self, command, websocket):
        """处理特殊命令"""
        cmd = command.lower()
        
        if cmd == '/clients':
            self.print_client_stats()
        elif cmd == '/stats':
            self.print_stats()
        elif cmd == '/batch':
            await self.send_batch_commands(websocket)
        elif cmd == '/quit':
            logger.info("👋 Exiting...")
            return
        else:
            logger.warning(f"Unknown command: {command}")
    
    async def send_batch_commands(self, websocket):
        """发送批量命令"""
        loop = asyncio.get_event_loop()
        
        logger.info("📦 Batch command mode (empty line to send, 'cancel' to abort):")
        commands = []
        
        while True:
            try:
                cmd = await loop.run_in_executor(None, input, "   Batch> ")
                
                if cmd.lower() == 'cancel':
                    logger.info("❌ Batch cancelled")
                    return
                
                if not cmd.strip():
                    if not commands:
                        logger.warning("No commands to send")
                        return
                    break
                
                commands.append(cmd)
                
            except EOFError:
                break
        
        logger.info(f"📤 Sending {len(commands)} batch commands...")
        for cmd in commands:
            cmd_id = str(uuid.uuid4())[:8]
            message = {
                "id": cmd_id,
                "type": "shell",
                "cmd": cmd
            }
            await websocket.send(json.dumps(message))
            await asyncio.sleep(0.2)
        
        logger.info("✅ Batch commands sent")
    
    async def message_receiver(self, websocket, client_id):
        """接收客户端消息"""
        start_times = {}
        
        async for message in websocket:
            try:
                data = json.loads(message)
                cmd_id = data.get('id', 'unknown')
                code = data.get('code', -1)
                stdout = data.get('stdout', '')
                stderr = data.get('stderr', '')
                
                # 计算响应时间
                response_time = time.time() - start_times.get(cmd_id, time.time())
                
                # 更新统计
                self.stats.add_command(cmd_id, data.get('cmd', 'unknown'), code, response_time)
                
                # 更新客户端信息
                if client_id in self.clients:
                    self.clients[client_id]['commands_executed'] += 1
                
                # 格式化输出
                self.print_response(cmd_id, code, stdout, stderr, response_time)
                
            except json.JSONDecodeError:
                logger.error(f"❌ Invalid JSON: {message[:100]}")
            except Exception as e:
                logger.error(f"❌ Error processing message: {e}")
    
    def print_response(self, cmd_id, code, stdout, stderr, response_time):
        """格式化打印响应"""
        print(f"\n{'='*60}")
        print(f"📥 Response [{cmd_id}] - Time: {response_time:.2f}s")
        print(f"   Exit Code: {code}")
        
        if stdout:
            print(f"   📊 STDOUT:")
            for line in stdout.strip().split('\n'):
                print(f"      {line}")
        
        if stderr:
            print(f"   ⚠️  STDERR:")
            for line in stderr.strip().split('\n'):
                print(f"      {line}")
        
        print(f"{'='*60}")
    
    def print_client_stats(self):
        """打印客户端统计"""
        print(f"\n{'='*60}")
        print(f"📊 Connected Clients: {len(self.clients)}")
        for client_id, info in self.clients.items():
            print(f"   • {client_id} from {info['ip']} - {info['commands_executed']} commands")
        print(f"{'='*60}")
    
    def print_stats(self):
        """打印统计信息"""
        stats = self.stats
        print(f"\n{'='*60}")
        print(f"📊 Command Statistics:")
        print(f"   Total: {stats.total_commands}")
        print(f"   Successful: {stats.successful}")
        print(f"   Failed: {stats.failed}")
        print(f"   Avg Response Time: {stats.get_average_time():.3f}s")
        print(f"{'='*60}")
    
    async def start(self):
        """启动服务器"""
        logger.info(f"🚀 Starting WebSocket server on ws://{self.host}:{self.port}")
        
        async with websockets.serve(
            self.handle_client, 
            self.host, 
            self.port,
            ping_interval=30,
            ping_timeout=10
        ):
            await asyncio.Future()

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Enhanced WebSocket Test Server')
    parser.add_argument('--host', default='0.0.0.0', help='Bind address')
    parser.add_argument('--port', type=int, default=8080, help='WebSocket port')
    
    args = parser.parse_args()
    
    server = WebSocketServer(host=args.host, port=args.port)
    
    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        logger.info("\n👋 Server stopped")

if __name__ == "__main__":
    main()