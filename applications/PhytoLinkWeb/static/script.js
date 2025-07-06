/**
 * 时间格式化工具
 */
Date.prototype.format = function(fmt) {
    const o = {
        "Y+": this.getFullYear(),
        "M+": this.getMonth() + 1,
        "D+": this.getDate(),
        "H+": this.getHours(),
        "m+": this.getMinutes(),
        "s+": this.getSeconds()
    };
    
    if (/(Y+)/.test(fmt)) {
        fmt = fmt.replace(RegExp.$1, this.getFullYear().toString().substr(4 - RegExp.$1.length));
    }
    
    for (const k in o) {
        if (new RegExp(`(${k})`).test(fmt)) {
            fmt = fmt.replace(RegExp.$1, (RegExp.$1.length === 1) ? (o[k]) : (("00" + o[k]).substr(("" + o[k]).length)));
        }
    }
    
    return fmt;
};

/**
 * 初始化趋势图表（整体概览）
 */
const trendCtx = document.getElementById('trendChart').getContext('2d');
const trendChart = new Chart(trendCtx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            { 
                label: '温度 (°C)', 
                data: [], 
                borderColor: '#ff6b6b', 
                backgroundColor: 'rgba(255, 107, 107, 0.1)',
                fill: false,
                tension: 0.3,
                yAxisID: 'y1'
            },
            { 
                label: '湿度 (%)', 
                data: [], 
                borderColor: '#4ecdc4', 
                backgroundColor: 'rgba(78, 205, 196, 0.1)',
                fill: false,
                tension: 0.3,
                yAxisID: 'y1'
            },
            { 
                label: '光照 (lux)', 
                data: [], 
                borderColor: '#ffe66d', 
                backgroundColor: 'rgba(255, 230, 109, 0.1)',
                fill: false,
                tension: 0.3,
                yAxisID: 'y2'
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
            mode: 'index',
            intersect: false,
        },
        scales: {
            y1: {
                type: 'linear',
                display: true,
                position: 'left',
                label: { text: '温度 (°C) / 湿度 (%)' },
                min: 0,
                max: 100,
                ticks: { stepSize: 20 }
            },
            y2: {
                type: 'linear',
                display: true,
                position: 'right',
                label: { text: '光照 (lux)' },
                min: 0,
                max: 15000,
                ticks: {
                    callback: (value) => value >= 1000 ? `${value/1000}k` : value
                }
            },
            x: {
                // 修改X轴配置：旋转45度防止重叠，最多显示15个刻度
                ticks: { 
                    maxRotation: 45, 
                    minRotation: 45,
                    autoSkip: false, // 禁用自动跳过标签
                    maxTicksLimit: 15 // 最多显示15个刻度
                }
            }
        },
        animation: { duration: 500, easing: 'easeOutQuart' },
        plugins: {
            legend: { position: 'top' },
            tooltip: {
                mode: 'index',
                intersect: false,
                callbacks: {
                    label: (context) => {
                        let label = context.dataset.label || '';
                        if (label) label += ': ';
                        if (context.parsed.y !== null) {
                            label += context.parsed.y.toFixed(context.datasetIndex === 2 ? 0 : 1);
                            label += context.datasetIndex === 0 ? ' °C' : 
                                      context.datasetIndex === 1 ? ' %' : ' lux';
                        }
                        return label;
                    }
                }
            }
        }
    }
});

/**
 * 初始化详细图表（可切换）
 */
const detailCtx = document.getElementById('detailChart').getContext('2d');
const detailChart = new Chart(detailCtx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            { 
                label: '温度 (°C)', 
                data: [], 
                borderColor: '#ff6b6b', 
                backgroundColor: 'rgba(255, 107, 107, 0.1)',
                fill: false,
                tension: 0.3
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
            mode: 'index',
            intersect: false,
        },
        scales: {
            y: {
                type: 'linear',
                display: true,
                position: 'left',
                min: 0,
                max: 50
            },
            x: {
                // 修改X轴配置：旋转45度防止重叠，最多显示15个刻度
                ticks: { 
                    maxRotation: 45, 
                    minRotation: 45,
                    autoSkip: false, // 禁用自动跳过标签
                    maxTicksLimit: 15 // 最多显示15个刻度
                }
            }
        },
        animation: { duration: 500, easing: 'easeOutQuart' },
        plugins: {
            legend: { position: 'top' },
            tooltip: {
                mode: 'index',
                intersect: false,
                callbacks: {
                    label: (context) => {
                        let label = context.dataset.label || '';
                        if (label) label += ': ';
                        if (context.parsed.y !== null) {
                            label += context.parsed.y.toFixed(1);
                            label += context.dataset.label.includes('温度') ? ' °C' : 
                                      context.dataset.label.includes('湿度') ? ' %' : ' lux';
                        }
                        return label;
                    }
                }
            }
        }
    }
});

