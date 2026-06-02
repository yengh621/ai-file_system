#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Bridge between the Tkinter GUI and the C filesystem executable."""

import os
import queue
import subprocess
import threading
import time


class CSystemWrapper:
    """Owns the filesystem.exe process and streams its output."""

    def __init__(self):
        self.process = None
        self.output_queue = queue.Queue()
        self.reading = False

    def start(self):
        """Start filesystem.exe, building it first only when missing."""
        try:
            if not os.path.exists("filesystem.exe"):
                if os.name == "nt":
                    subprocess.run(
                        "powershell -ExecutionPolicy Bypass -File build.ps1",
                        shell=True,
                    )
                else:
                    subprocess.run("make", shell=True)

            self.process = subprocess.Popen(
                ["filesystem.exe"],
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

    def __init__(self):
        self.wrapper = CSystemWrapper()
        self.is_logged_in = False

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
        output = self.execute("blocks")
        marker = "BLOCK_STATUS:"
        if marker in output:
            raw_status = output.split(marker, 1)[1]
            status = "".join(ch for ch in raw_status if ch in "01")[:512]
            blocks = [ch == "1" for ch in status]
            return blocks + [False] * (512 - len(blocks))
        return [False] * 512

    def nlp(self, text):
        """Send natural-language input to the backend."""
        return self.execute(f"nlp {text}")

    def stop(self):
        """Stop the backend process."""
        self.wrapper.stop()
