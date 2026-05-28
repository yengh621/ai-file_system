
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自定义专业组件
"""
import tkinter as tk
from tkinter import ttk
from .styles import Colors, Fonts


def create_frame(parent, bg=None):
    """创建标准 Frame"""
    return tk.Frame(parent, bg=bg or Colors.BG)


def create_label(parent, text, font=None, fg=None, bg=None, anchor=None):
    """创建标准 Label"""
    return tk.Label(
        parent,
        text=text,
        font=font or Fonts.NORMAL,
        fg=fg or Colors.FG,
        bg=bg or Colors.BG,
        anchor=anchor or "w"
    )


def create_entry(parent, placeholder=""):
    """创建标准输入框"""
    entry = tk.Entry(
        parent,
        bg=Colors.BG_LIGHT,
        fg=Colors.FG,
        insertbackground=Colors.FG,
        relief="flat",
        font=Fonts.NORMAL
    )
    if placeholder:
        entry.insert(0, placeholder)
    return entry


def create_button(parent, text, command, style="primary"):
    """创建标准按钮"""
    btn = tk.Button(
        parent,
        text=text,
        command=command,
        bg=Colors.ACCENT if style == "primary" else Colors.BG_LIGHTER,
        fg="white" if style == "primary" else Colors.FG,
        font=Fonts.NORMAL,
        relief="flat",
        activebackground=Colors.BG_LIGHTER if style == "primary" else Colors.ACCENT,
        activeforeground="white",
        padx=16,
        pady=8,
        cursor="hand2"
    )
    return btn


class SpaceUsageWidget:
    """专业的磁盘块可视化组件"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 磁盘块  ",
            bg=Colors.BG,
            fg=Colors.FG,
            font=Fonts.HEADER,
            labelanchor="n",
            padx=12,
            pady=12
        )

        # 上部分：数值
        self.top_frame = create_frame(self.frame)
        self.top_frame.pack(fill=tk.X, pady=(0, 12))

        self.used_label = create_label(self.top_frame, "已使用", font=Fonts.SMALL, fg=Colors.FG_MUTED)
        self.used_label.pack(side=tk.LEFT, padx=(0, 8))

        self.used_val = create_label(self.top_frame, "0 KB", font=Fonts.TITLE, fg=Colors.USED)
        self.used_val.pack(side=tk.LEFT)

        tk.Label(self.top_frame, text="/", bg=Colors.BG, fg=Colors.FG_MUTED, font=Fonts.HEADER).pack(side=tk.LEFT, padx=8)

        self.total_val = create_label(self.top_frame, "256 KB", font=Fonts.HEADER, fg=Colors.FG)
        self.total_val.pack(side=tk.LEFT)

        self.percent_label = create_label(self.top_frame, "0%", font=Fonts.HEADER, fg=Colors.ACCENT)
        self.percent_label.pack(side=tk.RIGHT)

        # 图例
        self.legend_frame = create_frame(self.frame)
        self.legend_frame.pack(fill=tk.X, pady=(0, 10))

        # 已用
        self.legend_used_dot = tk.Canvas(self.legend_frame, width=16, height=16, bg=Colors.BG, highlightthickness=0, bd=0)
        self.legend_used_dot.create_rectangle(2, 2, 14, 14, fill=Colors.USED, outline=Colors.USED)
        self.legend_used_dot.pack(side=tk.LEFT, padx=(0, 4))
        create_label(self.legend_frame, "已占用", font=Fonts.SMALL, fg=Colors.FG_MUTED).pack(side=tk.LEFT, padx=(0, 16))

        # 空闲
        self.legend_free_dot = tk.Canvas(self.legend_frame, width=16, height=16, bg=Colors.BG, highlightthickness=0, bd=0)
        self.legend_free_dot.create_rectangle(2, 2, 14, 14, fill=Colors.FREE, outline=Colors.FREE)
        self.legend_free_dot.pack(side=tk.LEFT, padx=(0, 4))
        create_label(self.legend_frame, "可用", font=Fonts.SMALL, fg=Colors.FG_MUTED).pack(side=tk.LEFT)

        # 磁盘块网格
        self.canvas_container = create_frame(self.frame)
        self.canvas_container.pack(fill=tk.BOTH, expand=True, pady=4)

        self.canvas = tk.Canvas(
            self.canvas_container,
            bg=Colors.BG_LIGHT,
            highlightthickness=0,
            bd=0
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)

        # 分配状态
        self.blocks = [False] * 512  # 512个块，False=空闲，初始全部为绿色

        # 绑定尺寸变化事件
        self.canvas.bind("<Configure>", lambda e: self.draw_blocks())

        # 绘制初始
        import time
        self.canvas.after(100, self.draw_blocks)

    def draw_blocks(self):
        """绘制所有磁盘块"""
        self.canvas.delete("all")
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()

        if w < 100:
            w = 280
        if h < 100:
            h = 400

        padding = 10
        usable_w = w - padding * 2
        usable_h = h - padding * 2

        cols = 16  # 每行16个块
        rows = 32  # 总共32行 (512/16)

        # 计算每个块的大小，取宽高最小值
        block_w = usable_w // cols - 1
        block_h = usable_h // rows - 1
        block_size = min(block_w, block_h)

        if block_size < 6:
            block_size = 6

        # 居中布局的起始坐标
        total_grid_w = cols * (block_size + 1)
        total_grid_h = rows * (block_size + 1)
        start_x = padding + (usable_w - total_grid_w) // 2
        start_y = padding + (usable_h - total_grid_h) // 2

        for i in range(512):
            row = i // cols
            col = i % cols
            x = start_x + col * (block_size + 1)
            y = start_y + row * (block_size + 1)

            color = Colors.USED if self.blocks[i] else Colors.FREE
            self.canvas.create_rectangle(
                x, y, x + block_size, y + block_size,
                fill=color,
                outline=Colors.BG,
                tags=f"block{i}"
            )

    def update(self, used_bytes, total_bytes):
        """更新显示"""
        used_blocks = used_bytes // 512
        free_blocks = (total_bytes - used_bytes) // 512
        used_percent = (used_bytes / total_bytes) * 100

        self.used_val.config(text=f"{used_bytes // 1024} KB")
        self.total_val.config(text=f"{total_bytes // 1024} KB")
        self.percent_label.config(text=f"{used_percent:.1f}%")

        # 更新块状态（模拟跳着分配）
        import random
        current_used = sum(self.blocks)

        if used_blocks > current_used:
            # 需要分配新块（跳着分配）
            free_indices = [i for i, used in enumerate(self.blocks) if not used]
            to_allocate = min(used_blocks - current_used, len(free_indices))

            # 跳着选择块（间隔随机）
            allocated = []
            i = 0
            step = 3
            while len(allocated) < to_allocate and i < len(free_indices):
                idx = free_indices[i]
                self.blocks[idx] = True
                allocated.append(idx)
                i += step + random.randint(0, 2)

        self.draw_blocks()


