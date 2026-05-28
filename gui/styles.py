
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
专业美观的样式配置
"""

class Colors:
    BG = "#0f1419"
    BG_LIGHT = "#1a2332"
    BG_LIGHTER = "#24344d"
    FG = "#e6e6e6"
    FG_MUTED = "#94a3b8"
    ACCENT = "#3b82f6"
    SUCCESS = "#10b981"
    DANGER = "#ef4444"
    WARNING = "#f59e0b"
    FREE = "#10b981"    # 绿色 - 空闲
    USED = "#ef4444"    # 红色 - 占用
    BORDER = "#334155"


class Fonts:
    TITLE = ("Microsoft YaHei UI", 16, "bold")
    HEADER = ("Microsoft YaHei UI", 13, "bold")
    NORMAL = ("Microsoft YaHei UI", 10)
    SMALL = ("Microsoft YaHei UI", 9)
    CODE = ("Consolas", 9, "normal")


class Layout:
    WINDOW_WIDTH = 1280
    WINDOW_HEIGHT = 800
    MIN_WIDTH = 1024
    MIN_HEIGHT = 640
    PADDING = 16


class Filesystem:
    TOTAL_BLOCKS = 512
    BLOCK_SIZE = 512
    TOTAL_BYTES = TOTAL_BLOCKS * BLOCK_SIZE
