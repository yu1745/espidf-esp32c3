#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TCP客户端脚本，用于向指定服务器持续发送数据
目标服务器: 192.168.5.209:8080
"""

import socket
import time
import threading
import sys
import random
import string
from collections import deque


class TCPClient:
    def __init__(self, host="192.168.5.210", port=8080):
        """
        初始化TCP客户端

        Args:
            host (str): 服务器地址
            port (int): 服务器端口
        """
        self.host = host
        self.port = port
        self.running = False
        self.socket = None
        self.send_interval = 1.0  # 发送间隔(秒)
        self.message_size = 64  # 消息大小(字节)

        # 发送速率统计相关变量
        self.bytes_sent_per_second = deque(maxlen=60)  # 存储最近60秒的发送字节数
        self.current_second_bytes = 0  # 当前秒已发送的字节数
        self.last_second_time = int(time.time())  # 上一秒的时间戳
        self.stats_thread = None  # 统计线程
        self.stats_running = False  # 统计线程运行标志

    def generate_random_data(self, size=64):
        """
        生成随机数据

        Args:
            size (int): 数据大小

        Returns:
            str: 随机字符串
        """
        return "".join(random.choices(string.ascii_letters + string.digits, k=size))

    def connect(self):
        """
        连接到服务器

        Returns:
            bool: 连接是否成功
        """
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(5)  # 设置超时时间
            self.socket.connect((self.host, self.port))
            print(f"成功连接到 {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            if self.socket:
                self.socket.close()
                self.socket = None
            return False

    def send_data(self, data):
        """
        发送数据

        Args:
            data (str): 要发送的数据

        Returns:
            bool: 发送是否成功
        """
        try:
            if self.socket:
                # 添加时间戳和换行符
                timestamp = time.strftime("%Y-%m-%d %H:%M:%S") + ".{:03d}".format(
                    int(time.time() * 1000) % 1000
                )
                message = f"[{timestamp}] {data}\n"
                message_bytes = message.encode("utf-8")
                self.socket.send(message_bytes)

                # 更新发送速率统计
                self._update_send_stats(len(message_bytes))

                # print(f"已发送: {message.strip()}")
                return True
        except Exception as e:
            print(f"发送数据失败: {e}")
            return False

    def _update_send_stats(self, bytes_count):
        """
        更新发送速率统计

        Args:
            bytes_count (int): 本次发送的字节数
        """
        current_time = int(time.time())

        # 如果进入新的一秒，保存上一秒的数据
        if current_time > self.last_second_time:
            self.bytes_sent_per_second.append(self.current_second_bytes)
            self.current_second_bytes = 0
            self.last_second_time = current_time

        # 累加当前秒发送的字节数
        self.current_second_bytes += bytes_count

    def _stats_worker(self):
        """
        统计工作线程，每秒打印发送速率
        """
        while self.stats_running:
            time.sleep(1)  # 每秒统计一次

            # 确保当前秒的数据被记录
            current_time = int(time.time())
            if current_time > self.last_second_time:
                self.bytes_sent_per_second.append(self.current_second_bytes)
                self.current_second_bytes = 0
                self.last_second_time = current_time

            # 计算并发送速率统计
            if len(self.bytes_sent_per_second) > 0:
                recent_bytes = list(self.bytes_sent_per_second)
                avg_bytes_per_second = sum(recent_bytes) / len(recent_bytes)
                max_bytes_per_second = max(recent_bytes) if recent_bytes else 0
                current_bps = recent_bytes[-1] if recent_bytes else 0

                print(
                    f"[发送速率] 当前: {current_bps} 字节/秒, 平均: {avg_bytes_per_second:.1f} 字节/秒, 最大: {max_bytes_per_second} 字节/秒"
                )

    def start_sending(self):
        """
        开始持续发送数据
        """
        self.running = True

        # 首先尝试连接
        if not self.connect():
            print("无法连接到服务器，程序退出")
            return

        # 启动统计线程
        self.stats_running = True
        self.stats_thread = threading.Thread(target=self._stats_worker, daemon=True)
        self.stats_thread.start()
        print("发送速率统计已启动")

        try:
            while self.running:
                # 生成随机数据
                data = self.generate_random_data(self.message_size)

                # 发送数据
                if not self.send_data(data):
                    print("发送失败，尝试重新连接...")
                    if self.connect():
                        print("重新连接成功")
                    else:
                        print("重新连接失败，等待5秒后重试...")
                        time.sleep(5)
                        continue

                # 等待指定间隔
                # time.sleep(self.send_interval)

        except KeyboardInterrupt:
            print("\n收到中断信号，停止发送...")
        finally:
            self.stop()

    def stop(self):
        """
        停止发送并关闭连接
        """
        self.running = False

        # 停止统计线程
        self.stats_running = False
        if self.stats_thread and self.stats_thread.is_alive():
            self.stats_thread.join(timeout=1)

        # 打印最终统计信息
        if len(self.bytes_sent_per_second) > 0:
            total_bytes = sum(self.bytes_sent_per_second) + self.current_second_bytes
            total_seconds = len(self.bytes_sent_per_second) + (
                1 if self.current_second_bytes > 0 else 0
            )
            avg_bytes_per_second = (
                total_bytes / total_seconds if total_seconds > 0 else 0
            )
            max_bytes_per_second = max(
                list(self.bytes_sent_per_second) + [self.current_second_bytes]
            )

            print(f"\n=== 发送速率统计摘要 ===")
            print(f"总发送字节数: {total_bytes} 字节")
            print(f"运行时间: {total_seconds} 秒")
            print(f"平均发送速率: {avg_bytes_per_second:.1f} 字节/秒")
            print(f"最大发送速率: {max_bytes_per_second} 字节/秒")
            print("=====================")

        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        print("连接已关闭")


def main():
    """
    主函数
    """
    print("TCP客户端启动")
    print("目标服务器: 192.168.5.209:8080")
    print("功能: 持续发送数据并统计发送速率")
    print("按 Ctrl+C 停止程序")
    print("-" * 50)

    # 创建TCP客户端实例
    client = TCPClient()

    try:
        # 开始发送数据
        client.start_sending()
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    except Exception as e:
        print(f"程序发生错误: {e}")
    finally:
        client.stop()


if __name__ == "__main__":
    main()
