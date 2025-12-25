#!/usr/bin/env python3
"""
蓝牙性能测试脚本
用于测试ESP32蓝牙服务器LED特征值的写入性能和频率
"""

import asyncio
import sys
import time
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# 从用户要求中提取的UUID
AUTO_IO_SERVICE_UUID = "00001815-0000-1000-8000-00805f9b34fb"  # Automation IO Service
LED_CHAR_UUID = "00001525-1212-efde-1523-785feabcd123"  # LED Characteristic


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


async def performance_test(device_address, device_name=None, test_duration=60):
    """连接到设备并进行LED特征值写入性能测试"""
    print(f"\n正在连接到设备: {device_name or device_address}")
    print(f"测试持续时间: {test_duration}秒")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取所有服务
            services = client.services
            services_list = list(services)
            print(f"\n发现 {len(services_list)} 个服务:")

            # 查找自动化IO服务
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if not auto_io_service:
                print(f"未找到自动化IO服务 ({AUTO_IO_SERVICE_UUID})")
                return False

            print(f"找到自动化IO服务 ({AUTO_IO_SERVICE_UUID})")

            # 检查LED特征
            led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
            if not led_char:
                print(f"未找到LED特征 ({LED_CHAR_UUID})")
                return False

            print(f"找到LED特征 ({LED_CHAR_UUID})")
            print(f"特征属性: {led_char.properties}")

            if "write" not in led_char.properties:
                print("LED特征不支持写入操作")
                return False

            # 开始性能测试
            print(f"\n开始LED特征值写入性能测试...")
            print("按Ctrl+C可提前结束测试")

            # 性能统计变量
            write_count = 0
            success_count = 0
            error_count = 0
            start_time = time.time()
            last_report_time = start_time
            test_values = [b"\x00", b"\x01"]  # LED关和开
            latency_list = []  # 存储每次写入的延迟

            try:
                while time.time() - start_time < test_duration:
                    current_time = time.time()

                    # 交替写入0和1来测试LED开关
                    value = test_values[write_count % 2]

                    try:
                        write_start = time.time()
                        await client.write_gatt_char(
                            LED_CHAR_UUID, value, response=True
                        )
                        write_end = time.time()

                        write_count += 1
                        success_count += 1

                        # 计算单次写入延迟
                        write_latency = (write_end - write_start) * 1000  # 转换为毫秒
                        latency_list.append(write_latency)

                        # 每5秒报告一次统计信息
                        if current_time - last_report_time >= 5.0:
                            elapsed_time = current_time - start_time
                            current_frequency = write_count / elapsed_time
                            avg_latency = (
                                sum(latency_list[-100:]) / len(latency_list[-100:])
                                if latency_list
                                else 0
                            )  # 最近100次的平均延迟

                            print(
                                f"已运行 {elapsed_time:.1f}秒 | "
                                f"总写入次数: {write_count} | "
                                f"成功: {success_count} | "
                                f"失败: {error_count} | "
                                f"当前频率: {current_frequency:.2f} 写入/秒 | "
                                f"最近延迟: {write_latency:.2f}ms | "
                                f"平均延迟: {avg_latency:.2f}ms"
                            )

                            last_report_time = current_time

                    except Exception as e:
                        error_count += 1
                        print(f"写入失败 (第{write_count + 1}次): {e}")
                        # 短暂延迟后继续
                        await asyncio.sleep(0.1)

                    # 短暂延迟以避免过快的请求
                    await asyncio.sleep(0.01)

            except KeyboardInterrupt:
                print("\n用户中断测试")

            # 计算最终统计信息
            total_time = time.time() - start_time
            if total_time > 0:
                avg_frequency = write_count / total_time
                success_rate = (
                    (success_count / write_count * 100) if write_count > 0 else 0
                )
            else:
                avg_frequency = 0
                success_rate = 0

            # 计算延迟统计
            if latency_list:
                min_latency = min(latency_list)
                max_latency = max(latency_list)
                avg_latency = sum(latency_list) / len(latency_list)
                # 计算中位数
                sorted_latencies = sorted(latency_list)
                n = len(sorted_latencies)
                if n % 2 == 0:
                    median_latency = (
                        sorted_latencies[n // 2 - 1] + sorted_latencies[n // 2]
                    ) / 2
                else:
                    median_latency = sorted_latencies[n // 2]
            else:
                min_latency = max_latency = avg_latency = median_latency = 0

            print("\n" + "=" * 60)
            print("性能测试结果:")
            print(f"测试总时间: {total_time:.2f}秒")
            print(f"总写入次数: {write_count}")
            print(f"成功写入次数: {success_count}")
            print(f"失败写入次数: {error_count}")
            print(f"平均写入频率: {avg_frequency:.2f} 写入/秒")
            print(f"成功率: {success_rate:.2f}%")
            print(f"延迟统计:")
            print(f"  最小延迟: {min_latency:.2f}ms")
            print(f"  最大延迟: {max_latency:.2f}ms")
            print(f"  平均延迟: {avg_latency:.2f}ms")
            print(f"  中位数延迟: {median_latency:.2f}ms")
            print("=" * 60)

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
    print("ESP32蓝牙LED特征值性能测试脚本")
    print("=" * 60)

    # 获取测试参数
    try:
        test_duration = int(input("请输入测试持续时间(秒，默认60): ") or "60")
    except ValueError:
        test_duration = 60
        print("使用默认测试时间: 60秒")

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
        await performance_test(
            selected_device.address, selected_device.name, test_duration
        )
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
                    await performance_test(
                        selected_device.address, selected_device.name, test_duration
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
