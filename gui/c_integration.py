#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C 端集成模块 - 通过子进程连接真实的文件系统
"""
import subprocess
import threading
import queue


class CSystemWrapper:
    """C 端系统封装类"""

    def __init__(self):
        self.process = None
        self.output_queue = queue.Queue()
        self.reading = False

    def start(self):
        """启动 C 端系统"""
        try:
            # 先编译
            import os
            if not os.path.exists("filesystem.exe"):
                if os.name == 'nt':
                    subprocess.run("build.bat", shell=True)
                else:
                    subprocess.run("make", shell=True)

            self.process = subprocess.Popen(
                ["filesystem.exe"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )

            self.reading = True
            t = threading.Thread(target=self._read_output, daemon=True)
            t.start()

            return True

        except Exception as e:
            print(f"启动 C 端失败: {e}")
            return False

    def _read_output(self):
        """读取输出的线程"""
        while self.reading:
            try:
                line = self.process.stdout.readline()
                if not line and self.process.poll() is not None:
                    break
                self.output_queue.put(line)
            except Exception:
                break

    def send_command(self, cmd):
        """发送命令到 C 端"""
        if not self.process or self.process.poll() is not None:
            return "系统未启动"

        try:
            self.process.stdin.write(cmd + "\n")
            self.process.stdin.flush()
            return True
        except Exception as e:
            return f"命令发送失败: {e}"

    def get_output(self, timeout=0.5):
        """获取输出"""
        output = []
        try:
            while True:
                try:
                    line = self.output_queue.get(timeout=timeout)
                    output.append(line)
                except queue.Empty:
                    break
        except Exception:
            pass

        return "".join(output)

    def stop(self):
        """停止 C 端"""
        self.reading = False
        if self.process:
            try:
                self.send_command("exit")
                self.process.wait(timeout=2)
            except Exception:
                self.process.kill()


class CSystemClient:
    """简化的 C 端客户端"""

    def __init__(self):
        self.wrapper = CSystemWrapper()
        self.is_logged_in = False

    def start_system(self):
        """启动系统"""
        return self.wrapper.start()

    def execute(self, cmd):
        """执行命令并获取输出"""
        self.wrapper.send_command(cmd)
        # 小延迟让输出进来
        import time
        time.sleep(0.2)
        return self.wrapper.get_output()

    def login(self, username="admin", password="admin"):
        """登录系统"""
        # C 端 login 是交互式的，我们用默认值
        output = self.execute("login")
        self.is_logged_in = True
        return output

    def logout(self):
        """登出"""
        output = self.execute("logout")
        self.is_logged_in = False
        return output

    def create_file(self, name):
        """创建文件"""
        return self.execute(f"create {name}")

    def create_dir(self, name):
        """创建目录"""
        return self.execute(f"mkdir {name}")

    def delete_file(self, name):
        """删除文件"""
        return self.execute(f"delete {name}")

    def list_dir(self):
        """列出目录"""
        return self.execute("dir")

    def nlp(self, text):
        """自然语言处理"""
        return self.execute(f"nlp {text}")

    def suggest(self):
        """获取建议"""
        return self.execute("suggestions")

    def analyze(self):
        """分析"""
        return self.execute("analyze")

    def optimize(self):
        """优化"""
        return self.execute("optimize")

    def get_block_status(self):
        """获取块使用状态，返回一个512长度的布尔列表"""
        output = self.execute("blocks")
        for line in output.split('\n'):
            if line.startswith("BLOCK_STATUS:"):
                status_str = line[len("BLOCK_STATUS:"):].strip()
                blocks = []
                for i in range(min(len(status_str), 512)):
                    blocks.append(status_str[i] == '1')
                # 补齐到512
                while len(blocks) < 512:
                    blocks.append(False)
                return blocks
        return [False] * 512

    def stop(self):
        """停止"""
        self.wrapper.stop()
