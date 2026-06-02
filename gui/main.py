#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
FIE System - 科技风格多智能体文件系统 GUI
"""
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, simpledialog
from datetime import datetime
import json
import os

from .styles import Colors, Fonts, Layout, Filesystem
from .widgets import (
    create_frame, create_label, create_entry, create_button,
    SpaceUsageWidget, LogWidget, ContentViewer, AgentPanel, CallLogViewer, FileTreeWidget
)
from .c_integration import CSystemClient
from .ai_integration import ai_integration


class FileSystemGUI:
    """科技风格文件系统 GUI"""

    def __init__(self, root):
        self.root = root
        self.root.title("FIE System - AI 多智能体文件系统")
        self.root.geometry(f"{Layout.WINDOW_WIDTH}x{Layout.WINDOW_HEIGHT}")
        self.root.minsize(Layout.MIN_WIDTH, Layout.MIN_HEIGHT)
        self.root.resizable(True, True)
        self.root.configure(bg=Colors.BG)

        self.client = CSystemClient()
        self.logged_in = False
        self.current_uid = -1
        self.orchestrator = None
        self.auto_analysis_timer = None

        self.setup_ui()
        self.start_c_system()

    def setup_ui(self):
        """设置界面"""
        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(1, weight=2)
        self.root.columnconfigure(2, weight=1)
        self.root.rowconfigure(1, weight=1)

        # 顶部状态栏
        top_frame = create_frame(self.root)
        top_frame.grid(row=0, column=0, columnspan=3, sticky="ew", padx=10, pady=5)

        self.user_label = create_label(top_frame, "未登录", font=Fonts.SUBTITLE, fg=Colors.FG_MUTED)
        self.user_label.pack(side=tk.LEFT)

        self.login_btn = create_button(top_frame, "登录", self.show_login_dialog, style="secondary")
        self.login_btn.pack(side=tk.RIGHT)

        # 左侧面板 - 磁盘状态
        left_frame = create_frame(self.root)
        left_frame.grid(row=1, column=0, sticky="nsew", padx=(10, 5), pady=5)

        self.space_widget = SpaceUsageWidget(left_frame)
        self.space_widget.frame.pack(fill=tk.BOTH, expand=True)

        # 中间面板 - 命令交互和文件树
        mid_frame = create_frame(self.root)
        mid_frame.grid(row=1, column=1, sticky="nsew", padx=(5, 5), pady=5)

        # 自然语言交互
        nlp_frame = create_frame(mid_frame)
        nlp_frame.pack(fill=tk.X, pady=(0, 10))

        create_label(nlp_frame, "💬 自然语言交互", font=Fonts.SUBTITLE, fg=Colors.PRIMARY).pack(anchor="w")
        self.nlp_frame_inner = create_frame(nlp_frame)
        self.nlp_frame_inner.pack(fill=tk.X, pady=(5, 0))

        self.nlp_entry = create_entry(self.nlp_frame_inner, "帮我创建一个文件 test.txt")
        self.nlp_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        self.nlp_entry.bind("<Return>", lambda e: self.send_nlp())

        create_button(self.nlp_frame_inner, "发送到 AI", self.send_nlp, style="accent").pack(side=tk.RIGHT)

        # 命令行交互
        cmd_frame = create_frame(mid_frame)
        cmd_frame.pack(fill=tk.BOTH, expand=True)

        create_label(cmd_frame, "⌨️ 命令行交互", font=Fonts.SUBTITLE, fg=Colors.PRIMARY).pack(anchor="w")

        self.cmd_entry = create_entry(cmd_frame, "dir")
        self.cmd_entry.pack(fill=tk.X, pady=(0, 8))
        self.cmd_entry.bind("<Return>", lambda e: self.send_command())

        btn_frame2 = create_frame(cmd_frame)
        btn_frame2.pack(fill=tk.X, pady=(0, 8))

        create_button(btn_frame2, "🔬 智能分析", self.run_analysis, style="accent").pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)

        # 文件树可视化区域
        self.file_tree_widget = FileTreeWidget(
            cmd_frame,
            on_context_menu=self.show_file_tree_menu,
            on_file_double_click=self.show_file_editor,
        )
        self.file_tree_widget.frame.pack(fill=tk.BOTH, expand=True, pady=(8, 0))

        # 右侧面板 - 智能体参数、调用记录、系统日志
        right_frame = create_frame(self.root)
        right_frame.grid(row=1, column=2, sticky="nsew", padx=(5, 10), pady=5)

        self.agent_panel = AgentPanel(right_frame)
        self.agent_panel.frame.pack(fill=tk.X, pady=(0, Layout.PADDING_SMALL))

        self.call_logger = CallLogViewer(right_frame)
        self.call_logger.frame.pack(fill=tk.X, pady=(0, Layout.PADDING_SMALL))

        self.content_viewer = ContentViewer(right_frame)
        self.content_viewer.frame.pack(fill=tk.BOTH, expand=True, pady=(0, Layout.PADDING_SMALL))
        self.content_viewer.set_content("系统输出将在此显示...")

        self.log_widget = LogWidget(right_frame)
        self.log_widget.frame.pack(fill=tk.BOTH, expand=True)

    def start_c_system(self):
        """启动 C 端系统"""
        success = self.client.start_system()
        if success:
            self.log("✅ C 端系统启动成功")
            self.refresh_display()
        else:
            self.log("⚠️ C 端系统启动失败，使用模拟模式")
            self.refresh_display()

    def quick_cmd(self, cmd):
        """快捷命令"""
        self.cmd_entry.delete(0, tk.END)
        self.cmd_entry.insert(0, cmd)
        self.send_command()

    def show_file_tree_menu(self, event, meta):
        """Show context actions for the directory visualization."""
        menu = tk.Menu(self.root, tearoff=0)
        name = meta.get("name", "")
        item_type = meta.get("type", "dir")

        if item_type == "file":
            menu.add_command(label="共享到用户", command=lambda: self.share_file_to_user(name))
            menu.add_command(label="删除文件", command=lambda: self.delete_file_from_tree(name))
        else:
            menu.add_command(label="创建文件", command=self.create_file_from_tree)

        menu.tk_popup(event.x_root, event.y_root)

    def create_file_from_tree(self):
        """Create a file from the directory tree context menu."""
        name = simpledialog.askstring("创建文件", "请输入文件名：", parent=self.root)
        if not name:
            return

        name = name.strip()
        self.execute_gui_command(f"create {name}", update_entry=True)
        self.refresh_directory_tree()

    def delete_file_from_tree(self, name):
        """Delete a selected file from the directory tree."""
        if not name:
            return
        if not messagebox.askyesno("删除文件", f"确定删除文件 {name} 吗？"):
            return

        self.execute_gui_command(f"delete {name}", update_entry=True)
        self.refresh_directory_tree()

    def share_file_to_user(self, name):
        """Share a file by opening group/other permissions."""
        if not name:
            return

        username = simpledialog.askstring("共享文件", "请输入要共享到的用户名：", parent=self.root)
        if not username:
            return

        username = username.strip()
        output = self.execute_gui_command(f"chmod {name} 666", update_entry=True)
        self.content_viewer.set_content(f"{output}\nShared {name} to {username}.")

    def show_file_editor(self, name):
        """Open a modal editor for file content."""
        if not name:
            return

        content = self.read_file_with_existing_commands(name)
        dialog = tk.Toplevel(self.root)
        dialog.title(f"编辑文件 - {name}")
        dialog.geometry("560x420")
        dialog.configure(bg=Colors.BG)
        dialog.transient(self.root)
        dialog.grab_set()

        editor = scrolledtext.ScrolledText(
            dialog,
            bg=Colors.BG_LIGHT,
            fg=Colors.FG,
            insertbackground=Colors.PRIMARY,
            font=Fonts.MONO,
            relief="flat",
            wrap="word"
        )
        editor.pack(fill=tk.BOTH, expand=True, padx=16, pady=(16, 8))
        editor.insert("1.0", content)

        btn_frame = create_frame(dialog)
        btn_frame.pack(fill=tk.X, padx=16, pady=(0, 16))

        def save_content():
            if not self.save_file_with_existing_commands(name, editor.get("1.0", "end-1c")):
                return
            dialog.destroy()
            self.refresh_directory_tree()

        create_button(btn_frame, "取消", dialog.destroy, style="secondary").pack(side=tk.RIGHT, padx=(8, 0))
        create_button(btn_frame, "保存", save_content, style="accent").pack(side=tk.RIGHT)

    def refresh_directory_tree(self):
        """Refresh the visual directory tree."""
        self.execute_gui_command("dir", update_entry=False)

    def _extract_fd(self, output):
        marker = "fd ="
        if marker not in output:
            return None
        tail = output.split(marker, 1)[1].strip()
        fd_text = tail.split()[0] if tail else ""
        return fd_text if fd_text.isdigit() else None

    def _extract_content(self, output):
        marker = "Content:"
        if marker not in output:
            return ""
        return output.split(marker, 1)[1].split("\n$ ", 1)[0].strip()

    def read_file_with_existing_commands(self, name):
        open_output = self.execute_gui_command(f"open {name} r", update_entry=True)
        fd = self._extract_fd(open_output)
        if fd is None:
            return ""

        read_output = self.execute_gui_command(f"read {fd} 4096", update_entry=True)
        self.execute_gui_command(f"close {fd}", update_entry=True)
        return self._extract_content(read_output)

    def save_file_with_existing_commands(self, name, content):
        normalized = content.strip()
        if any(ch.isspace() for ch in normalized):
            messagebox.showwarning(
                "保存受限",
                "当前后端 write 命令只支持不含空格和换行的一段内容，请去掉空白字符后再保存。"
            )
            return False

        self.execute_gui_command(f"delete {name}", update_entry=True)
        self.execute_gui_command(f"create {name}", update_entry=True)
        if normalized:
            open_output = self.execute_gui_command(f"open {name} w", update_entry=True)
            fd = self._extract_fd(open_output)
            if fd is None:
                return False
            self.execute_gui_command(f"write {fd} {normalized}", update_entry=True)
            self.execute_gui_command(f"close {fd}", update_entry=True)
        return True

    def execute_gui_command(self, cmd, update_entry=False):
        """Execute an existing backend command and record it in the normal memory flow."""
        cmd = cmd.strip()
        if not cmd:
            return ""

        if update_entry:
            self.cmd_entry.delete(0, tk.END)
            self.cmd_entry.insert(0, cmd)

        output = ""
        self.log(f"$ {cmd}")
        try:
            output = self.client.execute(cmd)

            if cmd.strip() == "dir":
                self.file_tree_widget.update_tree(output)
                self.content_viewer.set_content("目录列表已更新")
            else:
                self.content_viewer.set_content(output)

            self.log("命令执行完成")

            if self.logged_in and not cmd.startswith("login") and not cmd.startswith("logout"):
                parts = cmd.split(' ')
                operation = parts[0] if parts else cmd
                path = parts[1] if len(parts) > 1 else None
                ai_integration.record_operation(operation, path)
                self.log("操作已记录")

        except Exception:
            self.content_viewer.set_content(f"命令执行: {cmd}\n\n输出将由后端处理...")
            self.log("命令已发送")

        self.finish_operation()
        return output

    def send_nlp(self):
        """发送自然语言到 AI"""
        if not self.logged_in:
            messagebox.showwarning("提示", "请先登录才能使用 AI 功能")
            return

        text = self.nlp_entry.get().strip()
        if not text:
            return

        self.log(f"📝 NLP: {text}")

        try:
            output = self.client.nlp(text)
            self.content_viewer.set_content(output)
            self.log(f"✅ 处理完成")
        except Exception as e:
            self.log(f"⚠️ 使用 Python 智能体")
            try:
                self.init_orchestrator()
                result = self.orchestrator.run_full_analysis()

                if "call_logs" in result:
                    self.call_logger.update_logs(result["call_logs"])
                if "learned_params" in result:
                    self.agent_panel.update_parameters(result["learned_params"])

                output_text = self.format_analysis_result(result)
                self.content_viewer.set_content(output_text)
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

            if cmd.strip() == "dir":
                self.file_tree_widget.update_tree(output)
                self.content_viewer.set_content("目录列表已更新")
            else:
                self.content_viewer.set_content(output)

            self.log("✅ 命令执行完成")

            if cmd.startswith("login"):
                self.logged_in = True
                self.user_label.config(text="已登录")
                self.login_btn.config(text="登出", command=self.do_logout)
                self.current_uid = 100
                ai_integration.set_user(self.current_uid)
                self.init_orchestrator()
                self.log("📝 AI 记忆系统已激活")
                self.log("🔄 登录后自动分析...")
                self.run_analysis()
                self.start_auto_analysis()

            elif cmd.startswith("logout"):
                self.logged_in = False
                self.user_label.config(text="未登录")
                self.login_btn.config(text="登录", command=self.show_login_dialog)
                self.current_uid = -1
                ai_integration.clear_user()

            if self.logged_in and not cmd.startswith("login") and not cmd.startswith("logout"):
                parts = cmd.split(' ')
                operation = parts[0] if parts else cmd
                path = parts[1] if len(parts) > 1 else None
                ai_integration.record_operation(operation, path)
                self.log(f"📚 操作已记录")

        except Exception as e:
            self.content_viewer.set_content(f"命令执行: {cmd}\n\n输出将由后端处理...")
            self.log(f"⚠️ 命令已发送")

        self.finish_operation()

    def run_analysis(self):
        """运行智能体分析"""
        if not self.logged_in:
            messagebox.showwarning("提示", "请先登录")
            return

        self.log("🔬 开始多智能体分析...")

        try:
            result = ai_integration.run_analysis()

            call_logs = ai_integration.get_agent_calls()
            if call_logs:
                self.call_logger.update_logs(call_logs)

            params = ai_integration.get_current_params()
            self.agent_panel.update_parameters(params)

            output_text = self.format_analysis_result(result)
            self.content_viewer.set_content(output_text)

            short_term = ai_integration.get_short_term_memory()
            long_term = ai_integration.get_long_term_memory()
            self.log(f"📊 短时记忆: {len(short_term)} 条 | 长时记忆: {len(long_term)} 条")
            self.log("✅ 分析完成")
        except Exception as e:
            self.log(f"❌ 分析失败: {e}")

    def init_orchestrator(self):
        """初始化编排器"""
        if self.orchestrator is None:
            try:
                import sys
                sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
                from ai.orchestrator import AgentOrchestrator
                self.orchestrator = AgentOrchestrator()
                if self.current_uid != -1:
                    self.orchestrator.set_user(self.current_uid)
                self.log("✅ 编排器初始化成功")
            except Exception as e:
                self.log(f"⚠️ 编排器初始化: {e}")

    def uid_for_username(self, username):
        if username == "root":
            return 0
        if username.startswith("usr") and username[3:].isdigit():
            return 99 + int(username[3:])
        return 100

    def format_analysis_result(self, result):
        """格式化分析结果"""
        output = "╔════════════════════════════════════════════════════════════╗\n"
        output += "║                    多智能体分析报告                         ║\n"
        output += "╚════════════════════════════════════════════════════════════╝\n\n"

        if "analyzer_result" in result:
            output += "【 行为分析 】\n"
            output += f"{result['analyzer_result'].get('behavior_pattern', 'N/A')}\n\n"

        if "io_result" in result:
            output += "【 IO 优化 】\n"
            output += f"建议: {result['io_result'].get('suggestion', 'N/A')}\n"
            output += f"参数: {result['io_result'].get('parameters', {})}\n\n"

        if "security_result" in result:
            output += "【 安全分析 】\n"
            output += f"建议: {result['security_result'].get('suggestion', 'N/A')}\n"
            output += f"参数: {result['security_result'].get('parameters', {})}\n\n"

        if "kfs_result" in result:
            output += "【 文件分类 】\n"
            output += f"建议: {result['kfs_result'].get('suggestion', 'N/A')}\n"
            output += f"参数: {result['kfs_result'].get('parameters', {})}\n\n"

        if "learned_params" in result:
            output += "【 学习参数 】\n"
            for key, value in result['learned_params'].items():
                output += f"  {key}: {value}\n"

        return output

    def refresh_display(self):
        """刷新显示"""
        self.finish_operation()

    def log(self, msg):
        self.log_widget.log(msg)

    def show_login_dialog(self):
        """登录对话框"""
        dialog = tk.Toplevel(self.root)
        dialog.title("系统登录")
        dialog.geometry("400x300")
        dialog.resizable(False, False)
        dialog.configure(bg=Colors.BG)
        dialog.transient(self.root)
        dialog.grab_set()

        center_x = self.root.winfo_x() + (self.root.winfo_width() // 2) - 200
        center_y = self.root.winfo_y() + (self.root.winfo_height() // 2) - 150
        dialog.geometry(f"+{center_x}+{center_y}")

        create_label(dialog, "🔐 用户登录", font=Fonts.TITLE, fg=Colors.PRIMARY).pack(pady=(30, 20))

        create_label(dialog, "用户名：", font=Fonts.NORMAL, fg=Colors.FG).pack(pady=(0, 4), padx=40, anchor="w")
        u_entry = create_entry(dialog, "usr1")
        u_entry.pack(fill=tk.X, padx=40, pady=(0, 16))

        create_label(dialog, "密码：", font=Fonts.NORMAL, fg=Colors.FG).pack(pady=(0, 4), padx=40, anchor="w")
        p_entry = create_entry(dialog, "123456")
        p_entry.config(show="*")
        p_entry.pack(fill=tk.X, padx=40, pady=(0, 24))

        btn_frame = create_frame(dialog)
        btn_frame.pack(fill=tk.X, padx=40)
        create_button(btn_frame, "取消", dialog.destroy, style="secondary").pack(side=tk.RIGHT, padx=(8, 0))
        create_button(btn_frame, "登录", lambda: self._do_login(u_entry.get(), p_entry.get(), dialog), style="accent").pack(side=tk.RIGHT)

    def _do_login(self, username, password, dialog):
        """执行登录 - 调用后端验证"""
        try:
            output = self.client.login(username, password)
            print(f"[DEBUG] 登录输出: {repr(output)}")

            if "Login successful" not in output and "login ok" not in output.lower():
                self.logged_in = False
                self.client.is_logged_in = False
                if output:
                    self.content_viewer.set_content(output)
                self.log(f"❌ 登录失败: {username}")
                messagebox.showerror("错误", "用户名或密码错误")
                return

            self.logged_in = True
            self.client.is_logged_in = True
            self.user_label.config(text=username)
            self.login_btn.config(text="登出", command=self.do_logout)
            self.log(f"✅ 用户 {username} 登录成功")
            if output:
                self.content_viewer.set_content(output)
            dialog.destroy()

            self.current_uid = self.uid_for_username(username)
            ai_integration.set_user(self.current_uid)
            self.init_orchestrator()
            if self.orchestrator:
                self.orchestrator.set_user(self.current_uid)

            self.log("🔄 登录后自动分析...")
            self.run_analysis()

            self.start_auto_analysis()

            self.finish_operation()
            messagebox.showinfo("成功", f"欢迎回来，{username}！")
        except Exception as e:
            messagebox.showerror("错误", f"登录异常: {str(e)}")
            self.log(f"❌ 登录异常: {e}")

    def do_logout(self):
        """执行登出"""
        self.stop_auto_analysis()
        self.current_uid = -1
        ai_integration.clear_user()

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

    def start_auto_analysis(self):
        """启动定时分析（每10分钟执行一次）"""
        self.stop_auto_analysis()

        interval = 10 * 60 * 1000

        def scheduled_analysis():
            if self.logged_in:
                self.log("⏰ 定时分析触发...")
                self.run_analysis()
            self.auto_analysis_timer = self.root.after(interval, scheduled_analysis)

        self.auto_analysis_timer = self.root.after(interval, scheduled_analysis)
        self.log(f"⏰ 定时分析已启动（每{interval//60000}分钟）")

    def stop_auto_analysis(self):
        """停止定时分析"""
        if self.auto_analysis_timer:
            self.root.after_cancel(self.auto_analysis_timer)
            self.auto_analysis_timer = None
            self.log("⏹️ 定时分析已停止")

    def finish_operation(self):
        """操作后更新"""
        try:
            self.space_widget.blocks = self.client.get_block_status()
            self.space_widget.draw_blocks()

            used_bytes = sum(self.space_widget.blocks) * 512
            self.space_widget.update(used_bytes, Filesystem.TOTAL_BYTES)
        except Exception:
            pass

        self.root.after(100, lambda: None)


def main():
    root = tk.Tk()
    app = FileSystemGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
