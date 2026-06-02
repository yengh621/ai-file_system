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
import threading
import posixpath

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
        self.security_timer = None
        self.analysis_thread = None
        self.analysis_in_progress = False
        self.security_check_thread = None
        self.security_check_in_progress = False
        self.current_path = "/"
        self.home_path = "/"
        self.clipboard = None

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
            on_dir_double_click=self.open_directory_from_tree,
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

    def show_file_tree_menu(self, event, meta):
        """Show context actions for the directory visualization."""
        menu = tk.Menu(self.root, tearoff=0)
        item_type = meta.get("type", "dir")
        is_parent = meta.get("is_parent", False)

        if item_type == "file":
            menu.add_command(label="Copy File", command=lambda: self.copy_file_from_tree(meta))
            menu.add_command(label="Cut File", command=lambda: self.cut_file_from_tree(meta))
            menu.add_command(label="Share File", command=lambda: self.share_file_to_user(meta))
            if self.current_uid == 0:
                menu.add_command(label="Grant To User", command=lambda: self.grant_file_to_user(meta))
            menu.add_command(label="Delete File", command=lambda: self.delete_file_from_tree(meta))
        else:
            menu.add_command(label="New File", command=self.create_file_from_tree)
            menu.add_command(label="New Directory", command=self.create_directory_from_tree)
            if self.clipboard:
                menu.add_command(label="Paste", command=lambda: self.paste_into_directory(meta))
            if not is_parent and meta.get("path") not in {self.home_path, "/", "/usr"}:
                menu.add_command(label="Delete Directory", command=lambda: self.delete_directory_from_tree(meta))

        menu.tk_popup(event.x_root, event.y_root)

    def create_file_from_tree(self):
        """Create a file from the directory tree context menu."""
        name = simpledialog.askstring("Create File", "Enter file name:", parent=self.root)
        if not name:
            return

        name = name.strip()
        self.execute_gui_command(f"create {name}", update_entry=True)
        self.refresh_directory_tree()

    def create_directory_from_tree(self):
        """Create a directory from the directory tree context menu."""
        name = simpledialog.askstring("Create Directory", "Enter directory name:", parent=self.root)
        if not name:
            return

        name = name.strip()
        self.execute_gui_command(f"mkdir {name}", update_entry=True)
        self.refresh_directory_tree()

    def delete_file_from_tree(self, meta):
        """Delete a selected file from the directory tree."""
        name = meta.get("name", "") if isinstance(meta, dict) else str(meta)
        if not name:
            return
        if not messagebox.askyesno("Delete File", f"Delete file {name}?"):
            return

        self.execute_gui_command(f"delete {name}", update_entry=True)
        self.refresh_directory_tree()

    def delete_directory_from_tree(self, meta):
        """Delete a selected directory from the directory tree."""
        name = meta.get("name", "") if isinstance(meta, dict) else str(meta)
        if not name:
            return
        if not messagebox.askyesno("Delete Directory", f"Delete directory {name}?"):
            return

        self.execute_gui_command(f"rmdir {name}", update_entry=True)
        self.refresh_directory_tree()

    def copy_file_from_tree(self, meta):
        """Copy a file using the existing link command."""
        self.clipboard = {"action": "copy", "path": meta.get("path", ""), "name": meta.get("name", "")}
        self.log(f"Copied {meta.get('name', '')} to clipboard")

    def cut_file_from_tree(self, meta):
        """Cut a file using the existing link and unlink flow."""
        self.clipboard = {"action": "cut", "path": meta.get("path", ""), "name": meta.get("name", "")}
        self.log(f"Cut {meta.get('name', '')} to clipboard")

    def paste_into_directory(self, meta):
        """Paste the clipboard into the selected directory."""
        if not self.clipboard:
            return

        target_dir = meta.get("path", self.current_path) if meta.get("type") == "dir" else self.current_path
        source_path = self.clipboard.get("path", "")
        file_name = self.clipboard.get("name", "")
        if not source_path or not file_name:
            return

        target_path = posixpath.join(target_dir, file_name)
        if source_path == target_path and self.clipboard.get("action") == "cut":
            self.clipboard = None
            return

        output = self.execute_gui_command(f"link {source_path} {target_path}", update_entry=True, record_operation=False)
        if "already exists" in output:
            new_name = simpledialog.askstring("Paste File", "Target exists. Enter a new file name:", initialvalue=file_name, parent=self.root)
            if not new_name:
                return
            target_path = posixpath.join(target_dir, new_name.strip())
            output = self.execute_gui_command(f"link {source_path} {target_path}", update_entry=True, record_operation=False)

        if self.clipboard.get("action") == "cut" and "hard link" in output:
            self.execute_gui_command(f"unlink {source_path}", update_entry=True, record_operation=False)
            self.clipboard = None

        self.content_viewer.set_content(output)
        self.refresh_directory_tree()

    def share_file_to_user(self, meta):
        """Share a file by opening group and other permissions."""
        name = meta.get("name", "") if isinstance(meta, dict) else str(meta)
        file_path = meta.get("path", name) if isinstance(meta, dict) else str(meta)
        if not name:
            return

        username = simpledialog.askstring("Share File", "Enter the username to share with:", parent=self.root)
        if not username:
            return

        output = self.execute_gui_command(f"chmod {file_path} 666", update_entry=True)
        self.content_viewer.set_content(f"{output}\nShared {name} to {username.strip()}.")

    def grant_file_to_user(self, meta):
        """Grant a file into another user's directory using root commands."""
        if self.current_uid != 0:
            messagebox.showwarning("Grant Permission", "Only root can grant files to another user.")
            return

        source_path = meta.get("path", "")
        source_name = meta.get("name", "")
        if not source_path or not source_name:
            return

        username = simpledialog.askstring("Grant File", "Enter target username (for example usr2):", parent=self.root)
        if not username:
            return

        writable = messagebox.askyesno("Grant Mode", "Should the target user get write access too?")
        output = self.execute_gui_command(
            f"grant {source_path} {username.strip()} {1 if writable else 0}",
            update_entry=True,
            record_operation=False,
        )
        self.content_viewer.set_content(output)

    def open_directory_from_tree(self, meta):
        """Navigate into a directory from the tree."""
        target_path = meta.get("path", self.current_path)
        if target_path:
            self.navigate_to_path(target_path)

    def setup_user_paths(self, username):
        """Set the GUI browsing roots for the logged-in user."""
        if username == "root":
            self.home_path = "/"
            self.current_path = "/usr"
            self.navigate_to_path(self.current_path, record_operation=False)
        else:
            self.home_path = f"/usr/{username}"
            self.current_path = self.home_path

    def can_navigate_up(self):
        """Return whether the tree should offer a parent entry."""
        return self.current_path != self.home_path

    def navigate_to_path(self, path, record_operation=True):
        """Navigate to a directory and refresh the tree when successful."""
        path = path or self.current_path
        output = self.execute_gui_command(f"chdir {path}", update_entry=True, record_operation=record_operation)
        if "Chdir successful" in output:
            self.current_path = path
            self.refresh_directory_tree()
        return output

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

    def execute_gui_command(self, cmd, update_entry=False, record_operation=True):
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

            if cmd.startswith("chdir ") and "Chdir successful" in output:
                self.current_path = cmd.split(" ", 1)[1].strip()

            if cmd.strip() == "dir":
                self.file_tree_widget.update_tree(output, current_path=self.current_path, allow_parent=self.can_navigate_up())
                self.content_viewer.set_content("目录列表已更新")
            else:
                self.content_viewer.set_content(output)

            self.log("命令执行完成")

            if record_operation and self.logged_in and not cmd.startswith("login") and not cmd.startswith("logout"):
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

    def _display_cached_analysis(self):
        """Show the most recent saved analysis immediately if one exists."""
        cached = ai_integration.get_last_analysis()
        if not cached:
            return

        wrapped = {"learned_params": cached}
        self.content_viewer.set_content(self.format_analysis_result(wrapped))

        params = cached.get("parameters", {})
        if params:
            self.agent_panel.update_parameters(params)

    def _apply_analysis_result(self, result):
        """Apply analysis results back onto the GUI thread."""
        call_logs = ai_integration.get_agent_calls()
        if call_logs:
            self.call_logger.update_logs(call_logs)

        params = ai_integration.get_current_params()
        self.agent_panel.update_parameters(params)

        output_text = self.format_analysis_result(result)
        self.content_viewer.set_content(output_text)

        short_term = ai_integration.get_short_term_memory()
        long_term = ai_integration.get_long_term_memory()
        self.log(f"馃搳 鐭椂璁板繂: {len(short_term)} 鏉?| 闀挎椂璁板繂: {len(long_term)} 鏉?")
        self.log("鉁?鍒嗘瀽瀹屾垚")

    def _analysis_worker(self):
        """Run analysis in the background and marshal results to the UI thread."""
        try:
            result = ai_integration.run_analysis()
            self.root.after(0, lambda: self._apply_analysis_result(result))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"鉂?鍒嗘瀽澶辫触: {e}"))
        finally:
            self.root.after(0, self._finish_background_analysis)

    def _finish_background_analysis(self):
        """Reset background analysis state."""
        self.analysis_in_progress = False
        self.analysis_thread = None

    def _security_check_worker(self):
        """Run the periodic security analysis off the UI thread."""
        try:
            result = ai_integration.run_security_check()
            self.root.after(0, lambda: self._apply_security_check_result(result))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"Security check failed: {e}"))
        finally:
            self.root.after(0, self._finish_security_check)

    def _finish_security_check(self):
        """Reset periodic security check state."""
        self.security_check_in_progress = False
        self.security_check_thread = None

    def _apply_security_check_result(self, result):
        """Update the UI after a periodic security check completes."""
        if result.get("status") != "success":
            self.log(f"Security check skipped: {result.get('message', 'unknown error')}")
            return

        if result.get("alert_written"):
            alert_entry = result.get("alert_entry", {})
            uid = alert_entry.get("uid", result.get("uid"))
            reasons = ", ".join(alert_entry.get("reasons", []))
            self.log(f"Security alert recorded for uid {uid}: {reasons}")
        else:
            self.log("Security check completed with no alert.")

    def run_security_check_async(self):
        """Kick off the periodic security check without blocking the GUI."""
        if not self.logged_in:
            return

        if self.security_check_in_progress:
            self.log("Security check already running in background.")
            return

        self.security_check_in_progress = True
        self.security_check_thread = threading.Thread(target=self._security_check_worker, daemon=True)
        self.security_check_thread.start()

    def show_security_alerts_if_needed(self):
        """Show the shared security alert file to root once, then clear it."""
        if self.current_uid != 0:
            return

        alerts = ai_integration.get_security_alerts()
        if not alerts:
            return

        lines = ["Security anomalies detected:\n"]
        for index, alert in enumerate(alerts, start=1):
            reasons = "; ".join(alert.get("reasons", [])) or "No reason provided"
            suggestion = alert.get("security_suggestion", "")
            lines.append(
                f"{index}. User {alert.get('uid', 'N/A')} at {alert.get('timestamp', 'N/A')}\n"
                f"   Reasons: {reasons}\n"
                f"   Behavior: {alert.get('behavior_pattern', '')}\n"
                f"   Suggestion: {suggestion}"
            )

        messagebox.showwarning("Security Alerts", "\n\n".join(lines))
        ai_integration.clear_security_alerts()

    def run_analysis_async(self, show_cached=True):
        """Start analysis in the background without blocking the GUI."""
        if not self.logged_in:
            messagebox.showwarning("鎻愮ず", "璇峰厛鐧诲綍")
            return

        if show_cached:
            self._display_cached_analysis()

        if self.analysis_in_progress:
            self.log("馃敩 鍒嗘瀽姝ｅ湪鍚庡彴杩愯...")
            return

        self.analysis_in_progress = True
        self.log("馃敩 宸蹭娇鐢ㄤ笂娆″垎鏋愮粨鏋滐紝鍚庡彴姝ｅ湪鏇存柊鏈€鏂板垎鏋?..")
        self.analysis_thread = threading.Thread(target=self._analysis_worker, daemon=True)
        self.analysis_thread.start()

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

            if cmd.startswith("chdir ") and "Chdir successful" in output:
                self.current_path = cmd.split(" ", 1)[1].strip()

            if cmd.strip() == "dir":
                self.file_tree_widget.update_tree(output, current_path=self.current_path, allow_parent=self.can_navigate_up())
                self.content_viewer.set_content("目录列表已更新")
            else:
                self.content_viewer.set_content(output)

            self.log("✅ 命令执行完成")

            if cmd.startswith("login"):
                self.logged_in = True
                self.user_label.config(text="已登录")
                self.login_btn.config(text="登出", command=self.do_logout)
                self.current_uid = 100
                self.setup_user_paths("usr1")
                ai_integration.set_user(self.current_uid)
                self.init_orchestrator()
                self.log("📝 AI 记忆系统已激活")
                self.log("🔄 登录后自动分析...")
                self.run_analysis()
                self.start_auto_analysis()
                self.start_security_monitor()
                self.show_security_alerts_if_needed()

            elif cmd.startswith("logout"):
                self.logged_in = False
                self.user_label.config(text="未登录")
                self.login_btn.config(text="登录", command=self.show_login_dialog)
                self.current_uid = -1
                self.current_path = "/"
                self.home_path = "/"
                self.stop_auto_analysis()
                self.stop_security_monitor()
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
        self.run_analysis_async(show_cached=True)
        return
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

    def _repair_mojibake(self, text):
        """Best-effort repair for legacy mojibake strings before showing them."""
        if not isinstance(text, str) or not text:
            return text

        markers = ("馃", "閴", "鈴", "鉁", "鉂", "鈿", "鐧", "鏈", "宸", "鍒", "鏉", "璇", "闂")
        if not any(marker in text for marker in markers):
            return text

        candidates = [text]
        for src, dst in (("gb18030", "utf-8"), ("latin1", "utf-8"), ("cp1252", "utf-8")):
            try:
                candidates.append(text.encode(src, errors="ignore").decode(dst, errors="ignore"))
            except Exception:
                continue

        def score(value):
            if not value:
                return -10**9
            bad = sum(value.count(marker) for marker in markers)
            printable = sum(ch.isprintable() for ch in value)
            return printable - bad * 20

        best = max(candidates, key=score)
        return best or text

    def log(self, msg):
        self.log_widget.log(self._repair_mojibake(msg))

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
            self.setup_user_paths(username)
            ai_integration.set_user(self.current_uid)
            self.init_orchestrator()
            if self.orchestrator:
                self.orchestrator.set_user(self.current_uid)
            self.refresh_directory_tree()

            self.log("🔄 登录后自动分析...")
            self.run_analysis()

            self.start_auto_analysis()
            self.start_security_monitor()

            self.finish_operation()
            self.show_security_alerts_if_needed()
            messagebox.showinfo("成功", f"欢迎回来，{username}！")
        except Exception as e:
            messagebox.showerror("错误", f"登录异常: {str(e)}")
            self.log(f"❌ 登录异常: {e}")

    def do_logout(self):
        """执行登出"""
        self.stop_auto_analysis()
        self.stop_security_monitor()
        self.current_uid = -1
        self.current_path = "/"
        self.home_path = "/"
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

    def start_security_monitor(self):
        """Run the security behavior detection every 60 seconds."""
        self.stop_security_monitor()

        interval = 60 * 1000

        def scheduled_security_check():
            if self.logged_in:
                self.run_security_check_async()
            self.security_timer = self.root.after(interval, scheduled_security_check)

        self.security_timer = self.root.after(interval, scheduled_security_check)
        self.log("Security monitor started (every 60 seconds).")

    def stop_security_monitor(self):
        """Stop the periodic security monitor."""
        if self.security_timer:
            self.root.after_cancel(self.security_timer)
            self.security_timer = None
            self.log("Security monitor stopped.")

    def finish_operation(self):
        """操作后更新"""
        try:
            storage_status = self.client.get_storage_status()
            self.space_widget.blocks = storage_status["blocks"]
            self.space_widget.draw_blocks()

            used_block_bytes = sum(storage_status["blocks"]) * 512
            used_inodes = sum(storage_status["inodes"])
            inode_bytes = used_inodes * self.client.INODE_SIZE
            total_used_bytes = used_block_bytes + inode_bytes
            self.space_widget.update(
                total_used_bytes,
                Filesystem.TOTAL_BYTES,
                used_inodes=used_inodes,
                inode_bytes=inode_bytes,
            )
        except Exception:
            pass

        self.root.after(100, lambda: None)


def main():
    root = tk.Tk()
    app = FileSystemGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
