#!/usr/bin/env python3
"""
蓝牙性能测试脚本 - 优化版本
用于测试ESP32蓝牙服务器LED特征值的写入性能和频率
包含多种优化策略和测试模式
"""

import asyncio
import sys
import time
import argparse
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError
from bleak.backends.characteristic import BleakGATTCharacteristic

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


async def performance_test_basic(device_address, device_name=None, test_duration=60):
    """基础性能测试（与原版相同）"""
    print(f"\n=== 基础性能测试 ===")
    print(f"正在连接到设备: {device_name or device_address}")
    print(f"测试持续时间: {test_duration}秒")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取服务
            services = client.services
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if not auto_io_service:
                print(f"未找到自动化IO服务")
                return False

            led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
            if not led_char:
                print(f"未找到LED特征")
                return False

            print(f"找到LED特征，属性: {led_char.properties}")

            # 性能测试
            print(f"\n开始基础性能测试...")
            write_count = 0
            success_count = 0
            error_count = 0
            start_time = time.time()
            last_report_time = start_time
            test_values = [b"\x00", b"\x01"]
            latency_list = []

            try:
                while time.time() - start_time < test_duration:
                    current_time = time.time()
                    value = test_values[write_count % 2]

                    try:
                        write_start = time.time()
                        await client.write_gatt_char(
                            LED_CHAR_UUID, value, response=True
                        )
                        write_end = time.time()

                        write_count += 1
                        success_count += 1
                        write_latency = (write_end - write_start) * 1000
                        latency_list.append(write_latency)

                        if current_time - last_report_time >= 5.0:
                            elapsed_time = current_time - start_time
                            current_frequency = write_count / elapsed_time
                            avg_latency = (
                                sum(latency_list[-100:]) / len(latency_list[-100:])
                                if latency_list
                                else 0
                            )

                            print(
                                f"基础模式 - 已运行 {elapsed_time:.1f}秒 | "
                                f"频率: {current_frequency:.2f} 写入/秒 | "
                                f"最近延迟: {write_latency:.2f}ms | "
                                f"平均延迟: {avg_latency:.2f}ms"
                            )
                            last_report_time = current_time

                    except Exception as e:
                        error_count += 1
                        print(f"写入失败: {e}")
                        await asyncio.sleep(0.1)

                    await asyncio.sleep(0.01)  # 原始延迟

            except KeyboardInterrupt:
                print("\n用户中断测试")

            return calculate_and_report_stats(
                write_count,
                success_count,
                error_count,
                time.time() - start_time,
                latency_list,
                "基础测试",
            )

    except Exception as e:
        print(f"测试失败: {e}")
        return False


async def performance_test_no_delay(device_address, device_name=None, test_duration=60):
    """无延迟性能测试"""
    print(f"\n=== 无延迟性能测试 ===")
    print(f"正在连接到设备: {device_name or device_address}")
    print(f"测试持续时间: {test_duration}秒")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取服务
            services = client.services
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if not auto_io_service:
                print(f"未找到自动化IO服务")
                return False

            led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
            if not led_char:
                print(f"未找到LED特征")
                return False

            print(f"找到LED特征，属性: {led_char.properties}")

            # 性能测试
            print(f"\n开始无延迟性能测试...")
            write_count = 0
            success_count = 0
            error_count = 0
            start_time = time.time()
            last_report_time = start_time
            test_values = [b"\x00", b"\x01"]
            latency_list = []

            try:
                while time.time() - start_time < test_duration:
                    current_time = time.time()
                    value = test_values[write_count % 2]

                    try:
                        write_start = time.time()
                        await client.write_gatt_char(
                            LED_CHAR_UUID, value, response=True
                        )
                        write_end = time.time()

                        write_count += 1
                        success_count += 1
                        write_latency = (write_end - write_start) * 1000
                        latency_list.append(write_latency)

                        if current_time - last_report_time >= 5.0:
                            elapsed_time = current_time - start_time
                            current_frequency = write_count / elapsed_time
                            avg_latency = (
                                sum(latency_list[-100:]) / len(latency_list[-100:])
                                if latency_list
                                else 0
                            )

                            print(
                                f"无延迟模式 - 已运行 {elapsed_time:.1f}秒 | "
                                f"频率: {current_frequency:.2f} 写入/秒 | "
                                f"最近延迟: {write_latency:.2f}ms | "
                                f"平均延迟: {avg_latency:.2f}ms"
                            )
                            last_report_time = current_time

                    except Exception as e:
                        error_count += 1
                        print(f"写入失败: {e}")
                        await asyncio.sleep(0.01)  # 仅在错误时添加小延迟

                    # 移除正常情况下的延迟

            except KeyboardInterrupt:
                print("\n用户中断测试")

            return calculate_and_report_stats(
                write_count,
                success_count,
                error_count,
                time.time() - start_time,
                latency_list,
                "无延迟测试",
            )

    except Exception as e:
        print(f"测试失败: {e}")
        return False


