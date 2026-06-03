#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Security Agent - learns anomaly thresholds for destructive/modify operations."""
import json
import os
import re
from typing import Dict

from .base_agent import BaseAgent
from ..glm_client import call_glm_api


class SecurityAgent(BaseAgent):
    def __init__(self, config: Dict = None, memory_dir: str = "debug_memory"):
        super().__init__("Security", config)
        self.memory_dir = memory_dir
        self.learned_params_file = os.path.join(
            memory_dir, "agent", "memory", "long_term", "learned_params.json"
        )

    def process(self, guidance: str, context: Dict) -> Dict:
        current_params = self._get_current_params(context.get("uid", -1))
        prompt = self._build_prompt(guidance, context, current_params)

        try:
            glm_response = call_glm_api(prompt, config=self.config)
            self.log(f"GLM response: {glm_response}")
            parsed = self._parse_json_response(glm_response)
            fallback = self._build_local_recommendation(context, current_params)
            parameters = {
                "delete_threshold": self._clamp_threshold(
                    parsed.get("delete_threshold", fallback["parameters"]["delete_threshold"])
                ),
                "modify_threshold": self._clamp_threshold(
                    parsed.get("modify_threshold", fallback["parameters"]["modify_threshold"])
                ),
            }
            return {
                "status": "success",
                "agent": self.name,
                "suggestion": parsed.get("reason", fallback["reason"]),
                "parameters": parameters,
            }
        except Exception as exc:
            self.log(f"Security analysis failed: {exc}")
            fallback = self._build_local_recommendation(context, current_params)
            return {
                "status": "success",
                "agent": self.name,
                "error": str(exc),
                "suggestion": fallback["reason"],
                "parameters": fallback["parameters"],
            }

    def _get_current_params(self, uid: int) -> Dict:
        if not os.path.exists(self.learned_params_file):
            return {"suggested_delete_threshold": 5, "suggested_modify_threshold": 10}

        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            params = data.get("parameters", {}) if isinstance(data, dict) else {}
            return {
                "suggested_delete_threshold": params.get("delete_threshold", 5),
                "suggested_modify_threshold": params.get("modify_threshold", 10),
            }
        except Exception as exc:
            self.log(f"Failed to read current security params: {exc}")
            return {"suggested_delete_threshold": 5, "suggested_modify_threshold": 10}

    def _build_local_recommendation(self, context: Dict, current_params: Dict) -> Dict:
        """GLM fallback based on recent and long-term operation counts."""
        recent_counts = context.get("recent_stats", {}).get("operation_counts", {}) or {}
        all_counts = context.get("all_stats", {}).get("operation_counts", {}) or {}

        delete_count = int(recent_counts.get("delete", 0))
        modify_count = self._modify_count(recent_counts)
        historical_delete = int(all_counts.get("delete", 0))
        historical_modify = self._modify_count(all_counts)

        delete_threshold = self._choose_threshold(
            recent_count=delete_count,
            historical_count=historical_delete,
            current_value=int(current_params.get("suggested_delete_threshold", 5)),
            sensitive_default=5,
            normal_default=10,
            relaxed_default=12,
        )
        modify_threshold = self._choose_threshold(
            recent_count=modify_count,
            historical_count=historical_modify,
            current_value=int(current_params.get("suggested_modify_threshold", 10)),
            sensitive_default=8,
            normal_default=12,
            relaxed_default=16,
        )

        return {
            "reason": (
                "GLM unavailable; generated local security thresholds from operation stats "
                f"(recent deletes={delete_count}, recent modifies={modify_count})."
            ),
            "parameters": {
                "delete_threshold": delete_threshold,
                "modify_threshold": modify_threshold,
            },
        }

    def _choose_threshold(
        self,
        recent_count: int,
        historical_count: int,
        current_value: int,
        sensitive_default: int,
        normal_default: int,
        relaxed_default: int,
    ) -> int:
        if recent_count > 0:
            target = min(sensitive_default, max(5, recent_count))
        elif historical_count >= normal_default:
            target = normal_default
        elif historical_count > 0:
            target = current_value or normal_default
        else:
            target = relaxed_default
        return self._clamp_threshold(target)

    def _modify_count(self, counts: Dict) -> int:
        return (
            int(counts.get("write", 0))
            + int(counts.get("chmod", 0))
            + int(counts.get("create", 0))
        )

    def _clamp_threshold(self, value) -> int:
        try:
            return max(5, min(20, int(value)))
        except Exception:
            return 10

    def _parse_json_response(self, response: str) -> Dict:
        try:
            return json.loads(response)
        except Exception:
            pass

        try:
            json_match = re.search(r"\{[\s\S]*\}", response)
            if json_match:
                return json.loads(json_match.group(0))
        except Exception as exc:
            self.log(f"Failed to parse Security JSON: {exc}")

        return {}

    def _build_prompt(self, guidance: str, context: Dict, current_params: Dict) -> str:
        recent_stats = context.get("recent_stats", {})
        all_stats = context.get("all_stats", {})
        current_delete = current_params.get("suggested_delete_threshold", 5)
        current_modify = current_params.get("suggested_modify_threshold", 10)

        return f"""You are a filesystem security detection expert.

Global guidance:
{guidance}

Recent behavior stats:
- total operations: {recent_stats.get('total_ops', 0)}
- operation counts: {json.dumps(recent_stats.get('operation_counts', {}), ensure_ascii=False)}
- file types: {json.dumps(recent_stats.get('file_types', {}), ensure_ascii=False)}

Historical behavior stats:
- total operations: {all_stats.get('total_ops', 0)}
- operation counts: {json.dumps(all_stats.get('operation_counts', {}), ensure_ascii=False)}

Current configuration:
- delete_threshold: {current_delete}
- modify_threshold: {current_modify}

Recommend integer anomaly thresholds in range 5-20.
- delete_threshold controls delete operations per minute.
- modify_threshold controls create/write/chmod operations per minute.

Return JSON only:
{{
  "reason": "short reason",
  "delete_threshold": 5,
  "modify_threshold": 10
}}
"""
