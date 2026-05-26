
# 🤖 Security Agent - 安全智能体

## 📋 概述

Security Agent 负责分析安全阈值优化策略，参照 Analyzer Agent 的行为模式，调用 GLM 分析并给出具体的删除/修改安全阈值参数。

---

## 📁 文件位置

`ai/agents/security_agent.py`

---

## 🎯 职责

1. 接收 Analyzer 的行为模式描述
2. 结合操作上下文调用 GLM 分析安全风险
3. 给出安全优化建议
4. 给出具体的安全阈值参数：
   - `delete_threshold` - 删除操作安全阈值
   - `modify_threshold` - 修改操作安全阈值

---

## 🔄 输入输出

### 输入数据

```python
behavior_pattern = "用户删除操作较少，主要修改文件内容，总体行为比较保守..."
context = {
    "recent_stats": {
        "operation_counts": {"delete": 5, "modify": 30}
    },
    ...
}
```

### 输出数据

```python
{
    "status": "success",
    "agent": "Security",
    "suggestion": "用户删除操作较少，可适当提高删除阈值以减少警告...",
    "parameters": {
        "delete_threshold": 10,
        "modify_threshold": 15
    }
}
```

---

## 📝 GLM 提示词

Security Agent 调用 GLM 时使用的提示词：

```
你是一个文件系统安全专家。

[全局优化指导]
{behavior_pattern}

[当前上下文]
操作统计：{op_counts}

请给出安全阈值的优化建议，并给出建议的参数值（整数）：
- delete_threshold: 5-20
- modify_threshold: 5-20

请按以下 JSON 格式返回：
{
  "reason": "你的理由",
  "delete_threshold": 10,
  "modify_threshold": 15
}
```

---

## 💡 安全阈值说明

### 删除阈值 (delete_threshold)

定义：单位时间内允许的删除操作次数上限

| 阈值 | 适用场景 | 效果 |
|------|---------|------|
| 5-8 | 敏感环境 | 严格控制删除，频繁警告 |
| 9-12 | 正常环境 | 平衡安全与便利 |
| 13-20 | 宽松环境 | 较少的安全警告 |

### 修改阈值 (modify_threshold)

定义：单位时间内允许的修改操作次数上限

| 阈值 | 适用场景 | 效果 |
|------|---------|------|
| 5-10 | 敏感环境 | 严格控制修改 |
| 11-18 | 正常环境 | 平衡安全与便利 |
| 19-20 | 宽松环境 | 较少限制 |

---

## 🔧 使用示例

```python
from ai.agents.security_agent import SecurityAgent

security = SecurityAgent()
result = security.process(behavior_pattern, context)

print(result["suggestion"])
print(f"删除阈值: {result['parameters']['delete_threshold']}")
print(f"修改阈值: {result['parameters']['modify_threshold']}")
```

---

## 🔗 相关文档

- [多智能体系统总览](../multi_agent_system.md)
- [Analyzer Agent](./analyzer_agent.md)
- [KFS Agent](./kfs_agent.md)
- [IO Agent](./io_agent.md)
