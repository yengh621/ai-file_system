
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
科技风格样式配置
"""

class Colors:
    # 主背景 - 深空黑
    BG = "#0a0e17"
    BG_LIGHT = "#111827"
    BG_LIGHTER = "#1f2937"
    
    # 主色调 - 赛博蓝
    PRIMARY = "#06b6d4"
    PRIMARY_LIGHT = "#22d3ee"
    PRIMARY_DARK = "#0891b2"
    
    # 强调色 - 霓虹紫
    ACCENT = "#a855f7"
    ACCENT_LIGHT = "#c084fc"
    
    # 状态色
    SUCCESS = "#10b981"
    WARNING = "#f59e0b"
    DANGER = "#ef4444"
    INFO = "#3b82f6"
    
    # 文本色
    FG = "#f9fafb"
    FG_MUTED = "#9ca3af"
    FG_DIM = "#6b7280"
    
    # 磁盘块颜色
    FREE = "#10b981"    # 绿色 - 空闲
    USED = "#3b82f6"    # 蓝色 - 占用
    SYSTEM = "#a855f7"  # 紫色 - 系统
    
    # 边框和分割线
    BORDER = "#374151"
    BORDER_LIGHT = "#4b5563"
    
    # 渐变效果
    GRADIENT_START = "#06b6d4"
    GRADIENT_END = "#a855f7"


class Fonts:
    TITLE = ("Microsoft YaHei UI", 20, "bold")
    SUBTITLE = ("Microsoft YaHei UI", 14, "bold")
    HEADER = ("Microsoft YaHei UI", 12, "bold")
    NORMAL = ("Microsoft YaHei UI", 10)
    SMALL = ("Microsoft YaHei UI", 9)
    CODE = ("Consolas", 9, "normal")
    MONO = ("Consolas", 10)


class Layout:
    WINDOW_WIDTH = 1400
    WINDOW_HEIGHT = 900
    MIN_WIDTH = 1200
    MIN_HEIGHT = 700
    PADDING = 20
    PADDING_SMALL = 12
    BORDER_RADIUS = 8


class Filesystem:
    TOTAL_BLOCKS = 512
    BLOCK_SIZE = 512
    TOTAL_BYTES = TOTAL_BLOCKS * BLOCK_SIZE


class Animation:
    DURATION_FAST = 200
    DURATION_NORMAL = 400
    DURATION_SLOW = 600