async def performance_test_no_response(
    device_address, device_name=None, test_duration=60
):
    """无响应写入性能测试"""
    print(f"\n=== 无响应写入性能测试 ===")
    print(f"正在连接到设备: {device_name or device_address}")
    print(f"测试持续时间: {test_duration}秒")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取服务
            services = client.services
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if not auto_io_service:
                print(f"未找到自动化IO服务")
                return False

            led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
            if not led_char:
                print(f"未找到LED特征")
                return False

            print(f"找到LED特征，属性: {led_char.properties}")

            # 性能测试
            print(f"\n开始无响应写入性能测试...")
            write_count = 0
            success_count = 0
            error_count = 0
            start_time = time.time()
            last_report_time = start_time
            test_values = [b"\x00", b"\x01"]
            latency_list = []

            try:
                while time.time() - start_time < test_duration:
                    current_time = time.time()
                    value = test_values[write_count % 2]

                    try:
                        write_start = time.time()
                        await client.write_gatt_char(
                            LED_CHAR_UUID, value, response=False
                        )
                        write_end = time.time()

                        write_count += 1
                        success_count += 1
                        write_latency = (write_end - write_start) * 1000
                        latency_list.append(write_latency)

                        if current_time - last_report_time >= 5.0:
                            elapsed_time = current_time - start_time
                            current_frequency = write_count / elapsed_time
                            avg_latency = (
                                sum(latency_list[-100:]) / len(latency_list[-100:])
                                if latency_list
                                else 0
                            )

                            print(
                                f"无响应模式 - 已运行 {elapsed_time:.1f}秒 | "
                                f"频率: {current_frequency:.2f} 写入/秒 | "
                                f"最近延迟: {write_latency:.2f}ms | "
                                f"平均延迟: {avg_latency:.2f}ms"
                            )
                            last_report_time = current_time

                    except Exception as e:
                        error_count += 1
                        print(f"写入失败: {e}")
                        await asyncio.sleep(0.01)

                    # 极小延迟，避免过载
                    await asyncio.sleep(0.001)

            except KeyboardInterrupt:
                print("\n用户中断测试")

            return calculate_and_report_stats(
                write_count,
                success_count,
                error_count,
                time.time() - start_time,
                latency_list,
                "无响应测试",
            )

    except Exception as e:
        print(f"测试失败: {e}")
        return False


async def performance_test_concurrent(
    device_address, device_name=None, test_duration=60, max_concurrent=5
):
    """并发写入性能测试"""
    print(f"\n=== 并发写入性能测试 (最大并发数: {max_concurrent}) ===")
    print(f"正在连接到设备: {device_name or device_address}")
    print(f"测试持续时间: {test_duration}秒")

    try:
        async with BleakClient(device_address, timeout=15.0) as client:
            print(f"连接成功!")
            print(f"设备名称: {client.name}")
            print(f"MTU大小: {client.mtu_size}")

            # 获取服务
            services = client.services
            auto_io_service = services.get_service(AUTO_IO_SERVICE_UUID)
            if not auto_io_service:
                print(f"未找到自动化IO服务")
                return False

            led_char = auto_io_service.get_characteristic(LED_CHAR_UUID)
            if not led_char:
                print(f"未找到LED特征")
                return False

            print(f"找到LED特征，属性: {led_char.properties}")

            # 性能测试
            print(f"\n开始并发写入性能测试...")
            write_count = 0
            success_count = 0
            error_count = 0
            start_time = time.time()
            last_report_time = start_time
            test_values = [b"\x00", b"\x01"]
            latency_list = []
            semaphore = asyncio.Semaphore(max_concurrent)

            async def write_task(value):
                nonlocal write_count, success_count, error_count
                async with semaphore:
                    try:
                        write_start = time.time()
                        await client.write_gatt_char(
                            LED_CHAR_UUID, value, response=True
                        )
                        write_end = time.time()

                        write_count += 1
                        success_count += 1
                        write_latency = (write_end - write_start) * 1000
                        latency_list.append(write_latency)
                        return True
                    except Exception as e:
                        error_count += 1
                        return False

            try:
                while time.time() - start_time < test_duration:
                    current_time = time.time()
                    value = test_values[write_count % 2]

                    # 创建并发任务
                    task = asyncio.create_task(write_task(value))

                    # 等待一小段时间再创建下一个任务
                    await asyncio.sleep(0.005)

                    if current_time - last_report_time >= 5.0:
                        elapsed_time = current_time - start_time
                        current_frequency = write_count / elapsed_time
                        avg_latency = (
                            sum(latency_list[-100:]) / len(latency_list[-100:])
                            if latency_list
                            else 0
                        )

                        print(
                            f"并发模式 - 已运行 {elapsed_time:.1f}秒 | "
                            f"频率: {current_frequency:.2f} 写入/秒 | "
                            f"成功: {success_count} | 失败: {error_count} | "
                            f"平均延迟: {avg_latency:.2f}ms"
                        )
                        last_report_time = current_time

                # 等待所有任务完成
                await task

            except KeyboardInterrupt:
                print("\n用户中断测试")

            return calculate_and_report_stats(
                write_count,
                success_count,
                error_count,
                time.time() - start_time,
                latency_list,
                "并发测试",
            )

    except Exception as e:
        print(f"测试失败: {e}")
        return False


