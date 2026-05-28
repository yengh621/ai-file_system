
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI 集成模块 - 与多智能体系统交互
"""
import os
import json
from .styles import Filesystem


def get_used_space():
    """获取已用空间（估算）"""
    try:
        record_path = os.path.join("debug_memory", "agent", "memory", "long_term", "all_operations.json")
        if os.path.exists(record_path):
            with open(record_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                return min(Filesystem.TOTAL_BYTES, len(data) * 500)
        return 32768  # 默认 32KB
    except Exception:
        return 32768


def get_last_analysis():
    """获取最后一次分析结果"""
    try:
        path = os.path.join("debug_memory", "agent", "memory", "long_term", "last_analysis.json")
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
    except Exception:
        pass
    return None


def format_analysis(data):
    """格式化分析结果为文本"""
    if not data:
        return "暂无分析结果"

    params = data.get('parameters', {})
    return f"""📊 AI 优化建议

👤 用户: {data.get('uid', 'N/A')}
⏰ 时间: {data.get('timestamp', 'N/A')}

📝 行为模式:
{data.get('behavior_pattern', '暂无数据')}

📁 KFS 建议:
{data.get('kfs_suggestion', '暂无数据')}

⚡ IO 建议:
{data.get('io_suggestion', '暂无数据')}

🔒 Security 建议:
{data.get('security_suggestion', '暂无数据')}

⚙️ 建议参数:
- 预取窗口: {params.get('prefetch_window', 3)}
- 删除阈值: {params.get('delete_threshold', 8)}
- 修改阈值: {params.get('modify_threshold', 12)}
"""
