#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
from agent_tools import summarize_text, search_keywords, context_qa


class NLPParser:
    """自然语言解析器"""
    
    def __init__(self):
        self.commands = [
            # 记忆相关
            {
                "patterns": [
                    r"记住?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                    r"保存?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                    r"记录?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                    r"我想记住?[：:.]?\s*(.+)",
                    r"请记住?[：:.]?\s*(.+)",
                ],
                "action": "add_memory",
                "memory_type": "long",
                "extract_content": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"临时记住?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                    r"暂时记住?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                    r"短期记住?(?:这个|这个信息|这些|这句话|以下内容)?[：:.]?\s*(.+)",
                ],
                "action": "add_memory",
                "memory_type": "short",
                "extract_content": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"查看记忆",
                    r"列出记忆",
                    r"显示记忆",
                    r"我的记忆",
                    r"看看记忆",
                ],
                "action": "list_memories",
            },
            # 文件操作
            {
                "patterns": [
                    r"创建文件[：:.]?\s*(\w+)",
                    r"新建文件[：:.]?\s*(\w+)",
                    r"生成文件[：:.]?\s*(\w+)",
                ],
                "action": "create",
                "extract_name": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"删除文件[：:.]?\s*(\w+)",
                    r"移除文件[：:.]?\s*(\w+)",
                ],
                "action": "delete",
                "extract_name": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"创建目录[：:.]?\s*(\w+)",
                    r"新建目录[：:.]?\s*(\w+)",
                    r"生成目录[：:.]?\s*(\w+)",
                    r"创建文件夹[：:.]?\s*(\w+)",
                ],
                "action": "mkdir",
                "extract_name": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"删除目录[：:.]?\s*(\w+)",
                    r"移除目录[：:.]?\s*(\w+)",
                    r"删除文件夹[：:.]?\s*(\w+)",
                ],
                "action": "rmdir",
                "extract_name": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"切换到[：:.]?\s*(\w+)",
                    r"进入[：:.]?\s*(\w+)",
                    r"转到[：:.]?\s*(\w+)",
                    r"cd\s*(\w+)",
                ],
                "action": "chdir",
                "extract_name": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"查看目录",
                    r"列出目录",
                    r"显示目录",
                    r"dir",
                    r"ls",
                ],
                "action": "dir",
            },
            # AI 工具
            {
                "patterns": [
                    r"总结[：:.]?\s*(.+)",
                    r"摘要[：:.]?\s*(.+)",
                    r"帮我总结[：:.]?\s*(.+)",
                    r"总结一下[：:.]?\s*(.+)",
                ],
                "action": "call_tool",
                "tool_name": "summarize",
                "extract_args": lambda m: m.group(1)
            },
            {
                "patterns": [
                    r"搜索[：:.]?\s*(.+)\s+在[：:.]?\s*(.+)",
                    r"查找[：:.]?\s*(.+)\s+在[：:.]?\s*(.+)",
                    r"找[：:.]?\s*(.+)\s+在[：:.]?\s*(.+)",
                ],
                "action": "call_tool",
                "tool_name": "search",
                "extract_args": lambda m: f"{m.group(1)}:{m.group(2)}"
            },
            {
                "patterns": [
                    r"问答[：:.]?\s*(.+)\s+关于[：:.]?\s*(.+)",
                    r"回答[：:.]?\s*(.+)\s+基于[：:.]?\s*(.+)",
                ],
                "action": "call_tool",
                "tool_name": "qa",
                "extract_args": lambda m: f"{m.group(1)}:{m.group(2)}"
            },
            {
                "patterns": [
                    r"分析日志",
                    r"查看日志",
                    r"日志分析",
                ],
                "action": "call_tool",
                "tool_name": "analyze_logs",
                "extract_args": lambda m: ""
            },
            {
                "patterns": [
                    r"分类记忆[：:.]?\s*(.+)",
                    r"判断记忆类型[：:.]?\s*(.+)",
                ],
                "action": "call_tool",
                "tool_name": "classify_memory",
                "extract_args": lambda m: m.group(1)
            },
            # 帮助
            {
                "patterns": [
                    r"帮助",
                    r"help",
                    r"怎么用",
                    r"怎么操作",
                ],
                "action": "help",
            },
        ]
    
    def parse(self, text):
        """解析自然语言文本"""
        text = text.strip().lower()
        
        for cmd in self.commands:
            for pattern in cmd["patterns"]:
                match = re.search(pattern, text, re.IGNORECASE)
                if match:
                    return self._build_command(cmd, match)
        
        return None
    
    def _build_command(self, cmd, match):
        """构建命令对象"""
        result = {
            "action": cmd["action"]
        }
        
        if cmd["action"] == "add_memory":
            result["memory_type"] = cmd["memory_type"]
            result["content"] = cmd["extract_content"](match)
        
        elif cmd["action"] in ["create", "delete", "mkdir", "rmdir", "chdir"]:
            result["name"] = cmd["extract_name"](match)
        
        elif cmd["action"] == "call_tool":
            result["tool_name"] = cmd["tool_name"]
            result["args"] = cmd["extract_args"](match) if "extract_args" in cmd else ""
        
        return result


def main():
    if len(sys.argv) < 2:
        print("Usage: python nlp_parser.py <text>")
        sys.exit(1)
    
    text = sys.argv[1]
    parser = NLPParser()
    
    print("=== 自然语言解析 ===")
    print(f"输入: {text}")
    print("-" * 40)
    
    result = parser.parse(text)
    
    if result:
        print(f"识别动作: {result['action']}")
        
        if result["action"] == "add_memory":
            print(f"记忆类型: {result['memory_type']}")
            print(f"内容: {result['content']}")
            # 输出可执行的命令格式
            print(f"\nEXEC:add_memory {result['memory_type']} {result['content']}")
        
        elif result["action"] in ["create", "delete", "mkdir", "rmdir", "chdir"]:
            print(f"名称: {result['name']}")
            print(f"\nEXEC:{result['action']} {result['name']}")
        
        elif result["action"] == "call_tool":
            print(f"工具: {result['tool_name']}")
            print(f"参数: {result['args']}")
            print(f"\nEXEC:call_tool {result['tool_name']} {result['args']}")
        
        elif result["action"] in ["list_memories", "dir", "help"]:
            print(f"\nEXEC:{result['action']}")
        
    else:
        print("未能理解您的指令，请尝试：")
        print("  - 记住：xxx")
        print("  - 查看记忆")
        print("  - 总结：xxx")
        print("  - 创建文件 xxx")
        print("  - 帮助")
        print("\nEXEC:unknown")


if __name__ == "__main__":
    main()