class LogWidget:
    """专业日志组件"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 操作日志  ",
            bg=Colors.BG,
            fg=Colors.FG,
            font=Fonts.HEADER,
            labelanchor="n",
            padx=12,
            pady=12
        )

        self.text = tk.Text(
            self.frame,
            height=12,
            bg="#0a0f14",
            fg="#cbd5e1",
            font=Fonts.CODE,
            bd=0,
            relief="flat",
            wrap="word"
        )
        self.text.pack(fill=tk.BOTH, expand=True)

        scrollbar = ttk.Scrollbar(self.text, orient="vertical", command=self.text.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=scrollbar.set)

    def log(self, msg):
        from datetime import datetime
        t = datetime.now().strftime("%H:%M:%S")
        self.text.insert(tk.END, f"[{t}] {msg}\n")
        self.text.see(tk.END)


class ContentViewer:
    """内容查看组件"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 文件内容  ",
            bg=Colors.BG,
            fg=Colors.FG,
            font=Fonts.HEADER,
            labelanchor="n",
            padx=12,
            pady=12
        )

        self.text = tk.Text(
            self.frame,
            height=16,
            bg=Colors.BG_LIGHT,
            fg=Colors.FG,
            font=Fonts.CODE,
            bd=0,
            relief="flat",
            wrap="word"
        )
        self.text.pack(fill=tk.BOTH, expand=True)

        scrollbar = ttk.Scrollbar(self.text, orient="vertical", command=self.text.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=scrollbar.set)

    def set_content(self, content):
        self.text.delete(1.0, tk.END)
        if content:
            self.text.insert(tk.END, content)
