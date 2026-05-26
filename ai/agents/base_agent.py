#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
智能体基类
"""
from abc import ABC, abstractmethod
from typing import Dict, Any


class BaseAgent(ABC):
    def __init__(self, name: str, config: Dict = None):
        self.name = name
        self.config = config or {}
    
    @abstractmethod
    def process(self, guidance: str, context: Dict) -> Dict:
        """
        根据全局指导和上下文，执行本智能体的分析
        
        Args:
            guidance: Analyzer Agent 给出的全局优化指导
            context: 当前会话上下文
        
        Returns:
            本智能体的优化方案
        """
        pass
    
    def log(self, message: str):
        """记录日志"""
        print(f"[{self.name}] {message}")
