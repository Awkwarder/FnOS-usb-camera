#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <filesystem> // C++17 需要支持
#include <thread>

namespace fs = std::filesystem;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
}

// 获取当前时间并生成带路径的文件名
std::string get_output_directory_and_filename() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::gmtime(&currentTime);
    localTime->tm_hour += 8; // 转换为北京时间
    std::mktime(localTime);

    // 基础路径
    std::string basePath = "/vol1/1000/Videos/Capture";

    // 格式化输出路径 "year/month/day/filename"
    std::ostringstream directoryStream;
    directoryStream << basePath << "/" << std::put_time(localTime, "%Y/%m/%d");
    std::string directory = directoryStream.str();
    fs::create_directories(directory); // 创建目录（如果不存在）

    // 生成文件名
    std::ostringstream filenameStream;
    filenameStream << directory << "/Capture_" << std::put_time(localTime, "%Y%m%d_%H%M%S") << ".h265";
    return filenameStream.str();
}

int capture() {
    avdevice_register_all();
    AVFormatContext* inputFormatContext = nullptr;
    const AVInputFormat* inputFormat = av_find_input_format("video4linux2");
    AVDictionary* options = nullptr;
    av_dict_set(&options, "pixel_format", "mjpeg", 0);
    av_dict_set(&options, "video_size", "2560x1440", 0);
    int ret = avformat_open_input(&inputFormatContext, "/dev/video0", inputFormat, &options);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法打开摄像头！错误: " << errbuf << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        std::cerr << "无法找到流信息！" << std::endl;
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1) {
        std::cerr << "没有找到视频流！" << std::endl;
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    const AVCodec* decoder = avcodec_find_decoder(inputFormatContext->streams[videoStreamIndex]->codecpar->codec_id);
    AVCodecContext* decoderContext = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoderContext, inputFormatContext->streams[videoStreamIndex]->codecpar);
    if (avcodec_open2(decoderContext, decoder, nullptr) < 0) {
        std::cerr << "无法打开解码器！" << std::endl;
        avcodec_free_context(&decoderContext);
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H265);
    AVCodecContext* encoderContext = avcodec_alloc_context3(encoder);
    encoderContext->bit_rate = 3000000;
    encoderContext->width = decoderContext->width;
    encoderContext->height = decoderContext->height;
    encoderContext->time_base = {1, 30};  // 30 FPS
    encoderContext->framerate = {30, 1};
    encoderContext->gop_size = 100;
    encoderContext->max_b_frames = 0;
    encoderContext->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(encoderContext, encoder, nullptr) < 0) {
        std::cerr << "无法打开编码器！" << std::endl;
        avcodec_free_context(&encoderContext);
        avcodec_free_context(&decoderContext);
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    AVFormatContext* outputFormatContext = nullptr;
    AVStream* outStream = nullptr;
    auto open_output_file = [&](const std::string& fname) {
        if (outputFormatContext) {
            av_write_trailer(outputFormatContext);
            avio_closep(&outputFormatContext->pb);
            avformat_free_context(outputFormatContext);
        }
        avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, fname.c_str());
        if (!outputFormatContext) {
            std::cerr << "无法创建输出上下文！" << std::endl;
            return false;
        }
        outStream = avformat_new_stream(outputFormatContext, nullptr);
        avcodec_parameters_from_context(outStream->codecpar, encoderContext);
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, fname.c_str(), AVIO_FLAG_WRITE) < 0) {
                std::cerr << "无法打开输出文件！" << std::endl;
                return false;
            }
        }
        avformat_write_header(outputFormatContext, nullptr);
        return true;
    };

    std::string filename = get_output_directory_and_filename();
    if (!open_output_file(filename)) {
        avcodec_free_context(&encoderContext);
        avcodec_free_context(&decoderContext);
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    AVPacket* encodedPacket = av_packet_alloc();
    struct SwsContext* swsCtx = sws_getContext(decoderContext->width, decoderContext->height, decoderContext->pix_fmt,
                                               encoderContext->width, encoderContext->height, AV_PIX_FMT_YUV420P,
                                               SWS_BILINEAR, nullptr, nullptr, nullptr);

    while (true) {
	ret = av_read_frame(inputFormatContext,packet);
	if (ret < 0){
	     	char errbuf[128];
        	av_strerror(ret, errbuf, sizeof(errbuf));
        	std::cerr << "av_read_frame error " << errbuf << std::endl;
	}
        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                if (avcodec_send_packet(decoderContext, packet) == 0) {
                    while (avcodec_receive_frame(decoderContext, frame) == 0) {
                        AVFrame* yuvFrame = av_frame_alloc();
                        yuvFrame->format = encoderContext->pix_fmt;
                        yuvFrame->width = encoderContext->width;
                        yuvFrame->height = encoderContext->height;
                        av_image_alloc(yuvFrame->data, yuvFrame->linesize, encoderContext->width, encoderContext->height, encoderContext->pix_fmt, 32);
                        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, yuvFrame->data, yuvFrame->linesize);
                        yuvFrame->pts = frame->pts;
                        if (avcodec_send_frame(encoderContext, yuvFrame) == 0) {
                            while (avcodec_receive_packet(encoderContext, encodedPacket) == 0) {
                                if (av_interleaved_write_frame(outputFormatContext, encodedPacket) < 0) {
                                    std::cerr << "写入数据包时出错！" << std::endl;
                                }
                                av_packet_unref(encodedPacket);
                            }
                        }
                        av_freep(&yuvFrame->data[0]);
                        av_frame_free(&yuvFrame);
                    }
                }
            }
            av_packet_unref(packet);

            // 检查是否到达整点
	    auto now = std::chrono::system_clock::now();
	    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
            if (currentTime % 3600 == 0) {
                filename = get_output_directory_and_filename();
                if (!open_output_file(filename)) {
                    break;
                }
            }
        }
    }

    av_write_trailer(outputFormatContext);
    av_packet_free(&packet);
    av_packet_free(&encodedPacket);
    av_frame_free(&frame);
    avcodec_free_context(&decoderContext);
    avcodec_free_context(&encoderContext);
    avformat_close_input(&inputFormatContext);
    avformat_free_context(outputFormatContext);
    sws_freeContext(swsCtx);

    return 0;
}

int main(){
    int index = 0;
    while(1){
    	capture();
	std::cout << index <<" capture" << std::endl; 
	std::this_thread::sleep_for(std::chrono::minutes(15));
    }
    std::cout <<"fail" << std::endl;
    return 0;
}
