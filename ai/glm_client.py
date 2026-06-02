#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GLM 大模型客户端
"""
import sys
import os
import json
import requests
import ssl
from urllib3.poolmanager import PoolManager
from urllib3.util.ssl_ import create_urllib3_context
from urllib3.exceptions import InsecureRequestWarning
from typing import Dict, Any, Optional


class TLSAdapter(requests.adapters.HTTPAdapter):
    """自定义 TLS 适配器，支持更灵活的 TLS 配置"""
    
    def __init__(self, *args, **kwargs):
        self._ssl_verify = kwargs.pop('ssl_verify', True)
        super().__init__(*args, **kwargs)
    
    def init_poolmanager(self, connections, maxsize, block=False):
        if self._ssl_verify:
            # 创建自定义 SSL 上下文（验证模式）
            ctx = create_urllib3_context()
            ctx.minimum_version = ssl.TLSVersion.TLSv1_2
            ctx.maximum_version = ssl.TLSVersion.TLSv1_3
            ctx.options |= ssl.OP_NO_SSLv2
            ctx.options |= ssl.OP_NO_SSLv3
            
            self.poolmanager = PoolManager(
                num_pools=connections,
                maxsize=maxsize,
                block=block,
                ssl_context=ctx
            )
        else:
            # 创建不验证证书的 SSL 上下文
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            
            self.poolmanager = PoolManager(
                num_pools=connections,
                maxsize=maxsize,
                block=block,
                ssl_context=ctx,
                cert_reqs='CERT_NONE'
            )


def load_glm_config(config: Dict = None) -> Dict[str, Any]:
    """
    加载 GLM 配置
    
    Args:
        config: 可选，外部传入的配置
    
    Returns:
        GLM 配置
    """
    if config and "glm" in config:
        glm_cfg = config.get("glm", {})
        return {
            "api_key": glm_cfg.get("api_key", ""),
            "api_base_url": glm_cfg.get("api_base_url", "https://open.bigmodel.cn/api/paas/v4/chat/completions"),
            "model_name": glm_cfg.get("model_name", "glm-4"),
            "timeout": glm_cfg.get("timeout", 30),
            "proxies": glm_cfg.get("proxies", None),
            "verify_ssl": glm_cfg.get("verify_ssl", False)  # 默认关闭证书验证（解决Windows环境问题）
        }
    
    # 从 config.json 读取
    config_file = "config.json"
    if os.path.exists(config_file):
        try:
            with open(config_file, "r", encoding="utf-8") as f:
                cfg = json.load(f)
                glm_cfg = cfg.get("glm", {})
                return {
                    "api_key": glm_cfg.get("api_key", ""),
                    "api_base_url": glm_cfg.get("api_base_url", "https://open.bigmodel.cn/api/paas/v4/chat/completions"),
                    "model_name": glm_cfg.get("model_name", "glm-4"),
                    "timeout": glm_cfg.get("timeout", 30),
                    "proxies": glm_cfg.get("proxies", None),
                    "verify_ssl": glm_cfg.get("verify_ssl", False)
                }
        except Exception:
            pass
    
    return {
        "api_key": "",
        "api_base_url": "https://open.bigmodel.cn/api/paas/v4/chat/completions",
        "model_name": "glm-4",
        "timeout": 30,
        "proxies": None,
        "verify_ssl": False  # 默认关闭证书验证
    }


def call_glm_api(prompt: str, system_prompt: str = "你是一个专业的 AI 助手", config: Dict = None) -> str:
    """
    简单的 GLM API 调用
    
    Args:
        prompt: 用户提示词
        system_prompt: 系统提示词
        config: 可选，外部配置
    
    Returns:
        GLM 的响应
    """
    glm_config = load_glm_config(config)
    api_key = glm_config.get("api_key", "")
    verify_ssl = glm_config.get("verify_ssl", False)
    
    if not api_key or api_key == "your_api_key_here" or len(api_key) < 10:
        return "[未配置 GLM API 密钥，请在 config.json 中配置]"
    
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {api_key}"
    }
    
    payload = {
        "model": glm_config.get("model_name", "glm-4"),
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.7,
        "max_tokens": 1000
    }
    
    try:
        # 禁用不安全请求警告（当关闭SSL验证时）
        if not verify_ssl:
            requests.packages.urllib3.disable_warnings(InsecureRequestWarning)
        
        # 创建 Session 并配置 TLS
        session = requests.Session()
        session.mount("https://", TLSAdapter(ssl_verify=verify_ssl))
        
        # 移除代理配置，避免代理导致的连接问题
        session.trust_env = False
        
        response = session.post(
            glm_config.get("api_base_url", "https://open.bigmodel.cn/api/paas/v4/chat/completions"),
            headers=headers,
            json=payload,
            timeout=glm_config.get("timeout", 30),
            verify=verify_ssl
        )
        
        # 保存原始响应信息用于调试
        status_code = response.status_code
        content_type = response.headers.get('Content-Type', '')
        text = response.text
        
        if status_code == 200 and 'application/json' in content_type:
            try:
                result = response.json()
                return result["choices"][0]["message"]["content"]
            except (json.JSONDecodeError, KeyError) as e:
                return f"[JSON 解析失败: {e}] 响应内容: {text[:200]}"
        else:
            return f"[API 调用失败: {status_code}] Content-Type: {content_type} 响应内容: {text[:200]}"
    
    except requests.exceptions.SSLError as e:
        return f"[SSL 连接错误: {e}] 请检查网络环境或尝试更换网络"
    except requests.exceptions.ProxyError as e:
        return f"[代理错误: {e}] 已禁用代理，请检查网络设置"
    except requests.exceptions.ConnectionError as e:
        return f"[连接错误: {e}] 请检查网络连接或稍后重试"
    except requests.exceptions.Timeout as e:
        return f"[请求超时: {e}] 服务器响应时间过长"
    except Exception as e:
        return f"[请求异常: {e}]"


# ========== 旧的自然语言交互兼容 ==========

class GLMClient:
    """旧的 GLM 客户端（保留用于 nlp_interact）"""
    
    def __init__(self):
        self.system_prompt = self._build_system_prompt()
    
    def _build_system_prompt(self) -> str:
        return """你是一个文件系统的智能助手，你的任务是将用户的自然语言请求转换为文件系统命令。

