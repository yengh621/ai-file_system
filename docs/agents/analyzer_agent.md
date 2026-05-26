
# 🤖 Analyzer Agent - 行为模式分析

## 📋 概述

Analyzer Agent 是多智能体系统的入口，负责分析用户行为并用自然语言描述。它**不给出具体参数**，只负责理解和描述用户的行为模式。

---

## 📁 文件位置

`ai/agents/analyzer_agent.py`

---

## 🎯 职责

1. 接收用户操作上下文（短期记忆 + 长期记忆）
2. 调用 GLM 分析用户在以下方面的行为：
   - 文件访问模式
   - 修改频率与方式
   - 删除操作特征
3. 用自然语言描述行为模式
4. 将结果传递给其他 Agent

---

## 🔄 输入输出

### 输入数据

```python
context = {
    "uid": 0,
    "recent_stats": {
        "total_ops": 50,
        "operation_counts": {"create": 20, "write": 15, "delete": 5},
        "file_types": {".txt": 15, ".md": 8},
        "hours": [9, 10, 14],
        "days": [0, 1, 2, 3, 4]
    },
    "all_stats": {
        /* 长期统计数据 */
    },
    "recent_ops": [/* 最近操作 */],
    "historical_ops": [/* 历史操作 */]
}
```

### 输出数据

```python
{
    "status": "success",
    "agent": "Analyzer",
    "behavior_pattern": "用户主要在工作日白天操作，以文本文件为主，修改频率中等，删除操作较少..."
}
```

---

## 📝 GLM 提示词

Analyzer 调用 GLM 时使用的提示词：

```
你是一个专业的用户行为分析专家。请分析以下用户文件系统操作数据，用文字描述用户在以下三个方面的行为模式（各50字左右）：

1. 文件访问行为 - 用户主要访问哪些类型的文件？读写频率如何？
2. 修改行为 - 用户修改文件的频率和幅度如何？
3. 删除行为 - 用户删除操作的频率和特征如何？

[最近行为统计]
[历史行为统计]

请只用自然语言描述，不要给出任何数字参数或 JSON 格式。
```

---

## 🔧 使用示例

```python
from ai.agents.analyzer_agent import AnalyzerAgent
from ai.memory.recorder import MemoryRecorder

recorder = MemoryRecorder()
recorder.set_user(0)

# 记录一些操作
recorder.record_operation("create", "test.txt")
recorder.record_operation("write", "test.txt")
recorder.record_operation("create", "notes.md")

# 获取上下文
context = recorder.get_context()

# 分析
analyzer = AnalyzerAgent()
result = analyzer.process(context)

print(result["behavior_pattern"])
```

---

## 📊 输出示例

**Analyzer 的行为模式描述：**
```
文件访问行为：用户主要访问 .txt 和 .md 文本文件，读操作较多，写操作相对较少，
主要在上午和下午工作时间活跃。

修改行为：用户修改文件频率中等，通常在创建文件后会有 2-3 次修改，然后稳定下来。

删除行为：用户删除操作较少，平均每周 2-3 次，主要删除临时文件。
```

---

## 🔗 相关文档

- [多智能体系统总览](../multi_agent_system.md)
- [KFS Agent](./kfs_agent.md)
- [IO Agent](./io_agent.md)
- [Security Agent](./security_agent.md)
