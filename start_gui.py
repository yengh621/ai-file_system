#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
启动 GUI 的简单入口（跨平台 Windows / Linux / WSL）
"""
import sys
import io
import platform

# ==================== 跨平台编码修复（解决中文乱码）====================
# Windows 终端编码
if platform.system() == "Windows":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

# Linux / WSL 图形界面中文渲染修复（关键！）
else:
    try:
        import tkinter as tk
        from tkinter import font
        root = tk.Tk()
        # 自动设置 Linux 中文字体，解决方框乱码
        default_font = font.Font(family="WenQuanYi Zen Hei", size=10)
        root.option_add("*Font", default_font)
        root.destroy()
    except Exception:
        pass

# ==================== 启动GUI ====================
from gui import run_gui

if __name__ == "__main__":
    print("[Starting] FIE System GUI...")
    run_gui()