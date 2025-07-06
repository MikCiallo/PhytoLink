/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-05      Mik         v1.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include <drv_lcd.h>
#include <rttlogo.h>
#include "aht20.h"  // AHT20驱动
#include "ap3216c.h"  // AP3216C驱动

/* 网络相关头文件 */
#include <wlan_mgnt.h>
#include <wlan_prot.h>
#include <wlan_cfg.h>
#include <msh.h>

/* WebClient网络客户端头文件 */
#include <webclient.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 线程参数定义 */
#define THREAD_PRIORITY   25         // 线程优先级
#define THREAD_STACK_SIZE 1024       // 线程栈大小
#define THREAD_TIMESLICE  5          // 线程时间片

/* 设备句柄定义 */
static aht20_device_t aht20_dev;    // AHT20设备句柄
static ap3216c_device_t ap3216c_dev; // AP3216C设备句柄

/* 全局变量：存储传感器数据 */
static float g_temperature = 0.0f;   // 温度数据
static float g_humidity = 0.0f;      // 湿度数据
static float g_brightness = 0.0f;     // 光照强度数据
static rt_mutex_t g_sensor_mutex = RT_NULL;  // 保护传感器数据的互斥锁

/* 网络相关变量 */
static struct rt_semaphore net_ready;  // 网络就绪信号量
static int network_connected = 0;      // 网络连接状态标志
static rt_mutex_t net_state_mutex = RT_NULL;  // 保护网络状态的互斥锁

/* HTTP上传配置 */
#define UPLOAD_INTERVAL   1000    // 上传间隔1秒
#define SERVER_IP         "192.168.90.106"  // 本地服务器IP
#define SERVER_PORT       8000             // 服务器端口
#define UPLOAD_PATH       "/upload"        // 上传接口路径

/**
 * HTTP上传线程入口函数
 *
 * @param parameter 线程参数
 */
static void http_upload_thread_entry(void *parameter)
{
    char url[256];                          // URL缓冲区
    struct webclient_session *session = RT_NULL;  // 网络会话句柄
    int response_status = 0;                // 响应状态码
    char response_buffer[1024] = {0};       // 响应数据缓冲区
    int upload_attempts = 0;                // 上传尝试次数

    rt_kprintf("[HTTP] Upload thread started\n");

    while (1) {
        /* 等待网络连接就绪 */
        rt_mutex_take(net_state_mutex, RT_WAITING_FOREVER);
        int connected = network_connected;
        rt_mutex_release(net_state_mutex);

        if (connected) {
            /* 保护读取传感器数据 */
            rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER);
            int temp = (int)g_temperature;
            int humi = (int)g_humidity;
            int light = (int)g_brightness;
            rt_mutex_release(g_sensor_mutex);

            /* 构造完整的GET请求URL */
            rt_snprintf(url, sizeof(url),
                       "http://%s:%d%s?temp=%d&humi=%d&light=%d",
                       SERVER_IP, SERVER_PORT, UPLOAD_PATH, temp, humi, light);

            rt_kprintf("[HTTP] Uploading to: %s\n", url);

            /* 创建会话 */
            session = webclient_session_create(1024);
            if (session == RT_NULL) {
                rt_kprintf("[HTTP] Failed to create session, retry in 1s\n");
                rt_thread_mdelay(1000);
                continue;
            }

            /* 设置超时时间 */
            webclient_set_timeout(session, 5000);  // 5秒超时

            /* 发送GET请求 */
            response_status = webclient_get(session, url);

            /* 处理响应 */
            if (response_status == 200) {
                /* 读取响应内容 */
                int read_len = webclient_read(session, response_buffer, sizeof(response_buffer) - 1);
                if (read_len > 0) {
                    response_buffer[read_len] = '\0';
                    rt_kprintf("[HTTP] Response: %s\n", response_buffer);
                    if (strstr(response_buffer, "OK") != NULL) {
                        rt_kprintf("[HTTP] Upload success!\n");
                        upload_attempts = 0;  // 重置尝试次数
                    }
                } else {
                    rt_kprintf("[HTTP] Upload success, empty response\n");
                    upload_attempts = 0;  // 空响应视为成功
                }
            } else {
                rt_kprintf("[HTTP] Upload failed, status: %d\n", response_status);
                upload_attempts++;
            }

            /* 关闭会话 */
            webclient_close(session);
            session = RT_NULL;

            /* 指数退避重试策略 */
            if (upload_attempts > 0) {
                int backoff_time = 1000 * (1 << (upload_attempts > 5 ? 5 : upload_attempts));
                rt_kprintf("[HTTP] Retry attempt %d, waiting %d ms...\n",
                          upload_attempts, backoff_time);
                rt_thread_mdelay(backoff_time);
            } else {
                // 上传成功后等待固定间隔（1秒）
                rt_thread_mdelay(UPLOAD_INTERVAL);
            }
        } else {
            rt_kprintf("[HTTP] Network not ready, waiting...\n");
            rt_thread_mdelay(5000);
        }
    }
}

