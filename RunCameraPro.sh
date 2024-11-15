#!/bin/bash

# 设置库路径
LIB_PATH="lib"  # 根据实际库文件路径调整

# 检查 LD_LIBRARY_PATH 是否包含库路径
if [[ ":$LD_LIBRARY_PATH:" != *":$LIB_PATH:"* ]]; then
    export LD_LIBRARY_PATH="$LIB_PATH:$LD_LIBRARY_PATH"
fi

# 打印 LD_LIBRARY_PATH 以确认设置正确
echo "当前 LD_LIBRARY_PATH: $LD_LIBRARY_PATH"

# 检查 camera_app 是否存在
if [ ! -f "./build/CameraPro" ]; then
    echo "错误: CameraPro 文件未找到！请检查路径。"
    exit 1
fi

# 运行 camera_app
echo "正在运行 CameraPro..."
./build/CameraPro

# 检查执行结果
if [ $? -eq 0 ]; then
    echo "CameraPro 运行成功！"
else
    echo "CameraPro 运行失败，请检查输出日志。"
fi
