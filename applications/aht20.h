/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-05      Mik         v1.0
 */

// 头文件保护，防止重复包含
#ifndef __AHT20_H__
#define __AHT20_H__

// RT核心头文件
#include <rtthread.h>

// C++编译兼容性声明
#ifdef __cplusplus
extern "C" {
#endif

// 定义AHT20设备句柄类型
typedef struct aht20_device *aht20_device_t;

/**
 * 初始化AHT20温湿度传感器
 *
 * @param i2c_bus_name I2C总线名称
 * @return 成功返回设备句柄，失败返回RT_NULL
 */
aht20_device_t aht20_init(const char *i2c_bus_name);

/**
 * 读取AHT20传感器的温度和湿度数据
 *
 * @param dev   设备句柄
 * @param temp  温度值存储地址(°C)
 * @param humi  湿度值存储地址(%)
 * @return 成功返回RT_EOK，失败返回错误码
 */
rt_err_t aht20_read_temperature_humidity(aht20_device_t dev, float *temp, float *humi);

/**
 * 重置AHT20传感器
 *
 * @param dev 设备句柄
 * @return 成功返回RT_EOK，失败返回错误码
 */
rt_err_t aht20_reset(aht20_device_t dev);

/**
 * 释放AHT20设备资源
 *
 * @param dev 设备句柄
 */
void aht20_deinit(aht20_device_t dev);

// 结束C++兼容性声明
#ifdef __cplusplus
}
#endif

// 结束头文件保护
#endif
