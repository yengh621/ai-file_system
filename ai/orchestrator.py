#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
智能体编排器
- 协调多个 Agent 工作
- 调用 GLM
- 保存记忆
"""
import os
from datetime import datetime
from typing import Dict
from .memory.recorder import MemoryRecorder
from .agents.analyzer_agent import AnalyzerAgent
from .agents.kfs_agent import KFSAgent
from .agents.io_agent import IOAgent
from .agents.security_agent import SecurityAgent


class AgentOrchestrator:
    def __init__(self, memory_dir="debug_memory", config: Dict = None):
        self.memory_dir = memory_dir
        self.recorder = MemoryRecorder(memory_dir)
        self.config = config or {}
        
        # 初始化各个 Agent
        self.analyzer_agent = AnalyzerAgent(config=self.config)
        self.kfs_agent = KFSAgent(config=self.config, memory_dir=memory_dir)
        self.io_agent = IOAgent(config=self.config, memory_dir=memory_dir)
        self.security_agent = SecurityAgent(config=self.config, memory_dir=memory_dir)
        
        # 最新分析结果文件
        self.last_analysis_file = os.path.join(memory_dir, "agent", "memory", "long_term", "last_analysis.json")
    
    def set_user(self, uid: int) -> Dict:
        return self.recorder.set_user(uid)
    
    def clear_user(self) -> Dict:
        return self.recorder.clear_user()
    
    def record_operation(self, operation: str, path: str = None) -> Dict:
        return self.recorder.record_operation(operation, path)
    
    def get_context(self) -> Dict:
        return self.recorder.get_context()
    
    def run_full_analysis(self) -> Dict:
        """
        运行完整分析流程
        1. Analyzer Agent - 分析行为模式（文字）
        2. KFS Agent - 文件分类分析（参照上下文）
        3. IO Agent - IO 优化分析（参照上下文，给出参数）
        4. Security Agent - 安全分析（参照上下文，给出参数）
        5. 保存结果到长时记忆
        """
        print("=== 多智能体协同分析 ===")
        
        context_result = self.recorder.get_context()
        if context_result.get("status") != "success":
            return context_result
        
        context = context_result
        uid = context.get("uid", -1)
        
        # 步骤 1：Analyzer Agent - 行为模式分析（文字）
        print("\n🤖 [1/4] Analyzer Agent - 分析用户行为模式...")
        analyzer_result = self.analyzer_agent.process(context)
        behavior_pattern = analyzer_result.get("behavior_pattern", "")
        
        # 三个 Agent 参照上下文同时进行优化（没有前后依赖）
        print("\n🤖 [2/4] KFS Agent - 文件分类分析...")
        kfs_result = self.kfs_agent.process(behavior_pattern, context)
        
        print("\n🤖 [3/4] IO Agent - IO 优化分析...")
        io_result = self.io_agent.process(behavior_pattern, context)
        
        print("\n🤖 [4/4] Security Agent - 安全分析...")
        security_result = self.security_agent.process(behavior_pattern, context)
        
        # 保存学习参数
        print("\n💾 更新学习参数...")
        self._save_learned_params(uid, io_result, security_result, kfs_result)
        
        # 保存最新分析结果
        print("\n💾 保存最新分析结果...")
        self._save_last_analysis(uid, behavior_pattern, io_result, security_result, kfs_result)
        
        # 获取学习参数（保持兼容）
        learned_params = self.recorder.get_learned_params(uid)
        
        return {
            "status": "success",
            "analyzer_result": analyzer_result,
            "kfs_result": kfs_result,
            "io_result": io_result,
            "security_result": security_result,
            "learned_params": learned_params
        }
    
    def _save_learned_params(self, uid: int, io_result: Dict, security_result: Dict, kfs_result: Dict):
        """保存学习参数到 learned_params.json"""
        try:
            # 收集所有智能体的参数
            params_to_save = {
                "suggested_prefetch_window": io_result.get("parameters", {}).get("prefetch_window", 3),
                "suggested_delete_threshold": security_result.get("parameters", {}).get("delete_threshold", 5),
                "suggested_modify_threshold": security_result.get("parameters", {}).get("modify_threshold", 10),
                "auto_tagging_enabled": kfs_result.get("parameters", {}).get("auto_tagging_enabled", True),
                "category_rules": kfs_result.get("parameters", {}).get("category_rules", [])
            }
            
            # 保存到 recorder
            self.recorder.save_learned_params(uid, params_to_save)
            print(f"✅ 学习参数已更新: {params_to_save}")
        except Exception as e:
            print(f"❌ 保存学习参数失败: {e}")
    
    def get_optimization_config(self) -> Dict:
        """获取当前优化配置"""
        if self.recorder.current_uid == -1:
            return {
                "status": "error",
                "message": "无当前用户"
            }
        
        learned_params = self.recorder.get_learned_params(self.recorder.current_uid)
        return {
            "status": "success",
            "prefetch_window": learned_params.get("suggested_prefetch_window", 3),
            "delete_threshold": learned_params.get("suggested_delete_threshold", 5),
            "modify_threshold": learned_params.get("suggested_modify_threshold", 10),
            "auto_tagging_enabled": learned_params.get("auto_tagging_enabled", True),
            "category_rules": learned_params.get("category_rules", [])
        }
    
    def get_last_analysis(self) -> Dict:
        """获取最新分析结果"""
        if os.path.exists(self.last_analysis_file):
            import json
            try:
                with open(self.last_analysis_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            except:
                pass
        return {"status": "error", "message": "暂无分析结果"}
    
    def _save_last_analysis(self, uid: int, behavior_pattern: str, io_result: Dict, security_result: Dict, kfs_result: Dict):
        """保存最新分析结果"""
        os.makedirs(os.path.dirname(self.last_analysis_file), exist_ok=True)
        
        analysis_data = {
            "uid": uid,
            "timestamp": datetime.now().isoformat(),
            "behavior_pattern": behavior_pattern,
            "kfs_suggestion": kfs_result.get("suggestion", ""),
            "io_suggestion": io_result.get("suggestion", ""),
            "security_suggestion": security_result.get("suggestion", ""),
            "parameters": {
                "prefetch_window": io_result.get("parameters", {}).get("prefetch_window", 3),
                "delete_threshold": security_result.get("parameters", {}).get("delete_threshold", 5),
                "modify_threshold": security_result.get("parameters", {}).get("modify_threshold", 10),
                "auto_tagging_enabled": kfs_result.get("parameters", {}).get("auto_tagging_enabled", True),
                "category_rules": kfs_result.get("parameters", {}).get("category_rules", [])
            }
        }
        
        import json
        with open(self.last_analysis_file, "w", encoding="utf-8") as f:
            json.dump(analysis_data, f, ensure_ascii=False, indent=2)
