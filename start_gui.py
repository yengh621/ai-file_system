
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
启动 GUI 的简单入口
"""
import sys
import io

# 修复 Windows 终端编码
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from gui.main import run_gui

if __name__ == "__main__":
    print("[Starting] FIE System GUI...")
    run_gui()
