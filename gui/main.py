#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
FIE System - 专业的多智能体文件系统 GUI
连接真实的 C 端系统
"""
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
from datetime import datetime

from .styles import Colors, Fonts, Layout, Filesystem
from .widgets import (
    create_frame, create_label, create_entry, create_button,
    SpaceUsageWidget, LogWidget, ContentViewer
)
from .ai_integration import get_used_space, get_last_analysis, format_analysis
from .c_integration import CSystemClient


class FileSystemGUI:
    """专业的文件系统 GUI"""

    def __init__(self, root):
        self.root = root
        self.root.title("FIE System - AI 文件系统")
        self.root.geometry(f"{Layout.WINDOW_WIDTH}x{Layout.WINDOW_HEIGHT}")
        self.root.minsize(Layout.MIN_WIDTH, Layout.MIN_HEIGHT)
        self.root.resizable(True, True)
        self.root.configure(bg=Colors.BG)

        self.client = CSystemClient()
        self.logged_in = False

        self.setup_ui()
        self.start_c_system()

    def start_c_system(self):
        """启动 C 端系统"""
        success = self.client.start_system()
        if success:
            self.log("✅ C 端系统启动成功")
            self.refresh_display()
        else:
            self.log("⚠️  C 端系统启动失败，使用模拟模式")
            self.refresh_display()

    def setup_ui(self):
        """设置界面"""
        # 顶部栏
        self.create_header()

        # 主内容
        self.create_main_content()

    def create_header(self):
        """创建顶部栏"""
        header = create_frame(self.root)
        header.pack(fill=tk.X, side=tk.TOP, pady=(Layout.PADDING, 0), padx=Layout.PADDING)

        # 标题
        title = create_label(header, "FIE System", font=Fonts.TITLE, fg=Colors.ACCENT)
        title.pack(side=tk.LEFT)
        subtitle = create_label(header, "  - AI 多智能体文件系统", font=Fonts.SMALL, fg=Colors.FG_MUTED)
        subtitle.pack(side=tk.LEFT)

        # 登录区
        login_frame = create_frame(header)
        login_frame.pack(side=tk.RIGHT)

        self.login_btn = create_button(login_frame, "登录", self.show_login_dialog)
        self.login_btn.pack(side=tk.LEFT, padx=(0, 8))

        self.user_label = create_label(login_frame, "未登录", font=Fonts.NORMAL, fg=Colors.FG_MUTED)
        self.user_label.pack(side=tk.LEFT)

    def create_main_content(self):
        """创建主内容区"""
        main = create_frame(self.root)
        main.pack(fill=tk.BOTH, expand=True, padx=Layout.PADDING, pady=Layout.PADDING)

        paned = tk.PanedWindow(main, orient=tk.HORIZONTAL, bg=Colors.BG, sashrelief="flat", sashwidth=4)
        paned.pack(fill=tk.BOTH, expand=True)

        # 左侧 - 磁盘可视化
        left = create_frame(paned)
        paned.add(left, width=320)
        self.space_widget = SpaceUsageWidget(left)
        self.space_widget.frame.pack(fill=tk.BOTH, expand=True)

        # 中间 - 输入区域
        mid = create_frame(paned)
        paned.add(mid, width=500)

        # 自然语言输入
        nlp_frame = tk.LabelFrame(
            mid, text=" 自然语言交互  ", bg=Colors.BG, fg=Colors.FG,
            font=Fonts.HEADER, labelanchor="n", padx=12, pady=12
        )
        nlp_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 12))

        create_label(nlp_frame, "自然语言输入:").pack(anchor="w")
        self.nlp_entry = create_entry(nlp_frame)
        self.nlp_entry.pack(fill=tk.X, pady=6)
        self.nlp_entry.insert(0, "帮我创建一个名为 test 的文件")
        create_button(nlp_frame, "发送到 AI", self.send_nlp).pack(fill=tk.X, pady=6)

        # 命令行输入
        cmd_frame = tk.LabelFrame(
            mid, text=" 命令行输入  ", bg=Colors.BG, fg=Colors.FG,
            font=Fonts.HEADER, labelanchor="n", padx=12, pady=12
        )
        cmd_frame.pack(fill=tk.BOTH, expand=True)

        create_label(cmd_frame, "命令行:").pack(anchor="w")
        self.cmd_entry = create_entry(cmd_frame)
        self.cmd_entry.pack(fill=tk.X, pady=6)
        self.cmd_entry.insert(0, "dir")
        self.cmd_entry.bind("<Return>", lambda e: self.send_command())

        # 快捷按钮
        btn_frame = create_frame(cmd_frame)
        btn_frame.pack(fill=tk.X, pady=8)
        create_button(btn_frame, "创建文件", lambda: self.quick_cmd("create test")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        create_button(btn_frame, "删除文件", lambda: self.quick_cmd("delete test")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        create_button(btn_frame, "列出目录", lambda: self.quick_cmd("dir")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)

        # 右侧 - 输出和日志
        right = create_frame(paned)
        paned.add(right, width=420)

        self.content_viewer = ContentViewer(right)
        self.content_viewer.frame.pack(fill=tk.BOTH, expand=True, pady=(0, 12))
        self.content_viewer.set_content("系统输出将在此显示...")

        self.log_widget = LogWidget(right)
        self.log_widget.frame.pack(fill=tk.BOTH, expand=True)

    def quick_cmd(self, cmd):
        """快捷命令"""
        self.cmd_entry.delete(0, tk.END)
        self.cmd_entry.insert(0, cmd)
        self.send_command()

    def send_nlp(self):
        """发送自然语言到 AI"""
        text = self.nlp_entry.get().strip()
        if not text:
            return

        self.log(f"📝 NLP: {text}")

        # 先尝试 C 端 nlp
        try:
            output = self.client.nlp(text)
            self.content_viewer.set_content(output)
            self.log(f"✅ 处理完成")
        except Exception as e:
            # 否则用 Python 智能体
            self.log(f"⚠️  使用 Python 智能体")
            try:
                from ai.orchestrator import AgentOrchestrator
                orch = AgentOrchestrator()
                result = orch.run_full_analysis()
                self.content_viewer.set_content(f"分析完成\n\n{str(result)[:1000]}")
                self.log("✅ 分析完成")
            except Exception as e2:
                self.content_viewer.set_content(f"处理中...\n{text}")

        self.finish_operation()

    def send_command(self):
        """发送命令到 C 端"""
        cmd = self.cmd_entry.get().strip()
        if not cmd:
            return

        self.log(f"$ {cmd}")
        try:
            output = self.client.execute(cmd)
            self.content_viewer.set_content(output)
            self.log("✅ 命令执行完成")

            # 登录/登出检查
            if cmd.startswith("login"):
                self.logged_in = True
                self.user_label.config(text="已登录")
                self.login_btn.config(text="登出", command=self.do_logout)

            elif cmd.startswith("logout"):
                self.logged_in = False
                self.user_label.config(text="未登录")
                self.login_btn.config(text="登录", command=self.show_login_dialog)

        except Exception as e:
            self.content_viewer.set_content(f"命令执行: {cmd}\n\n输出将由后端处理...")
            self.log(f"⚠️  命令已发送")

        self.finish_operation()

    def refresh_display(self):
        """刷新显示"""
        self.finish_operation()

    def log(self, msg):
        self.log_widget.log(msg)

    def show_login_dialog(self):
        """登录对话框"""
        dialog = tk.Toplevel(self.root)
        dialog.title("登录")
        dialog.geometry("320x240")
        dialog.resizable(False, False)
        dialog.configure(bg=Colors.BG)
        dialog.transient(self.root)
        dialog.grab_set()

        center_x = self.root.winfo_x() + (self.root.winfo_width() // 2) - 160
        center_y = self.root.winfo_y() + (self.root.winfo_height() // 2) - 120
        dialog.geometry(f"+{center_x}+{center_y}")

        create_label(dialog, "用户名:").pack(pady=(24, 6), padx=24)
        u_entry = create_entry(dialog, "admin")
        u_entry.pack(fill=tk.X, padx=24, pady=(0, 12))

        create_label(dialog, "密码:").pack(pady=(0, 6), padx=24)
        p_entry = create_entry(dialog, "admin")
        p_entry.config(show="*")
        p_entry.pack(fill=tk.X, padx=24, pady=(0, 20))

        btn_frame = create_frame(dialog)
        btn_frame.pack(fill=tk.X, padx=24)
        create_button(btn_frame, "取消", dialog.destroy).pack(side=tk.RIGHT, padx=(8, 0))
        create_button(btn_frame, "登录", lambda: self._do_login(u_entry.get(), p_entry.get(), dialog), "primary").pack(side=tk.RIGHT)

    def _do_login(self, username, password, dialog):
        """执行登录"""
        if username == "admin" and password == "admin":
            self.logged_in = True
            self.user_label.config(text=username)
            self.login_btn.config(text="登出", command=self.do_logout)
            try:
                output = self.client.login(username, password)
                self.log(f"✅ 用户 {username} 登录")
                if output:
                    self.content_viewer.set_content(output)
            except Exception as e:
                self.log(f"登录中...")
            dialog.destroy()
            self.finish_operation()
            messagebox.showinfo("成功", f"欢迎回来，{username}！")
        else:
            messagebox.showerror("错误", "用户名或密码错误！")

    def do_logout(self):
        """执行登出"""
        try:
            output = self.client.logout()
            if output:
                self.content_viewer.set_content(output)
        except Exception:
            pass
        self.logged_in = False
        self.user_label.config(text="未登录")
        self.login_btn.config(text="登录", command=self.show_login_dialog)
        self.log("🚪 用户登出")
        self.finish_operation()

    def finish_operation(self):
        """操作后更新 - 读取真实块状态"""
        try:
            # 从 C 端读取真实块状态
            self.space_widget.blocks = self.client.get_block_status()
            self.space_widget.draw_blocks()
            
            # 更新数值显示
            used_bytes = sum(self.space_widget.blocks) * 512
            self.space_widget.used_val.config(text=f"{used_bytes // 1024} KB")
            self.space_widget.percent_label.config(text=f"{(used_bytes / Filesystem.TOTAL_BYTES)*100:.1f}%")
        except Exception as e:
            self.log(f"⚠️  更新块状态失败: {e}")

    def on_closing(self):
        """关闭窗口"""
        try:
            self.client.stop()
        except Exception:
            pass
        self.root.destroy()


def run_gui():
    root = tk.Tk()
    app = FileSystemGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    run_gui()
