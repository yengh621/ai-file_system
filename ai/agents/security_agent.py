#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Security Agent - 安全检测优化智能体
"""
import json
import os
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class SecurityAgent(BaseAgent):
    def __init__(self, config: Dict = None, memory_dir: str = "debug_memory"):
        super().__init__("Security", config)
        self.memory_dir = memory_dir
        self.learned_params_file = os.path.join(memory_dir, "agent", "memory", "long_term", "learned_params.json")
    
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
            delete_threshold = parsed_result.get("delete_threshold", current_params.get("suggested_delete_threshold", 5))
            modify_threshold = parsed_result.get("modify_threshold", current_params.get("suggested_modify_threshold", 10))
            reason = parsed_result.get("reason", "保持当前配置")
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": reason,
                "parameters": {
                    "delete_threshold": delete_threshold,
                    "modify_threshold": modify_threshold
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
    
    def _get_current_params(self, uid: int) -> Dict:
        """获取当前用户的最近参数"""
        if not os.path.exists(self.learned_params_file):
            return {"suggested_delete_threshold": 5, "suggested_modify_threshold": 10}
        
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                all_params = json.load(f)
            return all_params.get(str(uid), {"suggested_delete_threshold": 5, "suggested_modify_threshold": 10})
        except Exception as e:
            self.log(f"读取最近参数失败: {e}")
            return {"suggested_delete_threshold": 5, "suggested_modify_threshold": 10}
    
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
        current_delete = current_params.get("suggested_delete_threshold", 5)
        current_modify = current_params.get("suggested_modify_threshold", 10)
        
        return f"""你是一位安全检测专家。

【全局优化指导】
{guidance}

【最近行为统计】
操作总数: {recent_stats.get('total_ops', 0)}
操作统计: {json.dumps(recent_stats.get('operation_counts', {}), ensure_ascii=False)}
文件类型: {json.dumps(recent_stats.get('file_types', {}), ensure_ascii=False)}

【历史行为统计】
操作总数: {all_stats.get('total_ops', 0)}
操作统计: {json.dumps(all_stats.get('operation_counts', {}), ensure_ascii=False)}

【当前配置】
当前 delete_threshold: {current_delete}
当前 modify_threshold: {current_modify}

【任务】
请结合用户的长短期行为模式和当前配置，给出安全阈值的具体优化建议，并给出建议值（整数，范围 5-20）。

阈值建议：
- delete_threshold（每分钟删除操作阈值）：
  - 5-8：敏感环境，严格控制删除
  - 9-12：正常环境，平衡安全与便利
  - 13-20：宽松环境，较少警告

- modify_threshold（每分钟修改操作阈值）：
  - 5-10：敏感环境，严格控制修改
  - 11-18：正常环境，平衡安全与便利
  - 19-20：宽松环境，较少限制

请仅返回 JSON 格式，不要包含其他文字，格式如下：
{{
  "reason": "你的优化理由",
  "delete_threshold": 5,
  "modify_threshold": 10
}}
"""
