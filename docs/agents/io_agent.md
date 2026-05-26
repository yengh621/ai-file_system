
# 🤖 IO Agent - I/O 优化智能体

## 📋 概述

IO Agent 负责分析 I/O 预取策略，并给出具体的优化参数。它参照 Analyzer Agent 的行为模式，调用 GLM 分析并给出最佳的预取窗口设置。

---

## 📁 文件位置

`ai/agents/io_agent.py`

---

## 🎯 职责

1. 接收 Analyzer 的行为模式描述
2. 结合操作上下文调用 GLM 分析 I/O 模式
3. 给出预取策略优化建议
4. 给出具体的预取窗口参数（整数，1-10）

---

## 🔄 输入输出

### 输入数据

```python
behavior_pattern = "用户主要在工作日白天操作，以文本文件为主，顺序读写较多..."
context = {
    "recent_stats": {
        "total_ops": 50,
        "operation_counts": {"read": 30, "write": 20}
    },
    ...
}
```

### 输出数据

```python
{
    "status": "success",
    "agent": "IO",
    "suggestion": "用户主要进行顺序读写，建议增大预取窗口以提高性能...",
    "parameters": {
        "prefetch_window": 5
    }
}
```

---

## 📝 GLM 提示词

IO Agent 调用 GLM 时使用的提示词：

```
你是一个 I/O 优化专家。

[全局优化指导]
{behavior_pattern}

[当前上下文]
操作统计：{op_counts}

请给出 I/O 预取策略的具体优化建议，并给出建议的 prefetch_window 值（整数，范围 1-10）。

请按以下 JSON 格式返回：
{
  "reason": "你的理由",
  "prefetch_window": 5
}
```

---

## 💡 预取策略说明

### 预取窗口 (prefetch_window)

定义：一次预取操作读取多少个数据块

| 窗口大小 | 适用场景 | 效果 |
|---------|---------|------|
| 1-2 | 随机访问较多 | 减少不必要的预取 |
| 3-5 | 混合模式 | 平衡性能与空间 |
| 6-10 | 顺序访问较多 | 提高顺序读写性能 |

### 工作原理

```
用户请求读取块 X
   ↓
系统预取 X, X+1, X+2...X+N（N=预取窗口）
   ↓
如果用户继续顺序读取，缓存命中！
```

---

## 🔧 使用示例

```python
from ai.agents.io_agent import IOAgent

io_agent = IOAgent()
result = io_agent.process(behavior_pattern, context)

print(result["suggestion"])
print(f"建议预取窗口: {result['parameters']['prefetch_window']}")
```

---

## 🔗 相关文档

- [多智能体系统总览](../multi_agent_system.md)
- [Analyzer Agent](./analyzer_agent.md)
- [KFS Agent](./kfs_agent.md)
- [Security Agent](./security_agent.md)