/**
 * 在LCD上显示传感器数据和网络状态
 */
static void display_sensor_data(void)
{
    char temp_str[30];      // 温度显示字符串
    char humi_str[30];      // 湿度显示字符串
    char light_str[30];     // 光照显示字符串
    char net_str[30];       // 网络状态显示字符串
    int connected_state;    // 网络连接状态

    /* 加锁保护共享数据 */
    rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER);

    /* 格式化温湿度数据 */
    rt_snprintf(temp_str, sizeof(temp_str), "Temp(C): %5d", (int)g_temperature);
    rt_snprintf(humi_str, sizeof(humi_str), "Humi(%%): %5d", (int)g_humidity);
    rt_snprintf(light_str, sizeof(light_str), "Light(lux): %5d", (int)g_brightness);

    /* 释放锁 */
    rt_mutex_release(g_sensor_mutex);

    /* 获取网络连接状态 */
    rt_mutex_take(net_state_mutex, RT_WAITING_FOREVER);
    connected_state = network_connected;
    rt_mutex_release(net_state_mutex);

    /* 设置显示颜色 */
    lcd_set_color(WHITE, BLACK);

    /* 显示温湿度数据 */
    lcd_show_string(10, 120, 24, temp_str);
    lcd_show_string(10, 150, 24, humi_str);
    lcd_show_string(10, 180, 24, light_str);

    /* 显示网络状态 */
    if (connected_state) {
        rt_snprintf(net_str, sizeof(net_str), "NET:    CONNECTED");
        lcd_set_color(GREEN, BLACK);
    } else {
        rt_snprintf(net_str, sizeof(net_str), "NET: DISCONNECTED");
        lcd_set_color(RED, BLACK);
    }
    lcd_show_string(10, 210, 24, net_str);
}

/**
 * AHT20数据读取线程入口函数
 *
 * @param parameter 线程参数
 */
static void aht20_read_thread_entry(void *parameter)
{
    rt_err_t result;  // 函数返回值

    rt_kprintf("[AHT20] Initializing...\n");
    aht20_dev = aht20_init("i2c3");
    if (aht20_dev == RT_NULL) {
        rt_kprintf("[AHT20] Initialization failed\n");
        return;
    }
    rt_kprintf("[AHT20] Initialization successful\n");

    while (1) {
        rt_kprintf("[AHT20] Reading data...\n");

        // 读取传感器数据
        result = aht20_read_temperature_humidity(aht20_dev, &g_temperature, &g_humidity);

        if (result == RT_EOK) {
            /* 读取成功，更新显示 */
            rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER);
            rt_kprintf("[AHT20] Temperature: %d C, Humidity: %d %%\n",
                       (int)g_temperature, (int)g_humidity);
            display_sensor_data();
            rt_mutex_release(g_sensor_mutex);
        } else {
            rt_kprintf("[AHT20] Read failed (error code: %d), resetting...\n", result);
            aht20_reset(aht20_dev);

            /* 显示错误信息 */
            rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER);
            lcd_set_color(WHITE, RED);
            lcd_show_string(10, 120, 24, "Sensor Error!");
            rt_mutex_release(g_sensor_mutex);
        }

        // 统一为1秒读取一次
        rt_thread_mdelay(1000);
    }
}

/**
 * AP3216C数据读取线程入口函数
 *
 * @param parameter 线程参数
 */
static void ap3216c_read_thread_entry(void *parameter)
{
    rt_kprintf("[AP3216C] Initializing...\n");
    ap3216c_dev = ap3216c_init("i2c2");  // 根据实际I2C总线修改
    if (ap3216c_dev == RT_NULL) {
        rt_kprintf("[AP3216C] Initialization failed\n");
        return;
    }
    rt_kprintf("[AP3216C] Initialization successful\n");

    while (1) {
        float brightness = ap3216c_read_ambient_light(ap3216c_dev);

        if (brightness >= 0) {  // 正数表示有效数据
            // 加锁保护共享变量
            rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER);
            g_brightness = brightness;
            rt_kprintf("[AP3216C] Ambient light: %d lux\n", (int)g_brightness);
            display_sensor_data();  // 更新显示
            rt_mutex_release(g_sensor_mutex);
        } else {
            rt_kprintf("[AP3216C] Read failed\n");
        }

        // 保持1秒读取间隔
        rt_thread_mdelay(1000);
    }
}

