#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyzer Agent - 全局分析智能体
负责调用 GLM，只分析用户行为模式（文字描述）
"""
import json
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class AnalyzerAgent(BaseAgent):
    def __init__(self, config: Dict = None):
        super().__init__("Analyzer", config)
    
    def process(self, context: Dict) -> Dict:
        """
        分析上下文，调用 GLM 给出行为模式描述
        
        Args:
            context: 会话上下文，包含 recent_stats, all_stats, recent_ops, historical_ops
        
        Returns:
            行为模式描述
        """
        self.log("开始调用 GLM 分析用户行为模式...")
        
        prompt = self._build_prompt(context)
        
        try:
            glm_analysis = call_glm_api(prompt, config=self.config)
            
            self.log("GLM 分析完成")
            
            return {
                "status": "success",
                "agent": self.name,
                "behavior_pattern": glm_analysis
            }
        except Exception as e:
            self.log(f"分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "behavior_pattern": "用户行为模式分析失败"
            }
    
    def _build_prompt(self, context: Dict) -> str:
        """构建 GLM 提示词"""
        recent_stats = context.get("recent_stats", {})
        all_stats = context.get("all_stats", {})
        recent_ops = context.get("recent_ops", [])
        historical_ops = context.get("historical_ops", [])
        
        return f"""你是一个专业的用户行为分析专家。请分析以下用户文件系统操作数据，用文字描述用户在以下三个方面的行为模式（各50字左右）：

1. 【文件访问行为】- 用户主要访问哪些类型的文件？读写频率如何？
2. 【修改行为】- 用户修改文件的频率和幅度如何？
3. 【删除行为】- 用户删除操作的频率和特征如何？

【最近行为】
操作总数: {recent_stats.get('total_ops', 0)}
操作统计: {json.dumps(recent_stats.get('operation_counts', {}), ensure_ascii=False)}
文件类型: {json.dumps(recent_stats.get('file_types', {}), ensure_ascii=False)}

【历史行为】
操作总数: {all_stats.get('total_ops', 0)}
操作统计: {json.dumps(all_stats.get('operation_counts', {}), ensure_ascii=False)}
文件类型: {json.dumps(all_stats.get('file_types', {}), ensure_ascii=False)}

请只用自然语言描述，不要给出任何数字参数或 JSON 格式。
"""
