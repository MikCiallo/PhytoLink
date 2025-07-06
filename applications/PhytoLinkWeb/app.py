from flask import Flask, request, jsonify, render_template
import time
import os

app = Flask(__name__, static_folder='static', template_folder='templates')
latest_data = {"temp": 0, "humidity": 0, "light": 0, "update_time": 0}
historical_data = []  # 保存历史数据
MAX_HISTORY = 15  # 最多保留30条历史数据


# 注册模板函数（供前端模板使用，可选）
def get_gauge_percent(value, metric_type):
    ranges = {
        'temp' : (10, 40),  # 温度合理范围
        'humi' : (0, 100),  # 湿度合理范围
        'light': (0, 15000)  # 光照合理范围
    }
    min_val, max_val = ranges.get(metric_type, (0, 100))

    if max_val == min_val:
        return 0

    percent = ((value - min_val) / (max_val - min_val)) * 100
    return max(0, min(100, percent))


app.jinja_env.globals['get_gauge_percent'] = get_gauge_percent


# 传感器数据上传接口（RT-Thread调用）
@app.route('/upload', methods=['GET'])
def receive_data():
    try:
        temp = float(request.args.get('temp', 0))  # 支持小数
        humi = float(request.args.get('humi', 0))
        light = int(request.args.get('light', 0))
        current_time = time.time()

        # 更新最新数据
        latest_data.update({
            "temp"       : temp,
            "humidity"   : humi,
            "light"      : light,
            "update_time": current_time
        })

        # 保存历史数据（最多30条）
        historical_data.append({
            "time"    : current_time,
            "temp"    : temp,
            "humidity": humi,
            "light"   : light
        })
        if len(historical_data) > MAX_HISTORY:
            historical_data.pop(0)  # 移除最旧数据

        return "OK", 200
    except Exception as e:
        return f"Error: {str(e)}", 400


# 历史数据接口（供前端获取）
@app.route('/get_history', methods=['GET'])
def get_history():
    return jsonify(historical_data)


# 实时数据接口（供前端获取）
@app.route('/get_data', methods=['GET'])
def get_data():
    return jsonify(latest_data)


# 时间戳转换过滤器（供模板使用）
@app.template_filter('timestamp_to_datetime')
def timestamp_to_datetime(timestamp, format='%Y-%m-%d %H:%M:%S'):
    return time.strftime(format, time.localtime(timestamp))


# 主页路由
@app.route('/', methods=['GET'])
def index():
    return render_template('index.html')


if __name__ == '__main__':
    # 创建静态目录和模板目录
    for dir_name in ['static', 'templates']:
        if not os.path.exists(dir_name):
            os.makedirs(dir_name)

    # 启动服务
    app.run(host='0.0.0.0', port=8000, debug=True)