/**
 * 网络就绪事件回调函数
 *
 * @param event 事件类型
 * @param buff 事件缓冲区
 * @param parameter 用户参数
 */
void wlan_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_sem_release(&net_ready);

    /* 保护网络状态变量 */
    rt_mutex_take(net_state_mutex, RT_WAITING_FOREVER);
    network_connected = 1;
    rt_mutex_release(net_state_mutex);

    display_sensor_data();  // 更新网络状态显示

    // 测试网络连通性（修改：使用SERVER_IP而非SERVER_DOMAIN）
    char ping_cmd[128];
    rt_snprintf(ping_cmd, sizeof(ping_cmd), "ping %s", SERVER_IP);
    msh_exec(ping_cmd, rt_strlen(ping_cmd));
}

/**
 * 断开连接事件回调函数
 *
 * @param event 事件类型
 * @param buff 事件缓冲区
 * @param parameter 用户参数
 */
void wlan_station_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_kprintf("Network disconnected!\n");

    /* 保护网络状态变量 */
    rt_mutex_take(net_state_mutex, RT_WAITING_FOREVER);
    network_connected = 0;
    rt_mutex_release(net_state_mutex);

    display_sensor_data();  // 更新网络状态显示
}

/**
 * 连接成功事件回调函数
 *
 * @param event 事件类型
 * @param buff 事件缓冲区
 * @param parameter 用户参数
 */
static void wlan_connect_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_kprintf("%s\n", __FUNCTION__);
    if ((buff != RT_NULL) && (buff->len == sizeof(struct rt_wlan_info)))
    {
        rt_kprintf("Connected to SSID : %s \n", ((struct rt_wlan_info *)buff->data)->ssid.val);
    }
}

/**
 * 连接失败事件回调函数
 *
 * @param event 事件类型
 * @param buff 事件缓冲区
 * @param parameter 用户参数
 */
static void wlan_connect_fail_handler(int event, struct rt_wlan_buff *buff, void *parameter)
{
    rt_kprintf("%s\n", __FUNCTION__);
    if ((buff != RT_NULL) && (buff->len == sizeof(struct rt_wlan_info)))
    {
        rt_kprintf("Failed to connect SSID : %s \n", ((struct rt_wlan_info *)buff->data)->ssid.val);
    }
}

/**
 * 显示WLAN信息函数
 *
 * @param info WLAN信息结构体
 * @param index 显示索引
 */
static void print_wlan_information(struct rt_wlan_info *info, int index)
{
    char *security;  // 安全模式字符串

    if (index == 0)
    {
        rt_kprintf("             SSID                      MAC            security    rssi chn Mbps\n");
        rt_kprintf("------------------------------- -----------------  -------------- ---- --- ----\n");
    }

    {
        rt_kprintf("%-32.32s", &(info->ssid.val[0]));
        rt_kprintf("%02x:%02x:%02x:%02x:%02x:%02x  ",
                info->bssid[0], info->bssid[1], info->bssid[2],
                info->bssid[3], info->bssid[4], info->bssid[5]);
        switch (info->security)
        {
        case SECURITY_OPEN: security = "OPEN"; break;
        case SECURITY_WEP_PSK: security = "WEP_PSK"; break;
        case SECURITY_WEP_SHARED: security = "WEP_SHARED"; break;
        case SECURITY_WPA_TKIP_PSK: security = "WPA_TKIP_PSK"; break;
        case SECURITY_WPA_AES_PSK: security = "WPA_AES_PSK"; break;
        case SECURITY_WPA2_AES_PSK: security = "WPA2_AES_PSK"; break;
        case SECURITY_WPA2_TKIP_PSK: security = "WPA2_TKIP_PSK"; break;
        case SECURITY_WPA2_MIXED_PSK: security = "WPA2_MIXED_PSK"; break;
        case SECURITY_WPS_OPEN: security = "WPS_OPEN"; break;
        case SECURITY_WPS_SECURE: security = "WPS_SECURE"; break;
        default: security = "UNKNOWN"; break;
        }
        rt_kprintf("%-14.14s ", security);
        rt_kprintf("%-4d ", info->rssi);
        rt_kprintf("%3d ", info->channel);
        rt_kprintf("%4d\n", info->datarate / 1000000);
    }
}

/**
 * 主函数：系统初始化与多线程启动
 *
 * @return 程序退出状态
 */