/**
 * 全局变量
 */
const historicalData = [];
let currentDetailMetric = 'temp'; // 当前详细图表显示的指标

/**
 * 更新状态显示
 */
function updateStatus() {
    const temp = parseFloat(document.getElementById('temp-value').textContent);
    const humi = parseFloat(document.getElementById('humi-value').textContent);
    const light = parseInt(document.getElementById('light-value').textContent);
    const now = new Date();
    const isDaytime = now.getHours() >= 6 && now.getHours() < 18;

    updateElementStatus('temp', temp, {
        normalMin: 18, normalMax: 28,
        warnMin: 10, warnMax: 35,
        dangerMin: 0, dangerMax: 40,
        statusTexts: ['正常', '偏低', '偏高', '极冷', '极热']
    });

    updateElementStatus('humi', humi, {
        normalMin: 40, normalMax: 60,
        warnMin: 20, warnMax: 80,
        dangerMin: 0, dangerMax: 100,
        statusTexts: ['正常', '偏低', '偏高', '过干', '过湿']
    });

    updateElementStatus('light', light, {
        normalMin: isDaytime ? 1000 : 0,
        normalMax: isDaytime ? 5000 : 500,
        warnMin: isDaytime ? 500 : 500,
        warnMax: isDaytime ? 10000 : 1000,
        dangerMin: 0,
        dangerMax: 15000,
        statusTexts: ['正常', '偏暗', '偏亮', '过暗', '过亮']
    });
}

/**
 * 更新单个状态标签
 */
function updateElementStatus(prefix, value, thresholds) {
    const element = document.getElementById(`${prefix}-status`);
    const { normalMin, normalMax, warnMin, warnMax, dangerMin, dangerMax, statusTexts } = thresholds;
    
    if (value >= normalMin && value <= normalMax) {
        element.textContent = statusTexts[0];
        element.className = 'status-tag status-normal';
    } else if ((value >= warnMin && value < normalMin) || (value > normalMax && value <= warnMax)) {
        element.textContent = value < normalMin ? statusTexts[1] : statusTexts[2];
        element.className = 'status-tag status-warning';
    } else {
        element.textContent = value < normalMin ? statusTexts[3] : statusTexts[4];
        element.className = 'status-tag status-danger';
    }
}

/**
 * 更新进度条
 */
function updateProgressBars(data) {
    const tempPercent = getGaugePercent(data.temp, 'temp');
    const humiPercent = getGaugePercent(data.humidity, 'humi');
    const lightPercent = getGaugePercent(data.light, 'light');
    
    document.querySelector('.temp-gauge').style.width = `${tempPercent}%`;
    document.querySelector('.humi-gauge').style.width = `${humiPercent}%`;
    document.querySelector('.light-gauge').style.width = `${lightPercent}%`;
}

/**
 * 计算进度条百分比
 */
function getGaugePercent(value, type) {
    const ranges = {
        temp: [10, 40],
        humi: [0, 100],
        light: [0, 15000]
    };
    const [min, max] = ranges[type] || [0, 100];
    
    if (max === min) return '0.0';
    
    const percent = ((value - min) / (max - min)) * 100;
    return Math.min(100, Math.max(0, percent)).toFixed(1);
}

/**
 * 更新趋势图表
 */
function updateTrendChart() {
    if (historicalData.length === 0) return;
    
    // 修改：显示时分秒格式
    trendChart.data.labels = historicalData.map(item => {
        const d = new Date(item.time * 1000);
        return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
    });
    
    trendChart.data.datasets[0].data = historicalData.map(item => item.temp);
    trendChart.data.datasets[1].data = historicalData.map(item => item.humidity);
    trendChart.data.datasets[2].data = historicalData.map(item => item.light);
    
    trendChart.update();
}

/**
 * 更新详细图表
 */
function updateDetailChart() {
    if (historicalData.length === 0) return;
    
    let dataKey, label, color, min, max;
    
    // 根据当前选择的指标设置图表参数
    switch(currentDetailMetric) {
        case 'temp':
            dataKey = 'temp';
            label = '温度 (°C)';
            color = '#ff6b6b';
            min = Math.min(...historicalData.map(item => item.temp)) - 5;
            max = Math.max(...historicalData.map(item => item.temp)) + 5;
            break;
        case 'humi':
            dataKey = 'humidity';
            label = '湿度 (%)';
            color = '#4ecdc4';
            min = Math.min(...historicalData.map(item => item.humidity)) - 5;
            max = Math.max(...historicalData.map(item => item.humidity)) + 5;
            break;
        case 'light':
            dataKey = 'light';
            label = '光照 (lux)';
            color = '#ffe66d';
            min = Math.min(...historicalData.map(item => item.light)) - 500;
            max = Math.max(...historicalData.map(item => item.light)) + 500;
            break;
    }
    
    // 设置Y轴动态范围
    detailChart.options.scales.y.min = Math.max(0, min); // 确保最小值不小于0
    detailChart.options.scales.y.max = max;
    
    // 修改：显示时分秒格式
    detailChart.data.labels = historicalData.map(item => {
        const d = new Date(item.time * 1000);
        return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
    });
    
    detailChart.data.datasets[0].label = label;
    detailChart.data.datasets[0].data = historicalData.map(item => item[dataKey]);
    detailChart.data.datasets[0].borderColor = color;
    detailChart.data.datasets[0].backgroundColor = `${color}33`; // 带透明度的背景色
    
    detailChart.update();
}

