
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
小清新科技风格样式配置
"""

class Colors:
    # 主背景 - 清爽浅色
    BG = "#eef7f8"
    BG_LIGHT = "#ffffff"
    BG_LIGHTER = "#dff1f2"
    PANEL = "#fbfefe"

    # 主色调 - 湖蓝 + 薄荷绿
    PRIMARY = "#168aad"
    PRIMARY_LIGHT = "#46b5cf"
    PRIMARY_DARK = "#0f6d86"

    ACCENT = "#2fbf9f"
    ACCENT_LIGHT = "#78dcca"

    # 状态色
    SUCCESS = "#2fbf71"
    WARNING = "#d9932d"
    DANGER = "#dc5b5b"
    INFO = "#3a86c8"

    # 文本色
    FG = "#24323a"
    FG_MUTED = "#58717a"
    FG_DIM = "#8aa0a8"

    # 磁盘块颜色
    FREE = "#cceee7"
    USED = "#3aa7c9"
    SYSTEM = "#87c7ff"

    # 边框和分割线
    BORDER = "#bdd9df"
    BORDER_LIGHT = "#d8eaee"

    SHADOW = "#d6e8eb"


class Fonts:
    TITLE = ("Microsoft YaHei UI", 18)
    SUBTITLE = ("Microsoft YaHei UI", 12)
    HEADER = ("Microsoft YaHei UI", 11)
    PANEL_TITLE = ("Microsoft YaHei UI Light", 10)
    NORMAL = ("Microsoft YaHei UI", 10)
    SMALL = ("Microsoft YaHei UI", 9)
    BODY = ("Microsoft YaHei UI Light", 10)
    BODY_SMALL = ("Microsoft YaHei UI Light", 9)
    TEXT = ("Microsoft YaHei UI Light", 10)
    CODE = ("Consolas", 9, "normal")
    MONO = ("Consolas", 10)


class Layout:
    WINDOW_WIDTH = 1460
    WINDOW_HEIGHT = 920
    MIN_WIDTH = 1180
    MIN_HEIGHT = 720
    PADDING = 18
    PADDING_SMALL = 10
    BORDER_RADIUS = 6


class Filesystem:
    TOTAL_BLOCKS = 512
    BLOCK_SIZE = 512
    TOTAL_BYTES = TOTAL_BLOCKS * BLOCK_SIZE


class Animation:
    DURATION_FAST = 200
    DURATION_NORMAL = 400
    DURATION_SLOW = 600
