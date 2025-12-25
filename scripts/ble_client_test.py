#!/usr/bin/env python3
"""
蓝牙客户端测试脚本
用于测试ESP32蓝牙服务器（基于gatt_svc.c实现的服务）
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# 从gatt_svc.c中提取的UUID
HEART_RATE_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb"  # 0x180D
HEART_RATE_CHAR_UUID = "00002a37-0000-1000-8000-00805f9b34fb"  # 0x2A37
AUTO_IO_SERVICE_UUID = "00001815-0000-1000-8000-00805f9b34fb"  # 0x1815
LED_CHAR_UUID = "23d1bcea-5f78-2315-deef-121225150000"  # 自定义128位UUID


async def scan_devices(timeout=5.0):
    """扫描附近的BLE设备"""
    print(f"正在扫描BLE设备，等待{timeout}秒...")
    # 使用return_adv=True来获取包含RSSI信息的广播数据
    devices_with_adv = await BleakScanner.discover(timeout=timeout, return_adv=True)

    # 将设备信息转换为更易使用的格式
    devices = []
    for address, (device, adv_data) in devices_with_adv.items():
        devices.append((device, adv_data))

    if not devices:
        print("未找到任何BLE设备")
        return []

    print(f"找到 {len(devices)} 个设备:")
    for i, (device, adv_data) in enumerate(devices):
        print(
            f"  {i+1}. {device.name or '未知设备'} - {device.address} (RSSI: {adv_data.rssi} dB)"
        )

    return devices


async def connect_and_test(device_address, device_name=None):
    """连接到设备并测试GATT服务"""
    print(f"\n正在连接到设备: {device_name or device_address}")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取所有服务
            services = client.services
            services_list = list(services)
            print(f"\n发现 {len(services_list)} 个服务:")

            for service in services_list:
                print(f"  服务: {service.uuid} - {service.description}")
                for char in service.characteristics:
                    print(f"    特征: {char.uuid} - {char.description}")
                    print(f"      属性: {char.properties}")

            # 测试心率服务
            print(f"\n--- 测试心率服务 ---")
            heart_rate_service = services.get_service(HEART_RATE_SERVICE_UUID)
            if heart_rate_service:
                print(f"找到心率服务 ({HEART_RATE_SERVICE_UUID})")

                # 检查心率特征
                heart_rate_char = heart_rate_service.get_characteristic(
                    HEART_RATE_CHAR_UUID
                )
                if heart_rate_char:
                    print(f"找到心率特征 ({HEART_RATE_CHAR_UUID})")

                    # 读取心率特征
                    try:
                        value = await client.read_gatt_char(HEART_RATE_CHAR_UUID)
                        # 心率数据格式：第一个字节是标志位，第二个字节是心率值
                        if len(value) >= 2:
                            flags = value[0]
                            heart_rate = value[1]
                            print(f"心率值: {heart_rate} BPM (标志位: 0x{flags:02x})")
                        else:
                            print(f"心率原始数据: {value.hex()}")
                    except Exception as e:
                        print(f"读取心率特征失败: {e}")
                else:
                    print(f"未找到心率特征")
            else:
                print(f"未找到心率服务")

            # 测试自动化IO服务
            print(f"\n--- 测试自动化IO服务 ---")
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if auto_io_service:
                print(f"找到自动化IO服务 ({AUTO_IO_SERVICE_UUID})")

                # 检查LED特征
                led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
                if led_char:
                    print(f"找到LED特征 ({LED_CHAR_UUID})")

                    # 测试写入LED特征（打开LED）
                    print("测试写入LED特征: 打开LED (值: 0x01)")
                    try:
                        await client.write_gatt_char(
                            LED_CHAR_UUID, b"\x01", response=True
                        )
                        print("写入成功 - LED应已打开")
                    except Exception as e:
                        print(f"写入LED特征失败: {e}")

                    # 等待2秒
                    await asyncio.sleep(2)

                    # 测试写入LED特征（关闭LED）
                    print("测试写入LED特征: 关闭LED (值: 0x00)")
                    try:
                        await client.write_gatt_char(
                            LED_CHAR_UUID, b"\x00", response=True
                        )
                        print("写入成功 - LED应已关闭")
                    except Exception as e:
                        print(f"写入LED特征失败: {e}")
                else:
                    print(f"未找到LED特征")
            else:
                print(f"未找到自动化IO服务")

            print("\n测试完成!")

    except BleakError as e:
        print(f"连接失败: {e}")
        return False
    except Exception as e:
        print(f"发生错误: {e}")
        return False

    return True


async def main():
    """主函数"""
    print("=" * 60)
    print("ESP32蓝牙客户端测试脚本")
    print("=" * 60)

    # 扫描设备
    devices = await scan_devices(timeout=5.0)

    if not devices:
        print("未找到设备，无法继续测试")
        return

    # 尝试自动查找"The Handy"设备
    handy_devices = []
    for device, adv_data in devices:
        if device.name and "the handy" in device.name.lower():
            handy_devices.append((device, adv_data))

    if len(handy_devices) >= 1:
        # 自动选择第一个"The Handy"设备
        selected_device, _ = handy_devices[0]
        print(
            f"\n自动选择The Handy设备: {selected_device.name} - {selected_device.address}"
        )
        await connect_and_test(selected_device.address, selected_device.name)
    else:
        print("\n未找到The Handy设备，请手动选择")

        # 手动选择模式
        if len(devices) > 0:
            print("\n手动选择设备:")
            for i, (device, adv_data) in enumerate(devices):
                print(f"  {i+1}. {device.name or '未知设备'} - {device.address}")

            try:
                choice = int(input("请选择设备编号 (1-{}): ".format(len(devices))))
                if 1 <= choice <= len(devices):
                    selected_device, _ = devices[choice - 1]
                    await connect_and_test(
                        selected_device.address, selected_device.name
                    )
                else:
                    print("无效的选择")
            except (ValueError, KeyboardInterrupt):
                print("输入无效，退出程序")

    print("\n测试结束")


if __name__ == "__main__":
    # 检查是否安装了bleak库
    try:
        import bleak
    except ImportError:
        print("错误: 未安装bleak库")
        print("请使用以下命令安装: pip install bleak")
        sys.exit(1)

    # 运行主函数
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n用户中断，退出程序")
    except Exception as e:
        print(f"程序运行出错: {e}")
        import traceback

        traceback.print_exc()