/**
 * 数值变化动画
 */
function animateValue(elementId, targetValue, decimalPlaces) {
    const element = document.getElementById(elementId);
    const currentValue = parseFloat(element.textContent);
    const duration = 500;
    const frameDuration = 1000 / 60;
    const totalFrames = Math.round(duration / frameDuration);
    const valueIncrement = (targetValue - currentValue) / totalFrames;
    
    let currentFrame = 0;
    
    const animate = () => {
        currentFrame++;
        const newValue = currentValue + valueIncrement * currentFrame;
        
        if (currentFrame < totalFrames) {
            element.textContent = newValue.toFixed(decimalPlaces) + (elementId.includes('temp') ? ' °C' : '%');
            requestAnimationFrame(animate);
        } else {
            element.textContent = targetValue.toFixed(decimalPlaces) + (elementId.includes('temp') ? ' °C' : '%');
        }
    };
    
    requestAnimationFrame(animate);
}

/**
 * 获取历史数据
 */
async function fetchHistory() {
    try {
        const res = await fetch('/get_history');
        if (!res.ok) throw new Error(`HTTP错误：${res.status}`);
        
        const data = await res.json();
        historicalData.length = 0;
        historicalData.push(...data);
        
        if (historicalData.length > 0) {
            const latest = historicalData[historicalData.length - 1];
            
            animateValue('temp-value', latest.temp, 1);
            animateValue('humi-value', latest.humidity, 1);
            document.getElementById('light-value').textContent = latest.light + ' lux';
            
            updateProgressBars(latest);
            updateStatus();
            
            updateTrendChart();
            updateDetailChart();
            
            document.getElementById('chart-loading').style.display = 'none';
            document.getElementById('trendChart').style.display = 'block';
            document.getElementById('detail-chart-loading').style.display = 'none';
            document.getElementById('detailChart').style.display = 'block';
        }
    } catch (err) {
        console.error('加载历史数据失败：', err);
        document.getElementById('chart-loading').textContent = '加载历史数据失败，请刷新页面';
        document.getElementById('detail-chart-loading').textContent = '加载详细数据失败，请刷新页面';
    }
}

/**
 * 获取实时数据
 */
async function fetchData() {
    try {
        const res = await fetch('/get_data');
        if (!res.ok) throw new Error(`HTTP错误：${res.status}`);
        
        const data = await res.json();
        
        animateValue('temp-value', data.temp, 1);
        animateValue('humi-value', data.humidity, 1);
        document.getElementById('light-value').textContent = data.light + ' lux';
        
        const date = new Date(data.update_time * 1000);
        document.getElementById('last-update').textContent = `最后更新: ${date.format('YYYY年MM月DD日 HH:mm:ss')}`;
        
        updateProgressBars(data);
        updateStatus();
    } catch (err) {
        console.error('加载实时数据失败：', err);
    }
}

/**
 * 切换详细图表的指标
 */
function switchDetailMetric(metric) {
    currentDetailMetric = metric;
    
    // 更新按钮状态
    document.querySelectorAll('[data-metric]').forEach(btn => {
        if (btn.dataset.metric === metric) {
            btn.classList.add('active');
        } else {
            btn.classList.remove('active');
        }
    });
    
    // 更新图表
    updateDetailChart();
}

/**
 * 页面初始化
 */
document.addEventListener('DOMContentLoaded', () => {
    // 隐藏图表直到数据加载完成
    document.getElementById('trendChart').style.display = 'none';
    document.getElementById('detailChart').style.display = 'none';
    
    // 首次加载数据
    fetchHistory();
    
    // 每秒更新实时数据
    setInterval(fetchData, 1000);
    
    // 每秒更新历史数据
    setInterval(fetchHistory, 1000);
    
    // 绑定指标切换按钮事件
    document.querySelectorAll('[data-metric]').forEach(btn => {
        btn.addEventListener('click', () => {
            switchDetailMetric(btn.dataset.metric);
        });
    });
});