# 记忆系统设计方案

## 🎯 记忆分类策略

### 什么应该存在哪里？

| 内容类型 | 存储位置 | 原因 | 示例 |
|---------|---------|------|------|
| **密码/密钥** | 🔒 **正常加密文件** | 需要加密保护，不适合明文存记忆 | `/secure/passwords.txt` (加密) |
| **重要文档** | 📁 **正常文件** | 需要版本控制和备份 | `/docs/important_report.pdf` |
| **使用习惯** | 🧠 **长期记忆** | 用于优化系统，不需要加密 | "我通常在9点工作" |
| **临时笔记** | 💭 **短期记忆** | 临时信息，自动清理 | "明天记得开会" |
| **文件标签** | 🏷️ **KFS 标签** | 用于文件分类 | "项目文档", "学习资料" |
| **系统设置** | ⚙️ **配置文件** | 系统参数，独立管理 | `config.json` |

---

## 📋 长期记忆存储内容

**应该存：**
1. 用户行为模式
   - "我每天9点访问 /project/ 目录"
   - "我喜欢用 .py 文件"
   - "我每月底整理一次 /downloads/"

2. 个性化偏好
   - "我喜欢文件按类型分类"
   - "我通常打开3个文件同时工作"

3. 学习到的优化参数
   - "我的最佳预取窗口是8"
   - "我的安全阈值应该是15"

**不应该存：**
- ❌ 密码、密钥、敏感信息
- ❌ 大文件内容
- ❌ 需要加密的数据

---

## 📝 短期记忆存储内容

**应该存：**
1. 会话上下文
   - "刚才打开的文件是 /doc/notes.txt"
   - "上一个命令是 create test.py"

2. 临时提醒
   - "别忘了保存文件"
   - "30分钟后检查报告"

3. 缓存数据
   - 最近访问的文件列表
   - 搜索历史

**过期策略：**
- 会话结束自动清理
- 24小时后自动清理
- 手动清理

---

## 🗄️ 记忆数据格式

### 长期记忆格式
```
/agent/memory/long_term/pattern_{uid}.json
{
    "version": "1.0",
    "uid": 1,
    "learned_patterns": {
        "peak_hours": [9, 14, 16],
        "preferred_extensions": [".py", ".txt", ".md"],
        "common_directories": ["/project", "/docs"],
        "delete_frequency": 3,
        "modify_frequency": 8
    },
    "optimization_config": {
        "prefetch_window": 8,
        "delete_threshold": 8,
        "modify_threshold": 15
    },
    "last_updated": "2026-05-26T14:30:00"
}
```

### 短期记忆格式
```
/agent/memory/short_term/session_{uid}_{timestamp}.json
{
    "session_id": "abc123",
    "uid": 1,
    "context": {
        "last_file": "/doc/notes.txt",
        "last_command": "create test.py",
        "recent_files": ["/a.txt", "/b.py"]
    },
    "reminders": [
        {"text": "保存文件", "time": "2026-05-26T15:00:00"}
    ],
    "expires_at": "2026-05-27T14:30:00"
}
```

---

## 🔗 与其他系统的关系

```
用户操作
   ↓
NLP 解析 (glm_integration.py)
   ↓
命令执行
   ↓
行为记录 → 记忆优化器 (memory_optimizer.py)
   ↓           ↓
   ↓       学习模式
   ↓           ↓
   ↓       生成优化建议
   ↓           ↓
   └─────→ 应用到三大创新系统
              ├─ KFS 智能分类
              ├─ I/O 自适应优化
              └─ 安全行为检测
```

---

## 📊 记忆 vs 正常文件对比

| 特性 | 记忆文件 | 正常文件 |
|------|---------|---------|
| 位置 | `/agent/memory/` | 任意位置 |
| 格式 | 结构化 JSON | 任意格式 |
| 用途 | 系统优化学习 | 用户数据存储 |
| 编辑 | 系统自动管理 | 用户直接编辑 |
| 备份 | 可重建 | 需要用户备份 |
| 敏感数据 | ❌ 不适合 | ✅ 适合（可加密） |
