#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compatibility bridge for C-side nlp_interact.
Prints plain assistant text and optionally an EXEC: command line.
"""
import json
import re
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from ai.glm_client import GLMClient, load_glm_config
else:
    from ai.glm_client import GLMClient, load_glm_config


def heuristic_command(user_input: str):
    text = user_input.strip()
    lower = text.lower()

    patterns = [
        (r"(?:create|创建)(?:一个)?(?:文件)?\s+([A-Za-z0-9._-]+)", "create {0}"),
        (r"(?:delete|删除)(?:文件)?\s+([A-Za-z0-9._-]+)", "delete {0}"),
        (r"(?:mkdir|创建目录)\s+([A-Za-z0-9._-]+)", "mkdir {0}"),
        (r"(?:rmdir|删除目录)\s+([A-Za-z0-9._-]+)", "rmdir {0}"),
        (r"(?:chdir|进入目录|切换到目录)\s+([A-Za-z0-9._/-]+)", "chdir {0}"),
        (r"(?:readlink)\s+([A-Za-z0-9._/-]+)", "readlink {0}"),
        (r"(?:unlink)\s+([A-Za-z0-9._/-]+)", "unlink {0}"),
        (r"(?:link)\s+([A-Za-z0-9._/-]+)\s+([A-Za-z0-9._/-]+)", "link {0} {1}"),
        (r"(?:symlink)\s+([A-Za-z0-9._/-]+)\s+([A-Za-z0-9._/-]+)", "symlink {0} {1}"),
    ]

    if lower in {"dir", "ls", "list", "列出目录", "查看目录"}:
        return {"command": "dir", "explanation": "列出当前目录", "success": True}
    if lower in {"help", "帮助"}:
        return {"command": "help", "explanation": "显示帮助", "success": True}
    if lower in {"optimize", "优化"}:
        return {"command": "optimize", "explanation": "应用优化参数", "success": True}
    if lower in {"start session", "start_session", "开始会话"}:
        return {"command": "start_session", "explanation": "开始智能体会话", "success": True}
    if lower in {"end session", "end_session", "结束会话"}:
        return {"command": "end_session", "explanation": "结束智能体会话", "success": True}

    for pattern, template in patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return {
                "command": template.format(*match.groups()),
                "explanation": "已解析请求",
                "success": True,
            }

    return {"command": "", "explanation": "未能从输入中提取可执行命令", "success": False}


def parse_with_glm(user_input: str):
    cfg = load_glm_config()
    api_key = cfg.get("api_key", "")
    if not api_key or api_key == "your_api_key_here" or len(api_key) < 10:
        return heuristic_command(user_input)

    client = GLMClient()
    result = client.parse_command(user_input)
    if isinstance(result, dict) and result.get("success"):
        return result
    return heuristic_command(user_input)


def main():
    if len(sys.argv) < 2:
        print("请输入自然语言请求")
        print("EXEC:unknown")
        return

    user_input = sys.argv[1]
    result = parse_with_glm(user_input)
    explanation = result.get("explanation", "")
    command = result.get("command", "")

    if explanation:
        print(explanation)
    print(f"EXEC:{command or 'unknown'}")


if __name__ == "__main__":
    main()
