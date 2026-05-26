#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
命令行接口 - C 端调用
"""
import sys
import json
from .orchestrator import AgentOrchestrator


def main():
    if len(sys.argv) < 2:
        print("""Usage: python -m ai.cli <command> [args]

Commands:
  set_user <uid>
  clear_user
  record <operation> [path]
  analyze
  get_optimization
  get_last_analysis
  start_daemon <uid>
  stop_daemon
""")
        return
    
    command = sys.argv[1]
    orchestrator = AgentOrchestrator()
    
    try:
        if command == "set_user" and len(sys.argv) > 2:
            uid = int(sys.argv[2])
            result = orchestrator.set_user(uid)
            print(json.dumps(result, ensure_ascii=False))
        
        elif command == "clear_user":
            result = orchestrator.clear_user()
            print(json.dumps(result, ensure_ascii=False))
        
        elif command == "record" and len(sys.argv) > 2:
            operation = sys.argv[2]
            path = sys.argv[3] if len(sys.argv) > 3 else None
            result = orchestrator.record_operation(operation, path)
            print(json.dumps(result, ensure_ascii=False))
        
        elif command == "analyze":
            result = orchestrator.run_full_analysis()
            print(json.dumps(result, ensure_ascii=False, indent=2))
        
        elif command == "get_optimization":
            config = orchestrator.get_optimization_config()
            print(json.dumps(config, ensure_ascii=False))
        
        elif command == "get_last_analysis":
            analysis = orchestrator.get_last_analysis()
            print(json.dumps(analysis, ensure_ascii=False, indent=2))
        
        elif command == "start_daemon" and len(sys.argv) > 2:
            print(json.dumps({
                "status": "success",
                "message": "守护进程启动命令"
            }, ensure_ascii=False))
        
        elif command == "stop_daemon":
            print(json.dumps({
                "status": "success",
                "message": "守护进程停止命令"
            }, ensure_ascii=False))
        
        else:
            print(json.dumps({
                "status": "error",
                "message": "未知命令"
            }, ensure_ascii=False))
    
    except Exception as e:
        print(json.dumps({
            "status": "error",
            "message": str(e)
        }, ensure_ascii=False))


if __name__ == "__main__":
    main()
