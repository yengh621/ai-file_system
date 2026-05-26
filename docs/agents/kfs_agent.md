
# 🤖 KFS Agent - 智能文件分类

## 📋 概述

KFS (Knowledge-based File System) Agent 负责分析文件分类优化建议，参照 Analyzer Agent 给出的行为模式，为用户提供智能文件分类和组织建议。

---

## 📁 文件位置

`ai/agents/kfs_agent.py`

---

## 🎯 职责

1. 接收 Analyzer 的行为模式描述
2. 结合操作上下文，调用 GLM 分析文件分类优化
3. 给出文件分类、组织建议

---

## 🔄 输入输出

### 输入数据

```python
behavior_pattern = "用户主要在工作日白天操作，以文本文件为主，修改频率中等，删除操作较少..."
context = {
    "recent_stats": { ... },
    "all_stats": { ... },
    "recent_ops": [ ... ],
    "historical_ops": [ ... ]
}
```

### 输出数据

```python
{
    "status": "success",
    "agent": "KFS",
    "suggestion": "建议建立 /documents/ 目录存放文本文件，按日期组织，自动归档超过30天的文件..."
}
```

---

## 📝 GLM 提示词

KFS Agent 调用 GLM 时使用的提示词：

```
你是一个文件系统组织专家。

[全局优化指导]
{behavior_pattern}

[当前上下文]
操作统计：最近用户创建了 {op_counts} 个文件，文件类型分布：{file_types}

请给出文件分类与组织的优化建议，用自然语言描述。
```

---

## 💡 优化建议示例

**KFS Agent 可能给出的建议：**

1. **目录组织**
   - 建立 `/docs/` 目录存放文档文件
   - 建立 `/code/` 目录存放代码文件
   - 建立 `/temp/` 目录存放临时文件

2. **文件分类**
   - 自动识别文本文件、图片、代码
   - 根据文件扩展名自动归类

3. **归档策略**
   - 建议归档超过 30 天未访问的文件
   - 建议将大文件单独存放

---

## 🔧 使用示例

```python
from ai.agents.kfs_agent import KFSAgent

kfs = KFSAgent()
result = kfs.process(behavior_pattern, context)

print(result["suggestion"])
```

---

## 🔗 相关文档

- [多智能体系统总览](../multi_agent_system.md)
- [Analyzer Agent](./analyzer_agent.md)
- [IO Agent](./io_agent.md)
- [Security Agent](./security_agent.md)
