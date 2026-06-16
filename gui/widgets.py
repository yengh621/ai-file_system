
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
科技风格自定义组件
"""
import tkinter as tk
from tkinter import ttk
import re
from .styles import Colors, Fonts, Layout


def create_frame(parent, bg=None):
    """创建标准 Frame"""
    if bg is None:
        try:
            bg = parent.cget("bg")
        except Exception:
            bg = Colors.BG
    return tk.Frame(parent, bg=bg)


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
    """创建科技风格输入框"""
    entry = tk.Entry(
        parent,
        bg=Colors.BG_LIGHT,
        fg=Colors.FG,
        insertbackground=Colors.PRIMARY,
        relief="solid",
        bd=1,
        font=Fonts.NORMAL,
        highlightthickness=1,
        highlightbackground=Colors.BORDER_LIGHT,
        highlightcolor=Colors.PRIMARY
    )
    if placeholder:
        entry.insert(0, placeholder)
    return entry


def create_button(parent, text, command, style="primary"):
    """创建科技风格按钮"""
    if style == "primary":
        bg_color = Colors.PRIMARY
        fg_color = "#ffffff"
        active_bg = Colors.PRIMARY_LIGHT
        active_fg = "#ffffff"
    elif style == "accent":
        bg_color = Colors.ACCENT
        fg_color = "#08342f"
        active_bg = Colors.ACCENT_LIGHT
        active_fg = "#08342f"
    else:
        bg_color = Colors.BG_LIGHT
        fg_color = Colors.FG
        active_bg = Colors.BG_LIGHTER
        active_fg = Colors.FG
    
    btn = tk.Button(
        parent,
        text=text,
        command=command,
        bg=bg_color,
        fg=fg_color,
        font=Fonts.NORMAL,
        relief="solid",
        bd=1,
        activebackground=active_bg,
        activeforeground=active_fg,
        padx=16,
        pady=7,
        cursor="hand2"
    )
    return btn


class GlowLabel(tk.Label):
    """发光效果标签"""
    def __init__(self, parent, text, color=Colors.PRIMARY, **kwargs):
        super().__init__(parent, text=text, fg=color, **kwargs)
        self.color = color


class SpaceUsageWidget:
    """科技风格磁盘块可视化组件"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 存储状态  ",
            bg=Colors.PANEL,
            fg=Colors.PRIMARY,
            font=Fonts.PANEL_TITLE,
            labelanchor="nw",
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=1,
            relief="solid"
        )

        # 上部分：数值
        self.top_frame = create_frame(self.frame)
        self.top_frame.pack(fill=tk.X, pady=(0, Layout.PADDING_SMALL))

        self.used_label = create_label(self.top_frame, "已使用", font=Fonts.SMALL, fg=Colors.FG_MUTED)
        self.used_label.pack(side=tk.LEFT, padx=(0, 8))

        self.used_val = create_label(self.top_frame, "0 KB", font=Fonts.TITLE, fg=Colors.PRIMARY)
        self.used_val.pack(side=tk.LEFT)

        tk.Label(self.top_frame, text="/", bg=Colors.BG, fg=Colors.FG_DIM, font=Fonts.HEADER).pack(side=tk.LEFT, padx=8)

        self.total_val = create_label(self.top_frame, "256 KB", font=Fonts.HEADER, fg=Colors.FG)
        self.total_val.pack(side=tk.LEFT)

        self.percent_label = create_label(self.top_frame, "0%", font=Fonts.TITLE, fg=Colors.ACCENT)
        self.percent_label.pack(side=tk.RIGHT)

        self.meta_frame = create_frame(self.frame)
        self.meta_frame.pack(fill=tk.X, pady=(0, 8))

        self.meta_label = create_label(
            self.meta_frame,
            "Metadata: 0 inodes / 0 B",
            font=Fonts.SMALL,
            fg=Colors.FG_MUTED,
        )
        self.meta_label.pack(side=tk.LEFT)

        self.operation_time_label = create_label(
            self.meta_frame,
            "上一步操作用时: --",
            font=Fonts.SMALL,
            fg=Colors.PRIMARY_DARK,
        )
        self.operation_time_label.pack(side=tk.RIGHT)

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
            bg=Colors.PANEL,
            highlightthickness=0,
            bd=0
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)

        # 分配状态
        self.blocks = [False] * 512

        # 绑定尺寸变化事件
        self.canvas.bind("<Configure>", lambda e: self.draw_blocks())

        # 绘制初始
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

        cols = 16
        rows = 32

        block_w = usable_w // cols - 1
        block_h = usable_h // rows - 1
        block_size = min(block_w, block_h)

        if block_size < 6:
            block_size = 6

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
                outline=Colors.PANEL,
                tags=f"block{i}"
            )

    def update(self, used_bytes, total_bytes, used_inodes=0, inode_bytes=0):
        """更新显示"""
        used_percent = (used_bytes / total_bytes) * 100

        self.used_val.config(text=self._format_size(used_bytes))
        self.total_val.config(text=self._format_size(total_bytes))
        self.percent_label.config(text=f"{used_percent:.2f}%")
        self.meta_label.config(text=f"Metadata: {used_inodes} inodes / {inode_bytes} B")

        self.draw_blocks()

    def clear(self):
        """Clear disk usage details for logged-out sessions."""
        self.blocks = [False] * 512
        self.used_val.config(text="0 B")
        self.total_val.config(text="0 B")
        self.percent_label.config(text="0%")
        self.meta_label.config(text="Metadata: 0 inodes / 0 B")
        self.draw_blocks()

    def set_operation_time(self, elapsed_seconds):
        """Display the latest user-visible backend operation duration."""
        if elapsed_seconds < 0.001:
            value = f"{elapsed_seconds * 1_000_000:.0f} μs"
        elif elapsed_seconds < 1:
            value = f"{elapsed_seconds * 1000:.2f} ms"
        else:
            value = f"{elapsed_seconds:.3f} s"
        self.operation_time_label.config(text=f"上一步操作用时: {value}")

    def _format_size(self, size_bytes):
        """Format byte sizes without hiding small changes."""
        if size_bytes < 1024:
            return f"{size_bytes} B"
        if size_bytes < 1024 * 1024:
            return f"{size_bytes / 1024:.1f} KB"
        return f"{size_bytes / (1024 * 1024):.2f} MB"