def calculate_and_report_stats(
    write_count, success_count, error_count, total_time, latency_list, test_name
):
    """计算并报告统计信息"""
    if total_time > 0:
        avg_frequency = write_count / total_time
        success_rate = (success_count / write_count * 100) if write_count > 0 else 0
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
    print(f"{test_name} - 性能测试结果:")
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

    return {
        "test_name": test_name,
        "total_time": total_time,
        "write_count": write_count,
        "success_count": success_count,
        "error_count": error_count,
        "avg_frequency": avg_frequency,
        "success_rate": success_rate,
        "min_latency": min_latency,
        "max_latency": max_latency,
        "avg_latency": avg_latency,
        "median_latency": median_latency,
    }


async def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="BLE性能测试脚本 - 优化版本")
    parser.add_argument(
        "--duration", type=int, default=30, help="测试持续时间(秒，默认30)"
    )
    parser.add_argument(
        "--mode",
        type=str,
        default="all",
        choices=["basic", "no-delay", "no-response", "concurrent", "all"],
        help="测试模式: basic(基础), no-delay(无延迟), no-response(无响应), concurrent(并发), all(全部)",
    )
    parser.add_argument(
        "--concurrent", type=int, default=5, help="并发测试的最大并发数(默认5)"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("ESP32蓝牙LED特征值性能测试脚本 - 优化版本")
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
        device_address = selected_device.address
        device_name = selected_device.name
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
                    device_address = selected_device.address
                    device_name = selected_device.name
                else:
                    print("无效的选择")
                    return
            except (ValueError, KeyboardInterrupt):
                print("输入无效，退出程序")
                return

    # 执行测试
    results = []

    if args.mode == "all":
        print(f"\n将依次执行所有测试模式，每个测试{args.duration}秒")
        modes = ["basic", "no-delay", "no-response", "concurrent"]
    else:
        modes = [args.mode]

    for mode in modes:
        print(f"\n{'='*60}")
        print(f"开始执行 {mode} 测试模式")
        print(f"{'='*60}")

        if mode == "basic":
            result = await performance_test_basic(
                device_address, device_name, args.duration
            )
        elif mode == "no-delay":
            result = await performance_test_no_delay(
                device_address, device_name, args.duration
            )
        elif mode == "no-response":
            result = await performance_test_no_response(
                device_address, device_name, args.duration
            )
        elif mode == "concurrent":
            result = await performance_test_concurrent(
                device_address, device_name, args.duration, args.concurrent
            )

        if result:
            results.append(result)

        # 在不同测试之间添加短暂休息
        if mode != modes[-1]:
            print("\n等待5秒后开始下一个测试...")
            await asyncio.sleep(5)

    # 输出对比结果
    if len(results) > 1:
        print(f"\n{'='*80}")
        print("性能测试对比结果")
        print(f"{'='*80}")
        print(
            f"{'测试模式':<15} {'写入频率':<12} {'成功率':<8} {'平均延迟':<10} {'最小延迟':<10} {'最大延迟':<10}"
        )
        print("-" * 80)
        for result in results:
            print(
                f"{result['test_name']:<15} {result['avg_frequency']:<12.2f} "
                f"{result['success_rate']:<8.2f}% {result['avg_latency']:<10.2f} "
                f"{result['min_latency']:<10.2f} {result['max_latency']:<10.2f}"
            )
        print(f"{'='*80}")

    print("\n所有测试结束")


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
