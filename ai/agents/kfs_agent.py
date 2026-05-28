#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KFS Agent - 智能文件分类优化智能体
"""
import json
import os
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class KFSAgent(BaseAgent):
    def __init__(self, config: Dict = None, memory_dir: str = "debug_memory"):
        super().__init__("KFS", config)
        self.memory_dir = memory_dir
        self.learned_params_file = os.path.join(memory_dir, "agent", "memory", "long_term", "learned_params.json")
    
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
        
        # 获取当前用户 ID 和最近参数
        uid = context.get("uid", -1)
        current_params = self._get_current_params(uid)
        
        prompt = self._build_prompt(guidance, context, current_params)
        
        try:
            glm_response = call_glm_api(prompt, config=self.config)
            self.log(f"GLM 响应: {glm_response}")
            
            # 解析 JSON 响应
            parsed_result = self._parse_json_response(glm_response)
            
            # 使用解析结果或回退到当前参数
            auto_tagging_enabled = parsed_result.get("auto_tagging_enabled", current_params.get("auto_tagging_enabled", True))
            category_rules = parsed_result.get("category_rules", current_params.get("category_rules", []))
            reason = parsed_result.get("reason", "保持当前配置")
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": reason,
                "parameters": {
                    "auto_tagging_enabled": auto_tagging_enabled,
                    "category_rules": category_rules
                }
            }
        except Exception as e:
            self.log(f"KFS 分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "suggestion": "保持当前 KFS 配置",
                "parameters": {
                    "auto_tagging_enabled": True,
                    "category_rules": []
                }
            }
    
    def _get_current_params(self, uid: int) -> Dict:
        """获取当前用户的最近参数"""
        if not os.path.exists(self.learned_params_file):
            return {"auto_tagging_enabled": True, "category_rules": []}
        
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                all_params = json.load(f)
            return all_params.get(str(uid), {"auto_tagging_enabled": True, "category_rules": []})
        except Exception as e:
            self.log(f"读取最近参数失败: {e}")
            return {"auto_tagging_enabled": True, "category_rules": []}
    
    def _parse_json_response(self, response: str) -> Dict:
        """解析 GLM 返回的 JSON 响应"""
        try:
            # 尝试直接解析
            return json.loads(response)
        except:
            pass
        
        try:
            # 尝试提取 JSON 部分
            import re
            json_match = re.search(r'\{[\s\S]*\}', response)
            if json_match:
                return json.loads(json_match.group(0))
        except Exception as e:
            self.log(f"JSON 解析失败: {e}")
        
        return {}
    
    def _build_prompt(self, guidance: str, context: Dict, current_params: Dict) -> str:
        recent_stats = context.get("recent_stats", {})
        all_stats = context.get("all_stats", {})
        recent_files = recent_stats.get("recent_files", [])
        current_tagging = current_params.get("auto_tagging_enabled", True)
        current_rules = current_params.get("category_rules", [])
        
        return f"""你是一位智能文件分类专家。

【全局优化指导】
{guidance}

【最近行为统计】
操作总数: {recent_stats.get('total_ops', 0)}
操作统计: {json.dumps(recent_stats.get('operation_counts', {}), ensure_ascii=False)}
文件类型: {json.dumps(recent_stats.get('file_types', {}), ensure_ascii=False)}
最近访问文件: {recent_files}

【历史行为统计】
操作总数: {all_stats.get('total_ops', 0)}
操作统计: {json.dumps(all_stats.get('operation_counts', {}), ensure_ascii=False)}
文件类型: {json.dumps(all_stats.get('file_types', {}), ensure_ascii=False)}

【当前配置】
auto_tagging_enabled: {current_tagging}
category_rules: {json.dumps(current_rules, ensure_ascii=False)}

【任务】
请结合用户的长短期行为模式和当前配置，给出 KFS (智能文件分类) 模块的具体优化建议。

请仅返回 JSON 格式，不要包含其他文字，格式如下：
{{
  "reason": "你的优化理由",
  "auto_tagging_enabled": true,
  "category_rules": [
    {{
      "category": "文档",
      "extensions": [".txt", ".md", ".doc", ".docx"]
    }},
    {{
      "category": "代码",
      "extensions": [".py", ".js", ".c", ".cpp"]
    }}
  ]
}}

category_rules 中的 extensions 请根据用户实际访问的文件类型来设置，只包含用户实际使用过的扩展名。
"""
