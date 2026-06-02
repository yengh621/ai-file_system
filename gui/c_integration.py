#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Bridge between the Tkinter GUI and the C filesystem executable."""

import os
import queue
import subprocess
import threading
import time
import platform


# ========== 跨平台自动配置 ==========
OS_NAME = platform.system()
# 可执行文件名
if OS_NAME == "Windows":
    BIN_NAME = "filesystem.exe"
    BUILD_CMD = ["powershell", "-ExecutionPolicy", "Bypass", "-File", "build.ps1"]
else:
    # Linux / WSL / macOS
    BIN_NAME = "filesystem"
    BUILD_CMD = ["make"]


class CSystemWrapper:
    """Owns the filesystem process and streams its output."""

    def __init__(self):
        self.process = None
        self.output_queue = queue.Queue()
        self.reading = False

    def start(self):
        """Start filesystem, building it first only when missing."""
        try:
            # 自动判断是否存在可执行文件
            if not os.path.exists(BIN_NAME):
                subprocess.run(BUILD_CMD, shell=False)

            # 跨平台启动命令
            if OS_NAME == "Windows":
                exec_cmd = [BIN_NAME]
            else:
                exec_cmd = [f"./{BIN_NAME}"]

            self.process = subprocess.Popen(
                exec_cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
            )

            self.reading = True
            reader = threading.Thread(target=self._read_output, daemon=True)
            reader.start()

            self.get_output_until_prompt(timeout=0.5)
            return True
        except Exception as exc:
            print(f"Failed to start C backend: {exc}")
            return False

    def _read_output(self):
        """Block on stdout and push decoded chunks into the queue."""
        while self.reading:
            try:
                chunk = os.read(self.process.stdout.fileno(), 4096)
                if not chunk and self.process.poll() is not None:
                    break
                if chunk:
                    self.output_queue.put(self._decode_output(chunk))
            except Exception:
                break

    def send_command(self, cmd):
        """Send one command line to the backend."""
        if not self.process or self.process.poll() is not None:
            return "System is not running."

        try:
            self.process.stdin.write((cmd + "\n").encode("utf-8"))
            self.process.stdin.flush()
            return True
        except Exception as exc:
            return f"Failed to send command: {exc}"

    def _decode_output(self, chunk):
        """Decode mixed UTF-8/GB18030 output from the backend."""
        try:
            return chunk.decode("utf-8")
        except UnicodeDecodeError:
            return chunk.decode("gb18030", errors="replace")

    def get_output(self, timeout=0.5):
        """Drain currently available output until a queue timeout."""
        output = []
        while True:
            try:
                output.append(self.output_queue.get(timeout=timeout))
            except queue.Empty:
                break
        return "".join(output)

    def get_output_until_text(self, text, timeout=1.0):
        """Read output until a specific prompt text appears."""
        output = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            wait = max(0.01, min(0.05, deadline - time.monotonic()))
            try:
                chunk = self.output_queue.get(timeout=wait)
                output.append(chunk)
                if text in "".join(output):
                    break
            except queue.Empty:
                continue
        return "".join(output)

    def get_output_until_prompt(self, timeout=1.0):
        """Read output until the backend prints its next '$ ' prompt."""
        output = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            wait = max(0.01, min(0.05, deadline - time.monotonic()))
            try:
                chunk = self.output_queue.get(timeout=wait)
                output.append(chunk)
                joined = "".join(output)
                if joined.endswith("$ ") or joined.rstrip().endswith("$"):
                    break
            except queue.Empty:
                continue
        return "".join(output)

    def stop(self):
        """Stop the backend process."""
        self.reading = False
        if self.process:
            try:
                self.send_command("exit")
                self.process.wait(timeout=2)
            except Exception:
                self.process.kill()


class CSystemClient:
    """Small command-oriented client used by the GUI."""

    BLOCK_COUNT = 512
    INODE_SIZE = 32
    INODE_COUNT = 32 * (512 // 32)

    def __init__(self):
        self.wrapper = CSystemWrapper()
        self.is_logged_in = False
        self.last_storage_status = {
            "blocks": [False] * self.BLOCK_COUNT,
            "inodes": [False] * self.INODE_COUNT,
        }

    def start_system(self):
        """Start the backend process."""
        return self.wrapper.start()

    def execute(self, cmd):
        """Execute an existing backend command and return its output."""
        self.wrapper.send_command(cmd)
        return self.wrapper.get_output_until_prompt(timeout=1.0)

    def login(self, username, password):
        """Drive the interactive login flow without fixed delays."""
        self.wrapper.send_command("login")
        output = self.wrapper.get_output_until_text("Username:", timeout=1.0)
        self.wrapper.send_command(username)
        output += self.wrapper.get_output_until_text("Password:", timeout=1.0)
        self.wrapper.send_command(password)
        output += self.wrapper.get_output_until_prompt(timeout=1.0)
        self.is_logged_in = "Login successful" in output
        return output

    def logout(self):
        """Log out from the backend."""
        output = self.execute("logout")
        self.is_logged_in = False
        return output

    def create_file(self, name):
        """Create a file."""
        return self.execute(f"create {name}")

    def create_dir(self, name):
        """Create a directory."""
        return self.execute(f"mkdir {name}")

    def delete_file(self, name):
        """Delete a file."""
        return self.execute(f"delete {name}")

    def list_dir(self):
        """List the current directory."""
        return self.execute("dir")

    def get_block_status(self):
        """Return a 512-entry boolean block usage list."""
        return self.get_storage_status()["blocks"]

    def get_storage_status(self):
        """Return block and inode usage parsed from the backend blocks command."""
        output = self.wrapper.send_command("blocks")
        if output is not True:
            return self.last_storage_status.copy()

        output = self.wrapper.get_output_until_prompt(timeout=2.0)
        blocks = self._parse_usage_bits(output, "BLOCK_STATUS:", self.BLOCK_COUNT)
        inodes = self._parse_usage_bits(output, "INODE_STATUS:", self.INODE_COUNT)

        if "BLOCK_STATUS:" not in output:
            blocks = self.last_storage_status["blocks"][:]
        if "INODE_STATUS:" not in output:
            inodes = self.last_storage_status["inodes"][:]

        self.last_storage_status = {
            "blocks": blocks[:],
            "inodes": inodes[:],
        }
        return {
            "blocks": blocks,
            "inodes": inodes,
        }

    def _parse_usage_bits(self, output, marker, expected_count):
        """Parse a fixed-width 0/1 status line from backend output."""
        if marker in output:
            raw_status = output.split(marker, 1)[1]
            status = "".join(ch for ch in raw_status if ch in "01")[:expected_count]
            bits = [ch == "1" for ch in status]
            return bits + [False] * (expected_count - len(bits))
        return [False] * expected_count

    def nlp(self, text):
        """Send natural-language input to the backend."""
        return self.execute(f"nlp {text}")

    def stop(self):
        """Stop the backend process."""
        self.wrapper.stop()
