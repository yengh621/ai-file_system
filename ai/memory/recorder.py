#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Per-user memory recorder.

Each user owns an isolated memory tree:
  debug_memory/users/<uid>/short_term/operations.json
  debug_memory/users/<uid>/agent/memory/long_term/all_operations.json
  debug_memory/users/<uid>/agent/memory/long_term/learned_params.json

The active user is persisted in debug_memory/current_user.json so CLI calls from
the C backend can share the same user context across separate Python processes.
"""
import json
import os
from datetime import datetime, timedelta
from typing import Dict, List


class MemoryRecorder:
    def __init__(self, memory_dir="debug_memory"):
        self.memory_dir = memory_dir
        self.users_dir = os.path.join(self.memory_dir, "users")
        self.active_user_file = os.path.join(self.memory_dir, "current_user.json")
        self.current_uid = self._load_active_user()

        os.makedirs(self.users_dir, exist_ok=True)
        self._configure_paths(self.current_uid)
        self._load_all()

    def _configure_paths(self, uid: int):
        uid_name = str(uid) if uid != -1 else "_anonymous"
        self.user_dir = os.path.join(self.users_dir, uid_name)
        self.short_term_dir = os.path.join(self.user_dir, "short_term")
        self.short_term_file = os.path.join(self.short_term_dir, "operations.json")
        self.long_term_dir = os.path.join(self.user_dir, "agent", "memory", "long_term")
        self.long_term_file = os.path.join(self.long_term_dir, "all_operations.json")
        self.learned_params_file = os.path.join(self.long_term_dir, "learned_params.json")
        self.agent_calls_file = os.path.join(self.user_dir, "agent_calls.json")

        os.makedirs(self.short_term_dir, exist_ok=True)
        os.makedirs(self.long_term_dir, exist_ok=True)

    def _load_all(self):
        self.short_term_ops = self._load_short_term()
        self.long_term_ops = self._load_long_term()
        self.learned_params = self._load_learned_params()

    def _load_active_user(self) -> int:
        if not os.path.exists(self.active_user_file):
            return -1
        try:
            with open(self.active_user_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return int(data.get("uid", -1))
        except Exception:
            return -1

    def _save_active_user(self):
        os.makedirs(self.memory_dir, exist_ok=True)
        with open(self.active_user_file, "w", encoding="utf-8") as f:
            json.dump(
                {"uid": self.current_uid, "updated_at": datetime.now().isoformat()},
                f,
                ensure_ascii=False,
                indent=2,
            )

    def _load_json_list(self, path: str) -> List[Dict]:
        if not os.path.exists(path):
            return []
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, list) else []
        except Exception as e:
            print(f"[Recorder] failed to load list {path}: {e}")
            return []

    def _load_short_term(self) -> List[Dict]:
        data = self._load_json_list(self.short_term_file)
        if not data:
            return []

        first_record = data[0]
        if "timestamp" in first_record:
            try:
                record_time = datetime.fromisoformat(first_record["timestamp"])
                if datetime.now() - record_time > timedelta(hours=48):
                    return []
            except Exception:
                pass
        return data[-100:]

    def _save_short_term(self):
        with open(self.short_term_file, "w", encoding="utf-8") as f:
            json.dump(self.short_term_ops[-100:], f, ensure_ascii=False, indent=2)

    def _load_long_term(self) -> List[Dict]:
        return self._load_json_list(self.long_term_file)

    def _save_long_term(self):
        with open(self.long_term_file, "w", encoding="utf-8") as f:
            json.dump(self.long_term_ops[-10000:], f, ensure_ascii=False, indent=2)

    def _load_learned_params(self) -> Dict:
        if not os.path.exists(self.learned_params_file):
            return self._get_default_params()
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, dict) and "parameters" in data else self._get_default_params()
        except Exception as e:
            print(f"[Recorder] failed to load learned params: {e}")
            return self._get_default_params()

    def _get_default_params(self) -> Dict:
        return {
            "uid": self.current_uid,
            "timestamp": datetime.now().isoformat(),
            "behavior_pattern": "",
            "kfs_suggestion": "",
            "io_suggestion": "",
            "security_suggestion": "",
            "parameters": {
                "file_prefetch_windows": {},
                "delete_threshold": 5,
                "modify_threshold": 10,
                "auto_tagging_enabled": True,
                "category_rules": [],
                "hot_files": [],
            },
        }

    def _save_learned_params(self):
        with open(self.learned_params_file, "w", encoding="utf-8") as f:
            json.dump(self.learned_params, f, ensure_ascii=False, indent=2)

    def _migrate_legacy_user_data(self, uid: int):
        legacy_short = os.path.join(self.memory_dir, "short_term", "operations.json")
        legacy_long = os.path.join(self.memory_dir, "agent", "memory", "long_term", "all_operations.json")
        legacy_params = os.path.join(self.memory_dir, "agent", "memory", "long_term", "learned_params.json")

        if not os.path.exists(self.short_term_file):
            records = [op for op in self._load_json_list(legacy_short) if op.get("uid") == uid]
            if records:
                with open(self.short_term_file, "w", encoding="utf-8") as f:
                    json.dump(records[-100:], f, ensure_ascii=False, indent=2)

        if not os.path.exists(self.long_term_file):
            records = [op for op in self._load_json_list(legacy_long) if op.get("uid") == uid]
            if records:
                with open(self.long_term_file, "w", encoding="utf-8") as f:
                    json.dump(records[-10000:], f, ensure_ascii=False, indent=2)

        if not os.path.exists(self.learned_params_file) and os.path.exists(legacy_params):
            try:
                with open(legacy_params, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if isinstance(data, dict) and data.get("uid", uid) in {uid, -1}:
                    data["uid"] = uid
                    with open(self.learned_params_file, "w", encoding="utf-8") as out:
                        json.dump(data, out, ensure_ascii=False, indent=2)
            except Exception:
                pass

    def set_user(self, uid: int) -> Dict:
        self.current_uid = int(uid)
        self._configure_paths(self.current_uid)
        self._migrate_legacy_user_data(self.current_uid)
        self._load_all()
        self._save_active_user()
        return {
            "status": "success",
            "uid": self.current_uid,
            "memory_dir": self.user_dir,
            "message": f"user {self.current_uid} activated",
        }

    def clear_user(self) -> Dict:
        old_uid = self.current_uid
        self.current_uid = -1
        self._configure_paths(self.current_uid)
        self._load_all()
        self._save_active_user()
        return {"status": "success", "message": f"user {old_uid} logged out"}

    def record_operation(self, operation: str, path: str = None) -> Dict:
        if self.current_uid == -1:
            return {"status": "error", "message": "no active user"}

        now = datetime.now()
        record = {
            "uid": self.current_uid,
            "operation": operation,
            "path": path,
            "timestamp": now.isoformat(),
            "hour": now.hour,
            "day_of_week": now.weekday(),
        }

        self.short_term_ops.append(record)
        self.short_term_ops = self.short_term_ops[-100:]
        self._save_short_term()

        self.long_term_ops.append(record)
        self.long_term_ops = self.long_term_ops[-10000:]
        self._save_long_term()

        return {"status": "success", "record": record}

    def get_context(self) -> Dict:
        if self.current_uid == -1:
            return {"status": "error", "message": "no active user"}

        short_stats = self._calculate_stats(self.short_term_ops)
        long_stats = self._calculate_stats(self.long_term_ops)
        return {
            "status": "success",
            "uid": self.current_uid,
            "memory_dir": self.user_dir,
            "recent_stats": short_stats,
            "all_stats": long_stats,
            "recent_ops": self.short_term_ops[-20:],
            "historical_ops": self.long_term_ops[-30:],
        }

    def _calculate_stats(self, operations: List[Dict]) -> Dict:
        if not operations:
            return {
                "total_ops": 0,
                "operation_counts": {},
                "file_types": {},
                "hours": [],
                "days": [],
            }

        op_counts = {}
        file_types = {}
        hours = set()
        days = set()
        recent_files = set()

        for op in operations:
            op_type = op.get("operation", "")
            op_counts[op_type] = op_counts.get(op_type, 0) + 1

            path = op.get("path")
            if path:
                ext = os.path.splitext(path)[1].lower() or os.path.basename(path)
                file_types[ext] = file_types.get(ext, 0) + 1
                recent_files.add(path)

            if op.get("hour") is not None:
                hours.add(op.get("hour"))
            if op.get("day_of_week") is not None:
                days.add(op.get("day_of_week"))

        return {
            "total_ops": len(operations),
            "operation_counts": op_counts,
            "file_types": file_types,
            "hours": sorted(hours),
            "days": sorted(days),
            "recent_files": list(recent_files)[:10],
        }

    def save_full_analysis(self, analysis_data: Dict) -> Dict:
        if self.current_uid != -1:
            analysis_data["uid"] = self.current_uid
        self.learned_params = analysis_data
        self.learned_params["last_updated"] = datetime.now().isoformat()
        self._save_learned_params()
        return {"status": "success", "message": "analysis saved", "path": self.learned_params_file}

    def get_current_params(self) -> Dict:
        return self.learned_params.get("parameters", self._get_default_params()["parameters"])