class LogWidget:
    """科技风格日志组件"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 系统日志  ",
            bg=Colors.PANEL,
            fg=Colors.PRIMARY,
            font=Fonts.PANEL_TITLE,
            labelanchor="nw",
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=1,
            relief="solid"
        )

        self.text = tk.Text(
            self.frame,
            height=8,
            bg=Colors.PANEL,
            fg=Colors.FG_MUTED,
            font=Fonts.TEXT,
            bd=0,
            relief="flat",
            wrap="word"
        )
        self.text.configure(selectbackground=Colors.BG_LIGHTER, selectforeground=Colors.FG)
        self.text.tag_configure("analysis_title", foreground=Colors.PRIMARY_DARK, font=Fonts.HEADER, spacing3=8)
        self.text.pack(fill=tk.BOTH, expand=True)

        scrollbar = tk.Scrollbar(self.text, orient="vertical", command=self.text.yview, bg=Colors.BG_LIGHTER, troughcolor=Colors.PANEL)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=scrollbar.set)

    def log(self, msg):
        from datetime import datetime
        t = datetime.now().strftime("%H:%M:%S")
        self.text.insert(tk.END, f"[{t}] {msg}\n")
        self.text.see(tk.END)


class ContentViewer:
    """科技风格内容查看组件"""

    def __init__(self, parent):
        self.frame = tk.Frame(
            parent,
            bg=Colors.PANEL,
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=0,
            relief="flat"
        )

        self.title_label = tk.Label(
            self.frame,
            text="智能体分析",
            bg=Colors.BG_LIGHTER,
            fg=Colors.PRIMARY_DARK,
            font=Fonts.PANEL_TITLE,
            padx=10,
            pady=3,
            anchor="w"
        )
        self.title_label.pack(anchor="w", pady=(0, 8))

        self.text = tk.Text(
            self.frame,
            height=12,
            bg=Colors.PANEL,
            fg=Colors.FG,
            font=Fonts.TEXT,
            bd=0,
            relief="flat",
            wrap="word"
        )
        self.text.configure(selectbackground=Colors.BG_LIGHTER, selectforeground=Colors.FG)
        self.text.pack(fill=tk.BOTH, expand=True)

        scrollbar = tk.Scrollbar(self.text, orient="vertical", command=self.text.yview, bg=Colors.BG_LIGHTER, troughcolor=Colors.PANEL)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=scrollbar.set)

    def set_content(self, content):
        self.text.delete(1.0, tk.END)
        if content:
            title = "多智能体分析报告\n"
            if content.startswith(title):
                self.text.insert(tk.END, title, "analysis_title")
                self.text.insert(tk.END, content[len(title):])
                return
            self.text.insert(tk.END, content)


class AgentPanel:
    """智能体参数显示面板"""
    
    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 智能体参数  ",
            bg=Colors.PANEL,
            fg=Colors.PRIMARY,
            font=Fonts.PANEL_TITLE,
            labelanchor="nw",
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=1,
            relief="solid"
        )
        
        # IO Agent
        self.io_frame = create_frame(self.frame)
        self.io_frame.pack(fill=tk.X, pady=(0, 6))
        
        create_label(self.io_frame, "🤖 IO Agent", font=Fonts.BODY, fg=Colors.PRIMARY).pack(anchor="w")
        self.io_param_label = create_label(self.io_frame, "预取窗口: 3", font=Fonts.BODY_SMALL, fg=Colors.FG_MUTED)
        self.io_param_label.pack(anchor="w", padx=10, pady=(2, 0))
        # Security Agent
        self.security_frame = create_frame(self.frame)
        self.security_frame.pack(fill=tk.X, pady=(6, 6))
        
        create_label(self.security_frame, "🛡️ Security Agent", font=Fonts.BODY, fg=Colors.PRIMARY).pack(anchor="w")
        self.security_delete_label = create_label(self.security_frame, "删除阈值: 5", font=Fonts.BODY_SMALL, fg=Colors.FG_MUTED)
        self.security_delete_label.pack(anchor="w", padx=10, pady=(2, 0))
        self.security_modify_label = create_label(self.security_frame, "修改阈值: 10", font=Fonts.BODY_SMALL, fg=Colors.FG_MUTED)
        self.security_modify_label.pack(anchor="w", padx=10)
        
        # KFS Agent
        self.kfs_frame = create_frame(self.frame)
        self.kfs_frame.pack(fill=tk.X, pady=(6, 0))
        
        create_label(self.kfs_frame, "📁 KFS Agent", font=Fonts.BODY, fg=Colors.PRIMARY).pack(anchor="w")
        self.kfs_hot_label = create_label(self.kfs_frame, "热点文件: []", font=Fonts.BODY_SMALL, fg=Colors.FG_MUTED)
        self.kfs_hot_label.pack(anchor="w", padx=10, pady=(2, 0))

    def update_parameters(self, params):
        """Update the panel with actual agent parameters."""
        params = self._normalize_parameters(params)
        if "prefetch_window" in params:
            self.io_param_label.config(text=f"预取窗口: {params['prefetch_window']}")
        elif "file_prefetch_windows" in params:
            windows = params["file_prefetch_windows"]
            if isinstance(windows, dict) and windows:
                avg_window = sum(windows.values()) // len(windows)
                self.io_param_label.config(text=f"预取窗口: {avg_window} (per-file)")
            else:
                self.io_param_label.config(text="预取窗口: 3 (default)")

        if "delete_threshold" in params:
            self.security_delete_label.config(text=f"删除阈值: {params['delete_threshold']}")
        if "modify_threshold" in params:
            self.security_modify_label.config(text=f"修改阈值: {params['modify_threshold']}")

        if "hot_files" in params:
            hot_preview = self._format_hot_files(params["hot_files"])
            self.kfs_hot_label.config(text=f"热点文件: {hot_preview}")
        else:
            self.kfs_hot_label.config(text="热点文件: []")

    def _normalize_parameters(self, data):
        """Accept both raw params and full analysis result dictionaries."""
        if not isinstance(data, dict):
            return {}
        if isinstance(data.get("parameters"), dict):
            return data["parameters"]
        learned_params = data.get("learned_params")
        if isinstance(learned_params, dict):
            return self._normalize_parameters(learned_params)

        params = {}
        for key in ("io_result", "security_result", "kfs_result"):
            result_params = data.get(key, {}).get("parameters", {})
            if isinstance(result_params, dict):
                params.update(result_params)
        return params or data

    def _format_hot_files(self, hot_files):
        if not hot_files:
            return "[]"

        preview = []
        for item in hot_files[:4]:
            if isinstance(item, dict):
                name = item.get("filename") or item.get("name") or item.get("path") or str(item)
                access_count = item.get("access_count")
                preview.append(f"{name}({access_count})" if access_count is not None else str(name))
            else:
                preview.append(str(item))
        suffix = "..." if len(hot_files) > 4 else ""
        return ", ".join(preview) + suffix


class FileTreeWidget:
    """科技风格文件树组件"""

    def __init__(self, parent, on_file_select=None, on_context_menu=None, on_file_double_click=None, on_dir_double_click=None):
        self.on_file_select = on_file_select
        self.on_context_menu = on_context_menu
        self.on_file_double_click = on_file_double_click
        self.on_dir_double_click = on_dir_double_click
        self.item_meta = {}
        self.current_path = "/"
        self.frame = tk.LabelFrame(
            parent,
            text=" 📁 目录结构  ",
            bg=Colors.PANEL,
            fg=Colors.PRIMARY,
            font=Fonts.PANEL_TITLE,
            labelanchor="nw",
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=1,
            relief="solid"
        )

        # 树状视图
        self.tree = ttk.Treeview(self.frame, show="tree", selectmode="browse")
        self.tree.pack(fill=tk.BOTH, expand=True)
        
        # 设置样式
        style = ttk.Style()
        style.configure("Treeview",
                       background=Colors.PANEL,
                       foreground=Colors.FG,
                       fieldbackground=Colors.PANEL,
                       rowheight=26,
                       borderwidth=0,
                       font=Fonts.NORMAL)
        style.map("Treeview",
                  background=[("selected", Colors.BG_LIGHTER)],
                  foreground=[("selected", Colors.PRIMARY_DARK)])
        style.configure("Treeview.Heading",
                       background=Colors.PANEL,
                       foreground=Colors.PRIMARY,
                       font=Fonts.HEADER)
        
        # 绑定点击事件
        self.tree.bind("<<TreeviewSelect>>", self._on_select)
        self.tree.bind("<Button-3>", self._on_right_click)
        self.tree.bind("<Double-1>", self._on_double_click)
        
        # 初始化根节点
        self.root_node = self.tree.insert("", "end", text="/", open=True)
        self.item_meta[self.root_node] = {"name": "/", "type": "dir", "path": "/"}
        
        # 添加滚动条
        scrollbar = tk.Scrollbar(self.tree, orient="vertical", command=self.tree.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree.configure(yscrollcommand=scrollbar.set)

    def _on_select(self, event):
        """处理文件选择"""
        if self.on_file_select:
            selection = self.tree.selection()
            if selection:
                meta = self.item_meta.get(selection[0], {})
                self.on_file_select(meta.get("name", self.tree.item(selection[0])["text"]))

    def _on_right_click(self, event):
        item_id = self.tree.identify_row(event.y)
        if item_id:
            self.tree.selection_set(item_id)
        else:
            item_id = self.root_node

        if self.on_context_menu:
            meta = self.item_meta.get(item_id, {"name": "/", "type": "dir"})
            self.on_context_menu(event, meta)

    def _on_double_click(self, event):
        item_id = self.tree.identify_row(event.y)
        if not item_id:
            return

        meta = self.item_meta.get(item_id, {})
        if meta.get("type") == "file" and self.on_file_double_click:
            self.on_file_double_click(meta)
        elif meta.get("type") == "dir" and self.on_dir_double_click:
            self.on_dir_double_click(meta)

    def update_tree(self, dir_output, current_path="/", allow_parent=False):
        """根据目录输出更新树"""
        print(f"[DEBUG] 目录输出: {repr(dir_output)}")
        self.current_path = current_path or "/"
        self.tree.item(self.root_node, text=self.current_path, open=True)
        self.item_meta = {self.root_node: {"name": self.current_path, "type": "dir", "path": self.current_path}}
        
        # 清空现有树
        for child in self.tree.get_children(self.root_node):
            self.tree.delete(child)

        if allow_parent and self.current_path != "/":
            parent_path = self.current_path.rsplit("/", 1)[0] or "/"
            item_id = self.tree.insert(self.root_node, "end", text="..")
            self.item_meta[item_id] = {"name": "..", "type": "dir", "path": parent_path, "is_parent": True}
        
        # 解析目录输出
        lines = dir_output.strip().split('\n')
        for line in lines:
            line = line.strip()
            if not line or line.startswith("Directory contents") or line.startswith("$"):
                continue
            
            # 只解析符合格式的行：以 d/l/- 开头，后面跟着名称和 (ino: ...)
            if not (line.startswith('d ') or line.startswith('- ') or line.startswith('l ')):
                continue
            
            # 解析格式: "d usr (ino: 2)" 或 "- test.txt (ino: 3)" 或 "d . (ino: 65520, links: 2)"
            if ' ' in line:
                parts = line.split(' ', 1)
                if len(parts) >= 2:
                    type_char = parts[0]
                    rest = parts[1]
                    
                    # 跳过 . 和 ..
                    if rest.startswith('. '):
                        continue
                    
                    # 提取文件名（去掉 ino 信息）
                    if '(' in rest:
                        name = rest[:rest.index('(')].strip()
                    else:
                        name = rest.strip()
                    name = name.replace("[KFS]", "").strip()
                    ino_match = re.search(r"\bino:\s*(\d+)", rest)
                    uid_match = re.search(r"\buid:\s*(\d+)", rest)
                    
                    # 跳过空名称和 . ..
                    if not name or name == '.' or name == '..':
                        continue
                    
                    # 设置图标
                    if type_char == 'd':
                        display_name = f"📂 {name}"
                        item_type = "dir"
                    elif type_char == 'l':
                        # 符号链接
                        if ' -> ' in rest:
                            link_target = rest[rest.index('->')+3:].strip()
                            if '(' in link_target:
                                link_target = link_target[:link_target.index('(')].strip()
                            display_name = f"🔗 {name} -> {link_target}"
                        else:
                            display_name = f"🔗 {name}"
                        item_type = "file"
                    else:
                        # 普通文件
                        display_name = f"📄 {name}"
                        item_type = "file"
                    
                    # 添加到树
                    item_id = self.tree.insert(self.root_node, "end", text=display_name)
                    item_path = self.current_path.rstrip("/") + "/" + name if self.current_path != "/" else "/" + name
                    meta = {"name": name, "type": item_type, "path": item_path}
                    if ino_match and "[KFS]" in rest:
                        meta["kfs_ino"] = int(ino_match.group(1))
                    if uid_match and "[KFS-USER]" in rest:
                        meta["kfs_uid"] = int(uid_match.group(1))
                    self.item_meta[item_id] = meta

    def clear(self):
        """清空树"""
        self.current_path = "/"
        self.tree.item(self.root_node, text="/", open=True)
        self.item_meta = {self.root_node: {"name": "/", "type": "dir", "path": "/"}}
        for child in self.tree.get_children(self.root_node):
            self.tree.delete(child)


class CallLogViewer:
    """调用记录查看器"""

    def __init__(self, parent):
        self.frame = tk.LabelFrame(
            parent,
            text=" 调用记录  ",
            bg=Colors.PANEL,
            fg=Colors.ACCENT,
            font=Fonts.PANEL_TITLE,
            labelanchor="nw",
            padx=Layout.PADDING_SMALL,
            pady=Layout.PADDING_SMALL,
            bd=1,
            relief="solid"
        )
        
        self.text = tk.Text(
            self.frame,
            height=7,
            bg=Colors.PANEL,
            fg=Colors.FG_MUTED,
            font=Fonts.BODY_SMALL,
            bd=0,
            relief="flat",
            wrap="word"
        )
        self.text.configure(selectbackground=Colors.BG_LIGHTER, selectforeground=Colors.FG)
        self.text.pack(fill=tk.BOTH, expand=True)
        
        scrollbar = tk.Scrollbar(self.text, orient="vertical", command=self.text.yview, bg=Colors.BG_LIGHTER, troughcolor=Colors.PANEL)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=scrollbar.set)
    
    def update_logs(self, logs):
        """更新调用记录"""
        self.text.delete(1.0, tk.END)
        for log in reversed(logs):
            timestamp = log.get("timestamp", "")
            agent = log.get("agent", "")
            params = log.get("parameters", {})
            self.text.insert(tk.END, f"[{timestamp}] {agent}\n")
            self.text.insert(tk.END, f"  参数: {params}\n\n")
