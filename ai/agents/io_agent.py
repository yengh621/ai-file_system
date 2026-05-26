#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IO Agent - I/O 预取优化智能体
"""
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class IOAgent(BaseAgent):
    def __init__(self, config: Dict = None):
        super().__init__("IO", config)
    
    def process(self, guidance: str, context: Dict) -> Dict:
        """
        根据全局指导，调用 GLM 分析 IO 优化
        
        Args:
            guidance: Analyzer 给出的全局指导
            context: 会话上下文
        
        Returns:
            IO 优化方案
        """
        self.log("根据全局指导，分析 IO 优化...")
        
        prompt = self._build_prompt(guidance, context)
        
        try:
            glm_suggestion = call_glm_api(prompt, config=self.config)
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": glm_suggestion,
                "parameters": {
                    "prefetch_window": 3
                }
            }
        except Exception as e:
            self.log(f"IO 分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "suggestion": "保持当前 IO 配置",
                "parameters": {"prefetch_window": 3}
            }
    
    def _build_prompt(self, guidance: str, context: Dict) -> str:
        op_counts = context.get("operation_counts", {})
        
        return f"""你是一位 I/O 优化专家。

【全局优化指导】
{guidance}

【当前上下文】
操作统计: {json.dumps(op_counts, ensure_ascii=False)}

【任务】
请给出 I/O 预取策略的具体优化建议，并给出建议的 prefetch_window 值（整数，范围 1-10）。

请仅返回一行 JSON 格式，格式如下：
{{
  "reason": "你的理由",
  "prefetch_window": 5
}}
"""
