#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KFS Agent - 智能文件分类优化智能体
"""
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class KFSAgent(BaseAgent):
    def __init__(self, config: Dict = None):
        super().__init__("KFS", config)
    
    def process(self, guidance: str, context: Dict) -> Dict:
        """
        根据全局指导，调用 GLM 分析文件分类优化
        
        Args:
            guidance: Analyzer 给出的全局指导
            context: 会话上下文
        
        Returns:
            KFS 优化方案
        """
        self.log("根据全局指导，分析 KFS 优化...")
        
        prompt = self._build_prompt(guidance, context)
        
        try:
            glm_suggestion = call_glm_api(prompt, config=self.config)
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": glm_suggestion,
                "parameters": {
                    "auto_tagging_enabled": True,
                    "category_rules": []
                }
            }
        except Exception as e:
            self.log(f"KFS 分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "suggestion": "保持当前 KFS 配置",
                "parameters": {}
            }
    
    def _build_prompt(self, guidance: str, context: Dict) -> str:
        recent_files = context.get("recent_files", [])
        
        return f"""你是一位智能文件分类专家。

【全局优化指导】
{guidance}

【当前上下文】
最近访问文件: {recent_files}

【任务】
请给出 KFS (智能文件分类) 模块的具体优化建议，并说明理由。
"""
