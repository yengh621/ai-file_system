#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KFS Agent - 热点文件优化智能体
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
        self.kfs_stats_file = os.path.join(memory_dir, "kfs_stats.json")
    
    def _load_kfs_stats(self) -> Dict:
        """加载内核导出的 KFS 热点数据"""
        for path in (self.kfs_stats_file, os.path.join(self.memory_dir, "kfs_stats.json")):
            if not os.path.exists(path):
                continue

            try:
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception as e:
                self.log(f"读取 KFS 统计失败: {e}")
        return {}
    
    def _load_current_params(self) -> Dict:
        """从 learned_params.json 加载当前参数"""
        if not os.path.exists(self.learned_params_file):
            return {"hot_files": []}
        
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data.get("parameters", {"hot_files": []})
        except Exception as e:
            self.log(f"读取当前参数失败: {e}")
            return {"hot_files": []}
    
    def process(self, guidance: str) -> Dict:
        """
        根据全局指导和 kfs_stats.json，调用 GLM 分析热点文件优化
        
        Args:
            guidance: Analyzer 给出的全局指导
        
        Returns:
            KFS 优化方案
        """
        self.log("根据全局指导和内核 KFS 统计，分析热点文件优化...")
        
        # 获取当前参数和内核 KFS 统计
        current_params = self._load_current_params()
        kfs_stats = self._load_kfs_stats()
        selected_hot_files = self._select_hot_files(kfs_stats, current_params)
        
        prompt = self._build_prompt(guidance, current_params, kfs_stats)
        
        try:
            glm_response = call_glm_api(prompt, config=self.config)
            self.log(f"GLM 响应: {glm_response}")
            
            # 解析 JSON 响应
            parsed_result = self._parse_json_response(glm_response)
            
            # 使用有效的模型结果；如果模型漏掉 ino，则从内核统计按文件名补回。
            parsed_hot_files = self._normalize_hot_files(parsed_result.get("hot_files"), kfs_stats)
            hot_files = parsed_hot_files or selected_hot_files
            reason = parsed_result.get("reason", "保持当前配置")
            
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": reason,
                "parameters": {
                    "hot_files": hot_files
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
                    "hot_files": selected_hot_files
                }
            }

    def _select_hot_files(self, kfs_stats: Dict, current_params: Dict) -> list:
        """Pick hot files locally when GLM is unavailable or returns no list."""
        hot_files = kfs_stats.get("hot_files", [])
        if not hot_files:
            return self._normalize_hot_files(current_params.get("hot_files", []), kfs_stats)

        candidates = []
        for item in hot_files:
            if not isinstance(item, dict):
                continue
            filename = item.get("filename")
            ino = item.get("ino")
            if not filename or not ino:
                continue
            score = float(item.get("short_term_score", 0) or 0) + float(item.get("long_term_score", 0) or 0)
            access_count = int(item.get("access_count", 0) or 0)
            last_access = int(item.get("last_access", 0) or 0)
            if item.get("stored_in_kfs") or access_count >= 1 or score > 0:
                candidates.append((score, access_count, last_access, {"filename": filename, "ino": int(ino)}))

        candidates.sort(key=lambda entry: (entry[0], entry[1], entry[2]), reverse=True)
        return [entry[3] for entry in candidates[:4]]

    def _normalize_hot_files(self, hot_files, kfs_stats: Dict) -> list:
        """Return only KFS-storable entries and fill missing inode by filename."""
        if not isinstance(hot_files, list):
            return []

        ino_by_name = {}
        for item in kfs_stats.get("hot_files", []):
            if not isinstance(item, dict):
                continue
            filename = item.get("filename")
            ino = item.get("ino")
            if not filename or not ino:
                continue
            try:
                ino_by_name[str(filename)] = int(ino)
            except (TypeError, ValueError):
                continue

        normalized = []
        seen = set()
        for item in hot_files:
            if not isinstance(item, dict):
                continue
            filename = item.get("filename") or item.get("name") or item.get("path")
            if not filename:
                continue

            try:
                ino = int(item.get("ino") or 0)
            except (TypeError, ValueError):
                ino = 0
            if ino <= 0:
                ino = ino_by_name.get(str(filename), 0)
            if ino <= 0:
                continue

            key = (str(filename), ino)
            if key in seen:
                continue
            seen.add(key)
            normalized.append({"filename": str(filename), "ino": ino})

        return normalized
    
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
    
    def _build_prompt(self, guidance: str, current_params: Dict, kfs_stats: Dict) -> str:
        # 构建 KFS 热点文件描述
        kfs_desc = ""
        hot_files = kfs_stats.get("hot_files", [])
        if hot_files:
            kfs_desc = "【内核 KFS 热点文件数据】\n"
            for f in hot_files:
                kfs_desc += f"  - {f.get('filename')}: 访问={f.get('access_count')}, 短期分={f.get('short_term_score')}, 长期分={f.get('long_term_score')}, KFS存储={'是' if f.get('stored_in_kfs') else '否'}\n"
        
        return f"""你是一位热点文件优化专家。

【全局优化指导】
{guidance}

{kfs_desc}

【任务】
请结合全局优化指导和内核 KFS 热点文件数据，分析热点文件的访问规律和长短期热度，给出 KFS 热点文件管理的优化建议。
重点关注：
- 短期评分高的文件（最近频繁访问）
- 长期评分高的文件（长期活跃）
- 已存储在 KFS 中的文件是否合理

请从内核 KFS 热点文件数据中，根据短期分和长期分，提取出应该存储在 KFS 中的热点文件列表（hot_files），每个文件包含 filename、ino。

请仅返回 JSON 格式，不要包含其他文字，格式如下：
{{
  "reason": "你的优化理由",
  "hot_files": [
    {{"filename": "example.txt", "ino": 123}}
  ]
}}

"""
