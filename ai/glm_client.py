#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GLM 大模型客户端
"""
import os
import json
import requests
import ssl
from urllib3.poolmanager import PoolManager
from urllib3.util.ssl_ import create_urllib3_context
from urllib3.exceptions import InsecureRequestWarning
from typing import Dict, Any


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
        "max_tokens": 2000
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
            timeout=glm_config.get("timeout", 60),
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

