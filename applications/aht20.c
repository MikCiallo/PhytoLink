/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-05      Mik         v1.0
 */

#include "aht20.h"         // AHT20头文件
#include <rtdevice.h>      // RT设备驱动框架
#include <board.h>         // 板级支持包

#define AHT20_ADDR 0x38  // AHT20传感器I2C地址

/* AHT20设备私有数据结构，包含I2C总线指针 */
struct aht20_device {
    struct rt_i2c_bus_device *bus;  // I2C总线设备句柄
};

/**
 * 初始化AHT20温湿度传感器
 *
 * @param i2c_bus_name I2C总线设备名称
 * @return 成功返回设备句柄，失败返回RT_NULL
 */
aht20_device_t aht20_init(const char *i2c_bus_name)
{
    struct aht20_device *dev = RT_NULL;  // 设备结构体指针初始化
    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};  // 初始化命令序列

    RT_ASSERT(i2c_bus_name != RT_NULL);  // 校验输入参数有效性

    // 查找指定的I2C总线设备
    struct rt_i2c_bus_device *i2c_bus = rt_i2c_bus_device_find(i2c_bus_name);
    if (i2c_bus == RT_NULL) {
        return RT_NULL;
    }

    // 分配设备结构体内存
    dev = (struct aht20_device *)rt_malloc(sizeof(struct aht20_device));
    if (dev == RT_NULL) {
        return RT_NULL;
    }
    rt_memset(dev, 0, sizeof(struct aht20_device));  // 内存清零

    dev->bus = i2c_bus;  // 保存I2C总线句柄

    // 发送初始化命令
    if (rt_i2c_master_send(dev->bus, AHT20_ADDR, 0, init_cmd, 3) != 3) {
        rt_free(dev);
        return RT_NULL;
    }

    rt_thread_mdelay(15);  // 等待初始化完成

    return (aht20_device_t)dev;  // 返回设备句柄
}

/**
 * 读取AHT20温湿度数据
 *
 * @param device 设备句柄
 * @param temp 温度存储指针（单位：℃）
 * @param humi 湿度存储指针（单位：%RH）
 * @return 成功返回RT_EOK，失败返回错误码
 */
rt_err_t aht20_read_temperature_humidity(aht20_device_t device, float *temp, float *humi)
{
    struct aht20_device *dev = (struct aht20_device *)device;  // 转换设备句柄

    uint8_t cmd[3] = {0xAC, 0x33, 0x00};  // 触发测量命令
    uint8_t data[6] = {0};  // 数据接收缓冲区

    // 校验输入参数有效性
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(temp != RT_NULL);
    RT_ASSERT(humi != RT_NULL);

    // 发送测量命令
    if (rt_i2c_master_send(dev->bus, AHT20_ADDR, 0, cmd, 3) != 3) {
        return RT_ERROR;
    }

    rt_thread_mdelay(85);  // 等待测量完成

    // 读取测量数据
    if (rt_i2c_master_recv(dev->bus, AHT20_ADDR, 0, data, 6) != 6) {
        return RT_ERROR;
    }

    // 检查传感器状态位
    if ((data[0] & 0x80) == 0x80) {
        return RT_EBUSY;
    }

    // 解析湿度数据（20bit有效数据）
    uint32_t humi_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)(data[3] >> 4));
    *humi = (float)humi_raw / 1048576.0f * 100.0f;  // 转换为相对湿度百分比

    // 解析温度数据（20bit有效数据）
    uint32_t temp_raw = (((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5]);
    *temp = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;  // 转换为摄氏度

    // 数据有效性范围校验
    if (*humi > 100.0f) *humi = 100.0f;
    if (*humi < 0.0f) *humi = 0.0f;

    if (*temp > 85.0f) *temp = 85.0f;
    if (*temp < -40.0f) *temp = -40.0f;

    return RT_EOK;
}

/**
 * 软重置AHT20传感器
 *
 * @param device 设备句柄
 * @return 成功返回RT_EOK，失败返回错误码
 */
rt_err_t aht20_reset(aht20_device_t device)
{
    struct aht20_device *dev = (struct aht20_device *)device;  // 转换设备句柄
    uint8_t reset_cmd = 0xBA;  // 软重置命令

    RT_ASSERT(device != RT_NULL);  // 校验输入参数有效性

    // 发送软重置命令
    if (rt_i2c_master_send(dev->bus, AHT20_ADDR, 0, &reset_cmd, 1) != 1) {
        return RT_ERROR;
    }

    rt_thread_mdelay(25);  // 等待重置完成

    return RT_EOK;
}

/**
 * 释放AHT20设备资源
 *
 * @param device 设备句柄
 */
void aht20_deinit(aht20_device_t device)
{
    if (device != RT_NULL) {
        rt_free(device);  // 释放动态分配的内存
    }
}
