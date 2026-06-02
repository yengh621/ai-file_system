#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IO Agent - I/O 预取优化智能体
"""
import json
import os
from typing import Dict
from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class IOAgent(BaseAgent):
    def __init__(self, config: Dict = None, memory_dir: str = "debug_memory"):
        super().__init__("IO", config)
        self.memory_dir = memory_dir
        self.learned_params_file = os.path.join(memory_dir, "agent", "memory", "long_term", "learned_params.json")
        self.io_stats_file = os.path.join(memory_dir, "io_stats.json")
    
    def _load_io_stats(self) -> Dict:
        """加载内核导出的 IO 统计数据"""
        if not os.path.exists(self.io_stats_file):
            return {}
        
        try:
            with open(self.io_stats_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            self.log(f"读取 IO 统计失败: {e}")
            return {}
    
    def _load_current_params(self) -> Dict:
        """从 learned_params.json 加载当前参数"""
        if not os.path.exists(self.learned_params_file):
            return {"file_prefetch_windows": {}}
        
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data.get("parameters", {"file_prefetch_windows": {}})
        except Exception as e:
            self.log(f"读取当前参数失败: {e}")
            return {"file_prefetch_windows": {}}
    
    def process(self, guidance: str) -> Dict:
        """
        根据全局指导和 io_stats.json，调用 GLM 分析 IO 优化
        
        Args:
            guidance: Analyzer 给出的全局指导
        
        Returns:
            IO 优化方案，包含每个文件的 prefetch_window
        """
        self.log("根据全局指导和内核 IO 统计，分析 IO 优化...")
        
        # 获取当前参数和内核 IO 统计
        current_params = self._load_current_params()
        io_stats = self._load_io_stats()
        
        prompt = self._build_prompt(guidance, current_params, io_stats)
        
        try:
            glm_response = call_glm_api(prompt, config=self.config)
            self.log(f"GLM 响应: {glm_response}")
            
            # 解析 JSON 响应
            parsed_result = self._parse_json_response(glm_response)
            
            # 使用解析结果或回退到当前参数
            file_prefetch_windows = parsed_result.get("file_prefetch_windows", current_params.get("file_prefetch_windows", {}))
            reason = parsed_result.get("reason", "保持当前配置")
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": reason,
                "parameters": {
                    "file_prefetch_windows": file_prefetch_windows
                }
            }
        except Exception as e:
            self.log(f"IO 分析失败: {e}")
            return {
                "status": "error",
                "agent": self.name,
                "error": str(e),
                "suggestion": "保持当前 IO 配置",
                "parameters": {"file_prefetch_windows": {}}
            }
    
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
    
    def _build_prompt(self, guidance: str, current_params: Dict, io_stats: Dict) -> str:
        # 构建 IO 统计描述
        io_stats_desc = ""
        if io_stats:
            io_stats_desc = f"""【内核全局 IO 统计】
预取缓存命中: {io_stats.get('cache_hits', 0)}
预取缓存未命中: {io_stats.get('cache_misses', 0)}
总预取块数: {io_stats.get('total_prefetched_blocks', 0)}
顺序访问文件数: {io_stats.get('sequential_files', 0)}
随机访问文件数: {io_stats.get('random_files', 0)}
平均预取窗口: {io_stats.get('average_prefetch_window', 0)}
总读取操作: {io_stats.get('total_read_operations', 0)}
"""
            files = io_stats.get("files", [])
            if files:
                io_stats_desc += "\n【各文件 IO 统计】\n"
                for f in files:
                    io_stats_desc += f"  - inode {f.get('ino')}: type={f.get('type')}, reads={f.get('total_reads')}, current_window={f.get('current_prefetch_window')}\n"
        
        return f"""你是一位 I/O 优化专家。

【全局优化指导】
{guidance}

{io_stats_desc}

【任务】
请结合全局优化指导和内核实际的 IO 统计数据，为每个文件决定合适的 prefetch_window 值（整数，范围 1-10）。重点关注：
- 顺序访问的文件：使用较大的预取窗口（6-10）
- 随机访问的文件：使用较小的预取窗口（1-2）
- 不确定的文件：使用中等窗口（3-5）

请仅返回 JSON 格式，不要包含其他文字，格式如下：
{{
  "reason": "你的优化理由",
  "file_prefetch_windows": {{
    "123": 10,
    "456": 3,
    "789": 1
  }}
}}

其中 key 是文件的 inode（字符串格式），value 是该文件的 prefetch_window。
"""
