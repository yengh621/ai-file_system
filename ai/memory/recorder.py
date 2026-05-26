#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Recorder Agent - 记忆记录模块

短时记忆：
- 保存到 debug_memory/short_term/operations.json
- 保存最近 100 条操作
- 超过 48 小时自动清空

长时记忆：
- 保存到 debug_memory/agent/memory/long_term/all_operations.json
- 所有历史操作记录
- 学习参数保存到 debug_memory/agent/memory/long_term/learned_params.json
"""
import json
import os
from datetime import datetime, timedelta
from typing import Dict, List


class MemoryRecorder:
    def __init__(self, memory_dir="debug_memory"):
        self.memory_dir = memory_dir
        
        # 短时记忆目录和文件
        self.short_term_dir = os.path.join(self.memory_dir, "short_term")
        self.short_term_file = os.path.join(self.short_term_dir, "operations.json")
        
        # 长时记忆目录
        self.long_term_dir = os.path.join(self.memory_dir, "agent", "memory", "long_term")
        self.long_term_file = os.path.join(self.long_term_dir, "all_operations.json")
        
        # 学习参数文件
        self.learned_params_file = os.path.join(self.long_term_dir, "learned_params.json")
        
        self.current_uid = -1
        
        # 确保目录存在
        os.makedirs(self.short_term_dir, exist_ok=True)
        os.makedirs(self.long_term_dir, exist_ok=True)
        
        # 加载数据
        self.short_term_ops = self._load_short_term()
        self.long_term_ops = self._load_long_term()
        self.learned_params = self._load_learned_params()
    
    def _load_short_term(self) -> List[Dict]:
        """加载短时记忆，检查是否需要清空"""
        if not os.path.exists(self.short_term_file):
            return []
        
        try:
            with open(self.short_term_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            
            if not isinstance(data, list) or len(data) == 0:
                return []
            
            # 检查第一条记录的时间，如果超过 48 小时则清空
            first_record = data[0]
            if "timestamp" in first_record:
                try:
                    record_time = datetime.fromisoformat(first_record["timestamp"])
                    if datetime.now() - record_time > timedelta(hours=48):
                        print("[Recorder] 短时记忆超过 48 小时，已清空")
                        return []
                except:
                    pass
            
            # 确保不超过 100 条
            if len(data) > 100:
                return data[-100:]
            
            return data
        except Exception as e:
            print(f"[Recorder] 加载短时记忆失败: {e}")
            return []
    
    def _save_short_term(self):
        """保存短时记忆"""
        try:
            with open(self.short_term_file, "w", encoding="utf-8") as f:
                json.dump(self.short_term_ops, f, ensure_ascii=False, indent=2)
        except Exception as e:
            print(f"[Recorder] 保存短时记忆失败: {e}")
    
    def _load_long_term(self) -> List[Dict]:
        """加载长时记忆"""
        if not os.path.exists(self.long_term_file):
            return []
        
        try:
            with open(self.long_term_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, list):
                return data
        except Exception as e:
            print(f"[Recorder] 加载长时记忆失败: {e}")
        
        return []
    
    def _save_long_term(self):
        """保存长时记忆（最多保存 10000 条）"""
        try:
            with open(self.long_term_file, "w", encoding="utf-8") as f:
                json.dump(self.long_term_ops[-10000:], f, ensure_ascii=False, indent=2)
        except Exception as e:
            print(f"[Recorder] 保存长时记忆失败: {e}")
    
    def _load_learned_params(self) -> Dict:
        """加载学习参数"""
        if not os.path.exists(self.learned_params_file):
            return {}
        
        try:
            with open(self.learned_params_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"[Recorder] 加载学习参数失败: {e}")
            return {}
    
    def _save_learned_params(self):
        """保存学习参数"""
        try:
            with open(self.learned_params_file, "w", encoding="utf-8") as f:
                json.dump(self.learned_params, f, ensure_ascii=False, indent=2)
        except Exception as e:
            print(f"[Recorder] 保存学习参数失败: {e}")
    
    def set_user(self, uid: int) -> Dict:
        """设置当前用户"""
        self.current_uid = uid
        return {
            "status": "success",
            "uid": uid,
            "message": f"用户 {uid} 已激活，开始记录操作"
        }
    
    def clear_user(self) -> Dict:
        """清除当前用户"""
        old_uid = self.current_uid
        self.current_uid = -1
        return {
            "status": "success",
            "message": f"用户 {old_uid} 已登出"
        }
    
    def record_operation(self, operation: str, path: str = None) -> Dict:
        """记录操作（同时写到短时和长时记忆）"""
        if self.current_uid == -1:
            return {"status": "error", "message": "无当前用户，请先设置用户"}
        
        record = {
            "uid": self.current_uid,
            "operation": operation,
            "path": path,
            "timestamp": datetime.now().isoformat(),
            "hour": datetime.now().hour,
            "day_of_week": datetime.now().weekday()
        }
        
        # 添加到短时记忆
        self.short_term_ops.append(record)
        
        # 检查短时记忆是否超过 100 条
        if len(self.short_term_ops) > 100:
            self.short_term_ops = self.short_term_ops[-100:]
            print("[Recorder] 短时记忆超过 100 条，已清理旧记录")
        
        self._save_short_term()
        
        # 添加到长时记忆
        self.long_term_ops.append(record)
        self._save_long_term()
        
        return {"status": "success", "record": record}
    
    def get_context(self) -> Dict:
        """获取分析上下文"""
        if self.current_uid == -1:
            return {"status": "error", "message": "无当前用户，请先设置用户"}
        
        uid = self.current_uid
        
        # 当前用户的短时操作
        short_ops = [op for op in self.short_term_ops if op["uid"] == uid]
        
        # 当前用户的长时操作
        long_ops = [op for op in self.long_term_ops if op["uid"] == uid]
        
        # 计算统计
        short_stats = self._calculate_stats(short_ops)
        long_stats = self._calculate_stats(long_ops)
        
        return {
            "status": "success",
            "uid": uid,
            "recent_stats": short_stats,
            "all_stats": long_stats,
            "recent_ops": short_ops[-20:],
            "historical_ops": long_ops[-30:]
        }
    
    def _calculate_stats(self, operations: List[Dict]) -> Dict:
        """从操作列表计算统计数据"""
        if not operations:
            return {
                "total_ops": 0,
                "operation_counts": {},
                "file_types": {},
                "hours": [],
                "days": []
            }
        
        op_counts = {}
        file_types = {}
        hours = set()
        days = set()
        recent_files = set()
        
        for op in operations:
            op_type = op["operation"]
            op_counts[op_type] = op_counts.get(op_type, 0) + 1
            
            path = op.get("path")
            if path:
                ext = os.path.splitext(path)[1].lower() or os.path.basename(path)
                file_types[ext] = file_types.get(ext, 0) + 1
                recent_files.add(path)
            
            hours.add(op.get("hour"))
            days.add(op.get("day_of_week"))
        
        return {
            "total_ops": len(operations),
            "operation_counts": op_counts,
            "file_types": file_types,
            "hours": sorted(list(hours)),
            "days": sorted(list(days)),
            "recent_files": list(recent_files)[:10]
        }
    
    def save_learned_params(self, uid: int, params: Dict) -> Dict:
        """保存学习到的参数"""
        self.learned_params.setdefault(str(uid), {}).update(params)
        self.learned_params[str(uid)]["last_updated"] = datetime.now().isoformat()
        self._save_learned_params()
        return {"status": "success", "message": "参数已保存"}
    
    def get_learned_params(self, uid: int) -> Dict:
        """获取学习到的参数"""
        return self.learned_params.get(str(uid), {
            "suggested_prefetch_window": 3,
            "suggested_delete_threshold": 5,
            "suggested_modify_threshold": 10
        })