int main(void)
{
    rt_thread_t aht20_tid, ap3216c_tid, http_tid;  // 线程ID
    struct rt_wlan_info info;                     // WLAN信息结构体
    rt_err_t result;                              // 函数返回值

    /* 初始化LCD */
    lcd_clear(WHITE);

    /* 设置背景色和前景色 */
    lcd_set_color(WHITE, BLACK);

    /* 显示RT-Thread logo */
    lcd_show_image(0, 0, 240, 69, image_rttlogo);

    /* 在LCD上显示标题 */
    lcd_show_string(10, 69, 16, "Hello, World!");
    lcd_show_string(10, 69 + 16, 24, "Sensors Monitoring:");

    /* 绘制分隔线 */
    lcd_draw_line(0, 69 + 16 + 24, 240, 69 + 16 + 24);

    /* 创建保护共享数据的互斥锁 */
    g_sensor_mutex = rt_mutex_create("sensor_mutex", RT_IPC_FLAG_FIFO);
    if (g_sensor_mutex == RT_NULL) {
        rt_kprintf("Failed to create mutex!\n");
        return -1;
    }

    /* 创建保护网络状态的互斥锁 */
    net_state_mutex = rt_mutex_create("net_mutex", RT_IPC_FLAG_FIFO);
    if (net_state_mutex == RT_NULL) {
        rt_kprintf("Failed to create network mutex!\n");
        return -1;
    }

    /* 创建AHT20读取线程 */
    aht20_tid = rt_thread_create("aht20",
                                aht20_read_thread_entry,
                                RT_NULL,
                                THREAD_STACK_SIZE,
                                THREAD_PRIORITY,
                                THREAD_TIMESLICE);

    /* 创建AP3216C读取线程 */
    ap3216c_tid = rt_thread_create("ap3216c",
                                  ap3216c_read_thread_entry,
                                  RT_NULL,
                                  THREAD_STACK_SIZE,
                                  THREAD_PRIORITY,
                                  THREAD_TIMESLICE);

    /* 创建HTTP上传线程（增大堆栈到8192字节） */
    http_tid = rt_thread_create("http_upload",
                               http_upload_thread_entry,
                               RT_NULL,
                               8192,  // 增大堆栈空间，确保TLS有足够内存
                               THREAD_PRIORITY + 2,  // 稍低优先级
                               THREAD_TIMESLICE);

    /* 启动线程 */
    if (aht20_tid != RT_NULL) {
        rt_thread_startup(aht20_tid);
    } else {
        rt_kprintf("[MAIN] AHT20 thread startup failed\n");
    }

    if (ap3216c_tid != RT_NULL) {
        rt_thread_startup(ap3216c_tid);
    } else {
        rt_kprintf("[MAIN] AP3216C thread startup failed\n");
    }

    if (http_tid != RT_NULL) {
        rt_thread_startup(http_tid);
    } else {
        rt_kprintf("[MAIN] HTTP upload thread startup failed\n");
    }

    /* 初始化网络同步信号量 */
    rt_sem_init(&net_ready, "net_ready", 0, RT_IPC_FLAG_FIFO);

    /* 注册网络事件回调函数 */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wlan_ready_handler, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wlan_station_disconnect_handler, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED, wlan_connect_handler, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED_FAIL, wlan_connect_fail_handler, RT_NULL);

    /* 等待500ms以便WiFi完成初始化 */
    rt_thread_mdelay(500);

    /* 连接WiFi热点 */
    rt_kprintf("Connecting to AP: iQOO Neo9\n");
    result = rt_wlan_connect("iQOO Neo9", "1234567890");
    if (result == RT_EOK) {
        rt_memset(&info, 0, sizeof(struct rt_wlan_info));
        /* 获取当前连接热点信息 */
        rt_wlan_get_info(&info);
        rt_kprintf("Connected to AP information:\n");
        print_wlan_information(&info, 0);

        /* 等待成功获取IP */
        result = rt_sem_take(&net_ready, rt_tick_from_millisecond(15000));
        if (result == RT_EOK) {
            rt_kprintf("Network ready!\n");
            msh_exec("ifconfig", rt_strlen("ifconfig"));
        } else {
            rt_kprintf("Timeout waiting for IP!\n");
        }
    } else {
        rt_kprintf("Failed to connect AP!\n");
    }

    /* 主线程循环 */
    while (1) {
        /* 每5秒检查一次网络连接状态 */
        int connected;
        rt_mutex_take(net_state_mutex, RT_WAITING_FOREVER);
        connected = network_connected;
        rt_mutex_release(net_state_mutex);

        if (!connected) {
            rt_kprintf("Attempting to reconnect to AP...\n");
            rt_wlan_connect("iQOO Neo9", "1234567890");
        }
        rt_thread_mdelay(5000);
    }

    return 0;
}