可用的文件系统命令：
- login [username] [password]: 用户登录
- logout: 用户登出
- format: 格式化磁盘
- create <filename>: 创建文件
- delete <filename>: 删除文件
- open <filename> <mode>: 打开文件（mode: r/w/rw）
- close <fd>: 关闭文件（fd: 文件描述符）
- read <fd> <count>: 读取文件内容
- write <fd> <data>: 写入文件内容
- mkdir <dirname>: 创建目录
- rmdir <dirname>: 删除目录
- chmod <filename> <mode>: 修改文件权限
- chdir <dirname>: 切换目录
- dir: 列出当前目录内容
- init_kfs: 初始化 KFS 智能文件分类系统
- kfs_list <vdir>: 列出虚拟目录
- kfs_tags <ino>: 显示文件标签
- init_io_opt: 初始化 I/O 优化器
- io_stats: 显示 I/O 统计信息
- init_security: 初始化安全系统
- user_profile: 显示用户行为画像
- security_log: 显示安全事件日志
- start_session: 开始多智能体会话
- end_session: 结束多智能体会话
- show_context: 显示会话上下文
- suggestions: 获取多智能体建议
- optimize: 应用多智能体优化
- help: 显示帮助信息
- exit: 退出系统

要求：
1. 仔细分析用户输入，选择最合适的命令
2. 返回格式必须是 JSON，包含以下字段：
   - "command": 命令字符串（不含任何额外解释）
   - "explanation": 简短的命令解释
   - "success": 布尔值，表示是否成功解析
3. 如果无法理解用户请求，返回 success: false，command 为空，explanation 说明原因
4. 只返回 JSON，不要有任何其他文字
5. 命令应该是可以直接执行的格式，参数用空格分隔

示例：
用户：创建一个叫 test.txt 的文件
返回：{"command": "create test.txt", "explanation": "创建文件 test.txt", "success": true}

用户：开始学习会话
返回：{"command": "start_session", "explanation": "开始多智能体会话", "success": true}
"""
    
    def _call_glm_api(self, user_input: str) -> Optional[str]:
        glm_config = load_glm_config()
        api_key = glm_config.get("api_key", "")
        verify_ssl = glm_config.get("verify_ssl", False)
        
        if not api_key or api_key == "your_api_key_here":
            print("错误: 请先在 config.json 中配置您的 GLM API 密钥！")
            return None
        
        # 禁用不安全请求警告
        if not verify_ssl:
            requests.packages.urllib3.disable_warnings(InsecureRequestWarning)
        
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}"
        }
        
        payload = {
            "model": glm_config.get("model_name", "glm-4"),
            "messages": [
                {"role": "system", "content": self.system_prompt},
                {"role": "user", "content": user_input}
            ],
            "temperature": 0.3,
            "max_tokens": 500
        }
        
        try:
            # 创建 Session 并配置 TLS
            session = requests.Session()
            session.mount("https://", TLSAdapter(ssl_verify=verify_ssl))
            session.trust_env = False  # 禁用代理
            
            response = session.post(
                glm_config.get("api_base_url", "https://open.bigmodel.cn/api/paas/v4/chat/completions"),
                headers=headers,
                json=payload,
                timeout=glm_config.get("timeout", 30),
                verify=verify_ssl
            )
            
            if response.status_code == 200:
                result = response.json()
                return result["choices"][0]["message"]["content"]
            else:
                print(f"API 请求失败: {response.status_code}")
                print(response.text)
                return None
        except requests.exceptions.SSLError as e:
            print(f"SSL 连接错误: {e}")
            return None
        except requests.exceptions.ProxyError as e:
            print(f"代理错误: {e}")
            return None
        except Exception as e:
            print(f"请求异常: {e}")
            return None
    
    def parse_command(self, user_input: str) -> Optional[Dict[str, Any]]:
        response = self._call_glm_api(user_input)
        if response is None:
            return None
        
        try:
            response = response.strip()
            if response.startswith("```json"):
                response = response[7:]
            if response.endswith("```"):
                response = response[:-3]
            
            result = json.loads(response)
            return result
        except Exception as e:
            print(f"解析响应失败: {e}")
            print(f"原始响应: {response}")
            return None


# 旧的 CLI 入口兼容
def main():
    if len(sys.argv) < 2:
        print("Usage: python ai/glm_client.py <user_input>")
        return
    
    user_input = sys.argv[1]
    client = GLMClient()
    result = client.parse_command(user_input)
    
    if result:
        print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
