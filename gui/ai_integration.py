
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI 集成模块 - 与多智能体系统交互
"""
import os
import sys
import json
from .styles import Filesystem

# 添加 AI 模块路径
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from ai.memory.recorder import MemoryRecorder
from ai.orchestrator import AgentOrchestrator


class AIIntegration:
    """AI 集成类 - 连接 GUI 和 AI 智能体"""
    
    def __init__(self):
        self.recorder = MemoryRecorder()
        self.orchestrator = None
        self.current_uid = -1
    
    def set_user(self, uid):
        """设置当前用户并初始化记忆记录"""
        self.current_uid = uid
        self.recorder.set_user(uid)
        print(f"[AI Integration] 用户 {uid} 已设置，开始记录操作")
    
    def clear_user(self):
        """清除当前用户"""
        self.recorder.clear_user()
        self.current_uid = -1
        print("[AI Integration] 用户已登出")
    
    def record_operation(self, operation, path=None):
        """记录操作到记忆系统"""
        if self.current_uid == -1:
            return {"status": "error", "message": "未登录"}
        return self.recorder.record_operation(operation, path)
    
    def get_short_term_memory(self):
        """获取短时记忆（最近操作）"""
        context = self.recorder.get_context()
        if context.get("status") == "success":
            return context.get("recent_ops", [])
        return []
    
    def get_long_term_memory(self):
        """获取长时记忆（历史操作）"""
        context = self.recorder.get_context()
        if context.get("status") == "success":
            return context.get("historical_ops", [])
        return []
    
    def run_analysis(self):
        """运行完整的 AI 分析"""
        if self.current_uid == -1:
            return {"status": "error", "message": "请先登录"}
        
        if not self.orchestrator:
            self.orchestrator = AgentOrchestrator()
        
        # 设置用户
        self.orchestrator.set_user(self.current_uid)
        
        # 执行分析
        result = self.orchestrator.run_full_analysis()

        # The orchestrator saves the canonical learned_params shape. Keep this
        # recorder in sync without overwriting it with the outer result wrapper.
        if result.get("status") == "success":
            learned_params = result.get("learned_params")
            if isinstance(learned_params, dict) and isinstance(learned_params.get("parameters"), dict):
                self.recorder.learned_params = learned_params
            else:
                self.recorder._load_all()
        
        return result
    
    def get_current_params(self):
        """获取当前学习参数"""
        self.recorder._load_all()
        params = self._extract_parameters(self.recorder.learned_params)
        return params or self.recorder.get_current_params()
    
    def rename_hot_file_path(self, old_path, new_path):
        """Keep saved AI hot-file paths aligned with a filesystem rename."""
        if self.current_uid == -1:
            return False

        self.recorder._load_all()
        params = self._extract_parameters(self.recorder.learned_params)
        hot_files = params.get("hot_files", []) if isinstance(params, dict) else []
        changed = False

        for item in hot_files:
            if not isinstance(item, dict) or item.get("path") != old_path:
                continue
            item["path"] = new_path
            item["filename"] = new_path.rstrip("/").rsplit("/", 1)[-1]
            changed = True

        if changed:
            self.recorder._save_learned_params()
        return changed

    def get_agent_calls(self):
        """获取智能体调用记录"""
        try:
            path = self.recorder.agent_calls_file
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f)
        except Exception as e:
            print(f"[AI Integration] 读取调用记录失败: {e}")
        return []

    def get_last_analysis(self):
        """Return the last saved analysis for the active user."""
        try:
            path = self.recorder.learned_params_file
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if isinstance(data, dict):
                    return self._extract_analysis(data)
        except Exception as e:
            print(f"[AI Integration] failed to read last analysis: {e}")
        return None

    def _extract_analysis(self, data):
        """Return a canonical learned analysis from current or legacy shapes."""
        if not isinstance(data, dict):
            return None
        if isinstance(data.get("parameters"), dict):
            return data
        learned_params = data.get("learned_params")
        if isinstance(learned_params, dict) and isinstance(learned_params.get("parameters"), dict):
            return learned_params

        params = self._extract_parameters(data)
        if not params:
            return data
        return {
            "uid": data.get("uid", self.current_uid),
            "timestamp": data.get("timestamp", data.get("last_updated", "")),
            "behavior_pattern": data.get("behavior_pattern", ""),
            "kfs_suggestion": data.get("kfs_suggestion", data.get("kfs_result", {}).get("suggestion", "")),
            "io_suggestion": data.get("io_suggestion", data.get("io_result", {}).get("suggestion", "")),
            "security_suggestion": data.get("security_suggestion", data.get("security_result", {}).get("suggestion", "")),
            "parameters": params,
        }

    def _extract_parameters(self, data):
        """Extract flattened agent parameters from saved or live analysis data."""
        if not isinstance(data, dict):
            return {}
        if isinstance(data.get("parameters"), dict):
            return data["parameters"]
        learned_params = data.get("learned_params")
        if isinstance(learned_params, dict):
            nested = self._extract_parameters(learned_params)
            if nested:
                return nested

        params = {}
        for key in ("io_result", "security_result", "kfs_result"):
            result_params = data.get(key, {}).get("parameters", {})
            if isinstance(result_params, dict):
                params.update(result_params)
        return params

    def run_security_check(self):
        """Run the periodic security check for the active user."""
        if self.current_uid == -1:
            return {"status": "error", "message": "please login first"}

        if not self.orchestrator:
            self.orchestrator = AgentOrchestrator()

        self.orchestrator.set_user(self.current_uid)
        return self.orchestrator.run_security_check()

    def get_security_alerts(self):
        """Read the shared security alert file."""
        path = os.path.join(self.recorder.memory_dir, "security_alerts.json")
        if not os.path.exists(path):
            return []

        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, list) else []
        except Exception as e:
            print(f"[AI Integration] failed to read security alerts: {e}")
            return []

    def clear_security_alerts(self):
        """Clear the shared security alert file after root has viewed it."""
        path = os.path.join(self.recorder.memory_dir, "security_alerts.json")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump([], f, ensure_ascii=False, indent=2)


def get_used_space():
    """获取已用空间（估算）"""
    try:
        recorder = MemoryRecorder()
        record_path = recorder.long_term_file
        if os.path.exists(record_path):
            with open(record_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                return min(Filesystem.TOTAL_BYTES, len(data) * 500)
        return 32768  # 默认 32KB
    except Exception:
        return 32768


def get_last_analysis():
    """获取最后一次分析结果"""
    try:
        recorder = MemoryRecorder()
        path = recorder.learned_params_file
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
    except Exception as e:
        print(f"[AI Integration] 读取分析结果失败: {e}")
    return None


def format_analysis(data):
    """格式化分析结果为文本"""
    if not data:
        return "暂无分析结果"

    params = data.get('parameters', {})
    return f"""📊 AI 优化建议

👤 用户: {data.get('uid', 'N/A')}
⏰ 时间: {data.get('timestamp', 'N/A')}

📝 行为模式:
{data.get('behavior_pattern', '暂无数据')}

📁 KFS 建议:
{data.get('kfs_suggestion', '暂无数据')}

⚡ IO 建议:
{data.get('io_suggestion', '暂无数据')}

🔒 Security 建议:
{data.get('security_suggestion', '暂无数据')}

⚙️ 建议参数:
- 预取窗口: {params.get('prefetch_window', 3)}
- 删除阈值: {params.get('delete_threshold', 8)}
- 修改阈值: {params.get('modify_threshold', 12)}
- 热点文件: {len(params.get('hot_files', []))} 个
"""


# 全局实例
ai_integration = AIIntegration()
