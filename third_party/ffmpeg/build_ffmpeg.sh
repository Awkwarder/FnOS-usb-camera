#!/bin/bash

# 指定输出目录
OUTPUT_DIR="output/cpu"

# 运行 FFmpeg 配置
echo "配置 FFmpeg 编译选项..."
./configure --prefix="$OUTPUT_DIR" \
--enable-gpl \
--enable-nonfree \
--enable-libx264 \
--enable-libx265 \
--enable-libv4l2 \
--enable-libpulse \
--enable-shared \
--enable-static \
--enable-libmfx \
--enable-vaapi \
--enable-libdrm \
--disable-stripping \
--enable-postproc \
--extra-cflags="-fPIC"

# 检查配置是否成功
if [ $? -ne 0 ]; then
    echo "配置失败，请检查输出日志以了解详细信息。"
    exit 1
fi

# 编译 FFmpeg
echo "开始编译 FFmpeg..."
make -j$(nproc)

# 检查编译是否成功
if [ $? -ne 0 ]; then
    echo "编译失败，请检查输出日志以了解详细信息。"
    exit 1
fi

# 安装 FFmpeg 到指定目录
echo "安装 FFmpeg 到 $OUTPUT_DIR..."
sudo make install

# 检查安装是否成功
if [ $? -eq 0 ]; then
    echo "FFmpeg 安装完成！安装路径为 $OUTPUT_DIR"
else
    echo "安装失败，请检查输出日志以了解详细信息。"
    exit 1
fi
rm -rf ../../include
cp -r $OUTPUT_DIR/include ../../
rm -rf ../../lib
cp -r $OUTPUT_DIR/lib ../../
