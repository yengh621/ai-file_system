#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import os


def summarize_text(text):
    """文本摘要工具：简单提取关键句子"""
    sentences = text.split('.')
    if len(sentences) <= 3:
        return text
    
    # 简单策略：取第一句和最后一句作为摘要
    summary = sentences[0].strip() + '. ' + sentences[-1].strip() + '.'
    return summary


def search_keywords(text, keywords):
    """关键词搜索工具"""
    results = []
    lines = text.split('\n')
    for i, line in enumerate(lines, 1):
        for keyword in keywords.split(','):
            if keyword.strip().lower() in line.lower():
                results.append(f"Line {i}: {line}")
                break
    return '\n'.join(results) if results else "No matches found."


def context_qa(context, question):
    """上下文问答工具：简单的关键词匹配回答"""
    keywords = question.lower().split()
    sentences = context.split('.')
    
    for sentence in sentences:
        sentence_lower = sentence.lower()
        matches = sum(1 for kw in keywords if kw in sentence_lower)
        if matches >= len(keywords) // 2:
            return sentence.strip() + '.'
    
    return "Sorry, I couldn't find an answer to that question."


def analyze_logs():
    """日志分析工具"""
    print("=== AI Log Analysis ===")
    print("This tool would analyze the AI operation logs.")
    print("Currently, it's a placeholder for the full implementation.")
    return "Log analysis complete."


def main():
    if len(sys.argv) < 3:
        print("Usage: python agent_tools.py <tool_name> <uid> [args]")
        sys.exit(1)
    
    tool_name = sys.argv[1]
    uid = sys.argv[2]
    args = sys.argv[3] if len(sys.argv) > 3 else ""
    
    print(f"=== AI Tool: {tool_name} ===")
    print(f"User ID: {uid}")
    print(f"Arguments: {args}")
    print("-" * 40)
    
    try:
        if tool_name == "summarize":
            result = summarize_text(args)
            print("Summary:")
            print(result)
        elif tool_name == "search":
            # 格式: search keyword1,keyword2:text_to_search
            if ':' in args:
                keywords, text = args.split(':', 1)
                result = search_keywords(text, keywords)
                print("Search Results:")
                print(result)
            else:
                print("Invalid format. Use: search keywords:text")
        elif tool_name == "qa":
            # 格式: qa question:context
            if ':' in args:
                question, context = args.split(':', 1)
                result = context_qa(context, question)
                print("Answer:")
                print(result)
            else:
                print("Invalid format. Use: qa question:context")
        elif tool_name == "analyze_logs":
            result = analyze_logs()
            print(result)
        elif tool_name == "classify_memory":
            # 简单的记忆分类
            print("Memory Classification:")
            if any(keyword in args.lower() for keyword in ['important', 'remember', 'save', 'permanent']):
                print("This should be stored as LONG-TERM memory.")
            else:
                print("This can be stored as SHORT-TERM memory.")
        else:
            print(f"Unknown tool: {tool_name}")
            print("Available tools: summarize, search, qa, analyze_logs, classify_memory")
            sys.exit(1)
        
        print("\nTool execution completed successfully.")
        return 0
        
    except Exception as e:
        print(f"Error executing tool: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
