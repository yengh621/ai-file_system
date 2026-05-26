#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
后台定时分析守护进程
- 每 10 分钟运行一次分析
- 保存分析结果到长时记忆
"""
import sys
import os
import time
import signal
from datetime import datetime
from .orchestrator import AgentOrchestrator


class AnalysisDaemon:
    def __init__(self, uid: int, interval_minutes: int = 10):
        self.uid = uid
        self.interval = interval_minutes * 60  # 转换为秒
        self.running = False
        self.orchestrator = None
        
        # PID 文件位置
        self.pid_file = os.path.join("debug_memory", "daemon.pid")
    
    def start(self):
        """启动守护进程"""
        # 检查是否已在运行
        if os.path.exists(self.pid_file):
            try:
                with open(self.pid_file, "r") as f:
                    old_pid = int(f.read().strip())
                # 检查进程是否存在（Windows 下简化处理）
                try:
                    os.kill(old_pid, 0)
                    print(f"Daemon already running (PID: {old_pid})", file=sys.stderr)
                    return False
                except:
                    pass
            except:
                pass
        
        # 写入 PID 文件
        with open(self.pid_file, "w") as f:
            f.write(str(os.getpid()))
        
        self.running = True
        self.orchestrator = AgentOrchestrator()
        self.orchestrator.set_user(self.uid)
        
        print(f"[Daemon] Started for user {self.uid}, interval: {self.interval//60} minutes")
        
        # 立即运行第一次分析
        print(f"[Daemon] Running initial analysis...")
        try:
            self.orchestrator.run_full_analysis()
        except Exception as e:
            print(f"[Daemon] Initial analysis failed: {e}")
        
        # 注册信号处理
        signal.signal(signal.SIGINT, self._stop_handler)
        signal.signal(signal.SIGTERM, self._stop_handler)
        
        # 主循环
        while self.running:
            time.sleep(self.interval)
            if self.running:
                print(f"[Daemon] Running scheduled analysis at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
                try:
                    self.orchestrator.run_full_analysis()
                except Exception as e:
                    print(f"[Daemon] Analysis failed: {e}")
        
        return True
    
    def stop(self):
        """停止守护进程"""
        self.running = False
        if os.path.exists(self.pid_file):
            try:
                with open(self.pid_file, "r") as f:
                    pid = int(f.read().strip())
                os.kill(pid, signal.SIGTERM)
            except:
                pass
            try:
                os.remove(self.pid_file)
            except:
                pass
        print("[Daemon] Stopped")
    
    def _stop_handler(self, signum, frame):
        """信号处理"""
        print(f"[Daemon] Received signal {signum}")
        self.stop()
        sys.exit(0)


def main():
    if len(sys.argv) < 2:
        print("Usage: python -m ai.daemon <start|stop> <uid>")
        return
    
    cmd = sys.argv[1]
    
    if cmd == "start" and len(sys.argv) > 2:
        uid = int(sys.argv[2])
        daemon = AnalysisDaemon(uid)
        daemon.start()
    elif cmd == "stop":
        # 尝试停止
        pid_file = os.path.join("debug_memory", "daemon.pid")
        if os.path.exists(pid_file):
            try:
                with open(pid_file, "r") as f:
                    pid = int(f.read().strip())
                os.kill(pid, signal.SIGTERM)
                os.remove(pid_file)
                print("Daemon stopped")
            except Exception as e:
                print(f"Error stopping daemon: {e}")
    else:
        print("Usage: python -m ai.daemon <start|stop> <uid>")


if __name__ == "__main__":
    main()
