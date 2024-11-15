#!/bin/bash

# 指定源文件、输出文件和路径
SOURCE_FILE="src/CameraPro.cpp"  # 将此名称更改为你的源文件名
OUTPUT_FILE="build/CameraPro"
INCLUDE_PATH="include"  # 根据实际路径调整
LIB_PATH="lib"          # 根据实际路径调整

# 检查源文件是否存在
if [ ! -f "$SOURCE_FILE" ]; then
    echo "源文件 $SOURCE_FILE 不存在，请检查文件路径。"
    exit 1
fi

# 编译命令
g++ "$SOURCE_FILE" -o "$OUTPUT_FILE" \
    -I"$INCLUDE_PATH" \
    -L"$LIB_PATH" \
    -lavcodec -lavformat -lavutil -lswscale -lavdevice -lswresample -lavfilter -lpostproc\
    -lx264 -lx265 -lpthread -lm -ldl -lz \
    -std=c++17

# 检查编译是否成功
if [ $? -eq 0 ]; then
    echo "编译成功！输出文件为 $OUTPUT_FILE"
else
    echo "编译失败，请检查错误信息。"
fi
