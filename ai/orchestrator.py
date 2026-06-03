#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
智能体编排器
- 协调多个 Agent 工作
- 调用 GLM
- 保存记忆
"""
import os
import json
from datetime import datetime
from typing import Dict
from .memory.recorder import MemoryRecorder
from .agents.analyzer_agent import AnalyzerAgent
from .agents.kfs_agent import KFSAgent
from .agents.io_agent import IOAgent
from .agents.security_agent import SecurityAgent


def load_config():
    """加载配置文件"""
    config_file = "config.json"
    if os.path.exists(config_file):
        try:
            with open(config_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {}


class AgentOrchestrator:
    def __init__(self, memory_dir="debug_memory", config: Dict = None):
        self.memory_dir = memory_dir
        self.recorder = MemoryRecorder(memory_dir)
        self.config = config or load_config()
        self.security_alert_file = os.path.join(self.memory_dir, "security_alerts.json")
        
        # 调用记录文件
        self.call_log_file = self.recorder.agent_calls_file
        self.call_logs = self._load_call_logs()
        
        # 初始化各个 Agent
        self.analyzer_agent = AnalyzerAgent(config=self.config)
        self.kfs_agent = KFSAgent(config=self.config, memory_dir=memory_dir)
        self.io_agent = IOAgent(config=self.config, memory_dir=memory_dir)
        self.security_agent = SecurityAgent(config=self.config, memory_dir=memory_dir)
        self._sync_user_scoped_paths()
    
    def set_user(self, uid: int) -> Dict:
        result = self.recorder.set_user(uid)
        self._sync_user_scoped_paths()
        return result
    
    def clear_user(self) -> Dict:
        result = self.recorder.clear_user()
        self._sync_user_scoped_paths()
        return result
    
    def record_operation(self, operation: str, path: str = None) -> Dict:
        return self.recorder.record_operation(operation, path)

    def _sync_user_scoped_paths(self):
        self.call_log_file = self.recorder.agent_calls_file
        self.call_logs = self._load_call_logs()
        learned_params_file = self.recorder.learned_params_file
        self.kfs_agent.learned_params_file = learned_params_file
        self.io_agent.learned_params_file = learned_params_file
        self.security_agent.learned_params_file = learned_params_file
        self.kfs_agent.kfs_stats_file = os.path.join(self.recorder.user_dir, "kfs_stats.json")
        self.io_agent.io_stats_file = os.path.join(self.recorder.user_dir, "io_stats.json")
    
    def get_context(self) -> Dict:
        return self.recorder.get_context()
    
    def run_full_analysis(self) -> Dict:
        """
        运行完整分析流程
        1. Analyzer Agent - 分析行为模式（只读取 short_term + all_operations）
        2. KFS Agent - 热点文件分析（读取分析结果 + kfs_stats.json）
        3. IO Agent - IO 优化分析（读取分析结果 + io_stats.json）
        4. Security Agent - 安全分析（读取分析结果）
        5. 保存完整结果到 learned_params.json
        """
        print("=== 多智能体协同分析 ===")
        
        context_result = self.recorder.get_context()
        if context_result.get("status") != "success":
            return context_result
        
        context = context_result
        uid = context.get("uid", -1)
        
        # 步骤 1：Analyzer Agent - 行为模式分析（只读取 short_term + all_operations）
        print("\n🤖 [1/4] Analyzer Agent - 分析用户行为模式...")
        analyzer_result = self.analyzer_agent.process(context)
        behavior_pattern = analyzer_result.get("behavior_pattern", "")
        self._log_agent_call("Analyzer", {"context": context}, analyzer_result)
        
        # 步骤 2：KFS Agent - 热点文件分析（读取分析结果 + kfs_stats.json）
        print("\n🤖 [2/4] KFS Agent - 热点文件分析...")
        kfs_result = self.kfs_agent.process(behavior_pattern)
        self._log_agent_call("KFS", {"guidance": behavior_pattern}, kfs_result)
        
        # 步骤 3：IO Agent - IO 优化分析（读取分析结果 + io_stats.json）
        print("\n🤖 [3/4] IO Agent - IO 优化分析...")
        io_result = self.io_agent.process(behavior_pattern)
        self._log_agent_call("IO", {"guidance": behavior_pattern}, io_result)
        
        # 步骤 4：Security Agent - 安全分析（读取分析结果）
        print("\n🤖 [4/4] Security Agent - 安全分析...")
        security_result = self.security_agent.process(behavior_pattern, context)
        self._log_agent_call("Security", {"guidance": behavior_pattern, "context": context}, security_result)
        
        # 保存完整分析结果到 learned_params.json（覆盖旧的）
        print("\n💾 保存完整分析结果...")
        self._save_full_analysis(uid, behavior_pattern, io_result, security_result, kfs_result)
        
        # 获取当前参数
        current_params = self.recorder.get_current_params()
        
        return {
            "status": "success",
            "analyzer_result": analyzer_result,
            "kfs_result": kfs_result,
            "io_result": io_result,
            "security_result": security_result,
            "learned_params": self.recorder.learned_params,
            "call_logs": self.get_call_logs(4),
            "agent_parameters": self.get_agent_parameters()
        }

    def run_security_check(self) -> Dict:
        context_result = self.recorder.get_context()
        if context_result.get("status") != "success":
            return context_result

        context = context_result
        uid = context.get("uid", -1)
        analyzer_result = self.analyzer_agent.process(context)
        behavior_pattern = analyzer_result.get("behavior_pattern", "")
        self._log_agent_call("Analyzer", {"context": context, "security_check": True}, analyzer_result)

        security_result = self.security_agent.process(behavior_pattern, context)
        self._log_agent_call(
            "Security",
            {"guidance": behavior_pattern, "context": context, "security_check": True},
            security_result,
        )

        alert_entry = self._build_security_alert(uid, behavior_pattern, context, security_result)
        if alert_entry is not None:
            self._append_security_alert(alert_entry)

        return {
            "status": "success",
            "uid": uid,
            "analyzer_result": analyzer_result,
            "security_result": security_result,
            "alert_written": alert_entry is not None,
            "alert_entry": alert_entry,
        }
    
    def _save_full_analysis(self, uid: int, behavior_pattern: str, io_result: Dict, security_result: Dict, kfs_result: Dict):
        """保存完整分析结果到 learned_params.json"""
        try:
            # 构建完整分析数据
            analysis_data = {
                "uid": uid,
                "timestamp": "",
                "behavior_pattern": behavior_pattern,
                "kfs_suggestion": kfs_result.get("suggestion", ""),
                "io_suggestion": io_result.get("suggestion", ""),
                "security_suggestion": security_result.get("suggestion", ""),
                "parameters": {
                    "file_prefetch_windows": io_result.get("parameters", {}).get("file_prefetch_windows", {}),
                    "delete_threshold": security_result.get("parameters", {}).get("delete_threshold", 5),
                    "modify_threshold": security_result.get("parameters", {}).get("modify_threshold", 10),
                    "auto_tagging_enabled": kfs_result.get("parameters", {}).get("auto_tagging_enabled", True),
                    "category_rules": kfs_result.get("parameters", {}).get("category_rules", []),
                    "hot_files": kfs_result.get("parameters", {}).get("hot_files", [])
                }
            }
            
            # 保存到 recorder（覆盖旧的）
            self.recorder.save_full_analysis(analysis_data)
            print(f"✅ 完整分析结果已保存: {analysis_data['parameters']}")
        except Exception as e:
            print(f"❌ 保存分析结果失败: {e}")
    
    def _build_security_alert(self, uid: int, behavior_pattern: str, context: Dict, security_result: Dict):
        recent_stats = context.get("recent_stats", {})
        operation_counts = recent_stats.get("operation_counts", {})
        delete_count = int(operation_counts.get("delete", 0))
        modify_count = (
            int(operation_counts.get("write", 0))
            + int(operation_counts.get("chmod", 0))
            + int(operation_counts.get("create", 0))
        )

        params = security_result.get("parameters", {})
        delete_threshold = int(params.get("delete_threshold", 5))
        modify_threshold = int(params.get("modify_threshold", 10))

        reasons = []
        if delete_count >= delete_threshold:
            reasons.append(f"delete count {delete_count} reached threshold {delete_threshold}")
        if modify_count >= modify_threshold:
            reasons.append(f"modify count {modify_count} reached threshold {modify_threshold}")

        if not reasons:
            return None

        return {
            "uid": uid,
            "timestamp": datetime.now().isoformat(),
            "behavior_pattern": behavior_pattern,
            "security_suggestion": security_result.get("suggestion", ""),
            "delete_count": delete_count,
            "modify_count": modify_count,
            "delete_threshold": delete_threshold,
            "modify_threshold": modify_threshold,
            "reasons": reasons,
            "recent_stats": recent_stats,
        }

    def _append_security_alert(self, alert_entry: Dict):
        alerts = []
        if os.path.exists(self.security_alert_file):
            try:
                with open(self.security_alert_file, "r", encoding="utf-8") as f:
                    loaded = json.load(f)
                if isinstance(loaded, list):
                    alerts = loaded
            except Exception:
                alerts = []

        alerts.append(alert_entry)
        os.makedirs(os.path.dirname(self.security_alert_file), exist_ok=True)
        with open(self.security_alert_file, "w", encoding="utf-8") as f:
            json.dump(alerts[-100:], f, ensure_ascii=False, indent=2)

    def get_optimization_config(self) -> Dict:
        """获取当前优化配置"""
        if self.recorder.current_uid == -1:
            return {
                "status": "error",
                "message": "无当前用户"
            }
        
        current_params = self.recorder.get_current_params()
        return {
            "status": "success",
            "file_prefetch_windows": current_params.get("file_prefetch_windows", {}),
            "delete_threshold": current_params.get("delete_threshold", 5),
            "modify_threshold": current_params.get("modify_threshold", 10),
            "auto_tagging_enabled": current_params.get("auto_tagging_enabled", True),
            "category_rules": current_params.get("category_rules", []),
            "hot_files": current_params.get("hot_files", [])
        }
    
    def get_last_analysis(self) -> Dict:
        """获取最新分析结果"""
        return {"status": "success", **self.recorder.learned_params}
    
    def _load_call_logs(self):
        """加载调用记录"""
        if os.path.exists(self.call_log_file):
            try:
                with open(self.call_log_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return []
    
    def _save_call_logs(self):
        """保存调用记录"""
        os.makedirs(os.path.dirname(self.call_log_file), exist_ok=True)
        with open(self.call_log_file, "w", encoding="utf-8") as f:
            json.dump(self.call_logs, f, ensure_ascii=False, indent=2)
    
    def _log_agent_call(self, agent_name: str, input_data: Dict, output_data: Dict):
        """记录智能体调用"""
        log_entry = {
            "timestamp": datetime.now().isoformat(),
            "agent": agent_name,
            "input": input_data,
            "output": output_data,
            "parameters": output_data.get("parameters", {})
        }
        self.call_logs.append(log_entry)
        # 只保留最近 100 条记录
        if len(self.call_logs) > 100:
            self.call_logs = self.call_logs[-100:]
        self._save_call_logs()
    
    def get_call_logs(self, limit: int = 20):
        """获取调用记录"""
        return self.call_logs[-limit:]
    
    def get_agent_parameters(self):
        """获取三个主要智能体的参数"""
        return {
            "io_agent": {
                "parameter": "file_prefetch_windows",
                "description": "每个文件的预取窗口大小（1-10），顺序文件用大窗口，随机文件用小窗口"
            },
            "security_agent": {
                "parameters": [
                    {"name": "delete_threshold", "description": "每分钟删除操作阈值（5-20）"},
                    {"name": "modify_threshold", "description": "每分钟修改操作阈值（5-20）"}
                ]
            },
            "kfs_agent": {
                "parameters": [
                    {"name": "auto_tagging_enabled", "description": "是否启用自动标签"},
                    {"name": "category_rules", "description": "文件分类规则"},
                    {"name": "hot_files", "description": "热点文件列表"}
                ]
            }
        }
