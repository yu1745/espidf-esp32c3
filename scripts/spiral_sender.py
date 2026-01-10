#!/usr/bin/env python3
"""
螺线绘制并发送T-Code指令到ESP32设备
"""

import socket
import time
import math
from typing import Tuple


def generate_spiral_points(
    turns: int = 3,
    points_per_turn: int = 60,
    a: float = 0.3
) -> list[Tuple[float, float]]:
    """
    生成阿基米德螺线坐标点

    Args:
        turns: 旋转圈数
        points_per_turn: 每圈的点数
        a: 螺线间距参数（控制螺线的疏密）

    Returns:
        包含(x, y)坐标的列表

    阿基米德螺线公式：
    x = a * θ * cos(θ)
    y = a * θ * sin(θ)
    """
    points = []
    max_theta = turns * 2 * math.pi
    step = max_theta / (turns * points_per_turn)

    theta = 0.0
    while theta <= max_theta:
        x = a * theta * math.cos(theta)
        y = a * theta * math.sin(theta)
        points.append((x, y))
        theta += step

    return points


def normalize_to_0_1(x: float, y: float, max_abs_value: float) -> Tuple[float, float]:
    """
    将坐标从[-max_abs_value, max_abs_value]映射到[0, 1]范围

    Args:
        x: x坐标
        y: y坐标
        max_abs_value: 绝对值的最大值

    Returns:
        映射后的(x_norm, y_norm)
    """
    x_norm = (x + max_abs_value) / (2 * max_abs_value)
    y_norm = (y + max_abs_value) / (2 * max_abs_value)

    # 限制在[0, 1]范围内
    x_norm = max(0.0, min(1.0, x_norm))
    y_norm = max(0.0, min(1.0, y_norm))

    return x_norm, y_norm


def format_tcode_value(value: float) -> str:
    """
    将0-1范围的值格式化为T-Code数值格式

    Args:
        value: 0-1范围的浮点数

    Returns:
        T-Code格式的数字字符串（例如：0.5 -> "500"）
    """
    # 转换为0-999的整数
    int_value = int(value * 1000)
    # 限制在0-999范围内
    int_value = max(0, min(999, int_value))
    return f"{int_value:03d}"


def create_tcode_command(l1_value: float, l2_value: float) -> str:
    """
    创建T-Code指令字符串

    Args:
        l1_value: L1轴的值（0-1范围）
        l2_value: L2轴的值（0-1范围）

    Returns:
        T-Code指令字符串（例如："L1500 L2500"）
    """
    l1_str = format_tcode_value(l1_value)
    l2_str = format_tcode_value(l2_value)
    return f"L1{l1_str} L2{l2_str}"


def send_tcode_via_udp(command: str, host: str = "192.168.5.210", port: int = 8000) -> None:
    """
    通过UDP发送T-Code指令

    Args:
        command: T-Code指令字符串
        host: 目标主机地址
        port: 目标端口
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(command.encode('utf-8'), (host, port))
        sock.close()
        print(f"发送: {command}")
    except Exception as e:
        print(f"发送失败: {e}")


def main():
    """
    主函数：生成螺线并发送T-Code指令
    """
    # 配置参数
    UDP_HOST = "192.168.5.210"
    UDP_PORT = 8000
    SPIRAL_TURNS = 3          # 螺线圈数
    POINTS_PER_TURN = 60      # 每圈点数
    SPIRAL_A = 0.3            # 螺线参数
    MAX_ABS_VALUE = 1.0       # 最大绝对值
    DELAY_MS = 20             # 发送间隔（毫秒）

    print("=" * 50)
    print("螺线绘制程序 - T-Code发送器")
    print("=" * 50)
    print(f"目标地址: {UDP_HOST}:{UDP_PORT}")
    print(f"螺线参数: {SPIRAL_TURNS}圈, 每圈{POINTS_PER_TURN}点")
    print(f"发送间隔: {DELAY_MS}ms")
    print("=" * 50)

    # 生成螺线点
    print("\n生成螺线坐标...")
    points = generate_spiral_points(
        turns=SPIRAL_TURNS,
        points_per_turn=POINTS_PER_TURN,
        a=SPIRAL_A
    )

    # 找出实际的最大绝对值
    actual_max_abs = max(abs(x) for x, y in points)
    actual_max_abs = max(actual_max_abs, max(abs(y) for x, y in points))
    print(f"生成 {len(points)} 个点，最大绝对值: {actual_max_abs:.4f}")

    # 确认是否继续
    response = input("\n按Enter开始发送，或输入'q'退出: ")
    if response.lower() == 'q':
        print("已取消")
        return

    print("\n开始发送...")

    # 发送每个点
    for i, (x, y) in enumerate(points):
        # 映射到0-1范围
        x_norm, y_norm = normalize_to_0_1(x, y, MAX_ABS_VALUE)

        # 创建T-Code指令
        tcode_cmd = create_tcode_command(x_norm, y_norm)

        # 发送指令
        send_tcode_via_udp(tcode_cmd, UDP_HOST, UDP_PORT)

        # 显示进度
        if (i + 1) % 20 == 0 or i == 0:
            progress = (i + 1) / len(points) * 100
            print(f"进度: {i + 1}/{len(points)} ({progress:.1f}%) - "
                  f"x={x_norm:.3f}, y={y_norm:.3f}")

        # 延迟
        time.sleep(DELAY_MS / 1000.0)

    print(f"\n✓ 完成！共发送 {len(points)} 条指令")


if __name__ == "__main__":
    main()
