#!/usr/bin/env python3
"""
蓝牙客户端测试脚本
连接名为'The Handy'的蓝牙设备并打印所有服务
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

DEVICE_NAME = "The Handy"


async def scan_and_find_device(device_name, timeout=10.0):
    """扫描并查找指定名称的蓝牙设备"""
    print(f"正在扫描BLE设备，查找 '{device_name}'...")
    print(f"扫描超时时间: {timeout}秒")
    
    try:
        # 扫描设备，获取广播数据
        devices_with_adv = await BleakScanner.discover(timeout=timeout, return_adv=True)
        
        if not devices_with_adv:
            print("未找到任何BLE设备")
            return None
        
        print(f"\n找到 {len(devices_with_adv)} 个设备:")
        target_device = None
        
        for address, (device, adv_data) in devices_with_adv.items():
            # 尝试从广播数据或设备名称中获取设备名
            name = adv_data.local_name or device.name or "未知设备"
            rssi = adv_data.rssi if adv_data.rssi else "N/A"
            
            print(f"  - {name} ({address}) RSSI: {rssi} dB")
            
            # 检查是否匹配目标设备名称
            if device_name.lower() in name.lower():
                target_device = device
                print(f"\n✓ 找到目标设备: {name} ({address})")
        
        if not target_device:
            print(f"\n✗ 未找到名为 '{device_name}' 的设备")
            print("请确认:")
            print("  1. 设备已开启并处于可发现模式")
            print("  2. 设备名称拼写正确")
            print("  3. 设备在蓝牙范围内")
        
        return target_device
        
    except Exception as e:
        print(f"扫描设备时出错: {e}")
        return None


async def list_all_services(device_address, device_name=None):
    """连接到设备并列出所有服务"""
    display_name = device_name or device_address
    print(f"\n正在连接到设备: {display_name}")
    
    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"✓ 连接成功!")
            print(f"设备地址: {client.address}")
            if hasattr(client, 'name') and client.name:
                print(f"设备名称: {client.name}")
            if hasattr(client, 'mtu_size'):
                print(f"MTU大小: {client.mtu_size}")
            
            print("\n" + "="*60)
            print("服务列表:")
            print("="*60)
            
            # 获取所有服务
            services = client.services
            services_list = list(services)
            
            if not services_list:
                print("未发现任何服务")
                return
            
            print(f"\n总共发现 {len(services_list)} 个服务:\n")
            
            # 遍历所有服务
            for service in services_list:
                print(f"服务 UUID: {service.uuid}")
                if service.description:
                    print(f"  描述: {service.description}")
                
                # 获取该服务的所有特征值
                characteristics = service.characteristics
                if characteristics:
                    print(f"  特征值数量: {len(characteristics)}")
                    for char in characteristics:
                        print(f"    - UUID: {char.uuid}")
                        if char.description:
                            print(f"      描述: {char.description}")
                        print(f"      属性: {', '.join(char.properties)}")
                        if char.descriptors:
                            print(f"      描述符数量: {len(char.descriptors)}")
                else:
                    print(f"  无特征值")
                
                print()  # 空行分隔
            
            print("="*60)
            
    except BleakError as e:
        print(f"✗ 连接失败: {e}")
        print("可能的原因:")
        print("  - 设备已断开连接")
        print("  - 设备不支持BLE连接")
        print("  - 连接超时")
    except Exception as e:
        print(f"✗ 发生错误: {e}")
        import traceback
        traceback.print_exc()


async def main():
    """主函数"""
    print("="*60)
    print("蓝牙客户端测试 - 列出所有服务")
    print("="*60)
    
    # 扫描并查找设备
    device = await scan_and_find_device(DEVICE_NAME)
    
    if not device:
        print("\n无法继续，未找到目标设备")
        sys.exit(1)
    
    # 连接并列出服务
    await list_all_services(device.address, device.name)
    
    print("\n测试完成!")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n用户中断")
        sys.exit(0)
    except Exception as e:
        print(f"\n程序异常: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

