#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Security Agent - 安全检测优化智能体
"""
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class SecurityAgent(BaseAgent):
    def __init__(self, config: Dict = None):
        super().__init__("Security", config)
    
    def process(self, guidance: str, context: Dict) -> Dict:
        """
        根据全局指导，调用 GLM 分析安全优化
        
        Args:
            guidance: Analyzer 给出的全局指导
            context: 会话上下文
        
        Returns:
            安全优化方案
        """
        self.log("根据全局指导，分析安全优化...")
        
        prompt = self._build_prompt(guidance, context)
        
        try:
            glm_suggestion = call_glm_api(prompt, config=self.config)
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": glm_suggestion,
                "parameters": {
                    "delete_threshold": 5,
                    "modify_threshold": 10
                }
            }
        except Exception as e:
            self.log(f"Security 分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "suggestion": "保持当前安全配置",
                "parameters": {
                    "delete_threshold": 5,
                    "modify_threshold": 10
                }
            }
    
    def _build_prompt(self, guidance: str, context: Dict) -> str:
        op_counts = context.get("operation_counts", {})
        
        return f"""你是一位安全检测专家。

【全局优化指导】
{guidance}

【当前上下文】
操作统计: {json.dumps(op_counts, ensure_ascii=False)}

【任务】
请给出安全检测阈值的具体优化建议，并给出建议值。
- delete_threshold: 每分钟删除操作阈值
- modify_threshold: 每分钟修改操作阈值

请仅返回一行 JSON 格式，格式如下：
{{
  "reason": "你的理由",
  "delete_threshold": 5,
  "modify_threshold": 10
}}
"""
