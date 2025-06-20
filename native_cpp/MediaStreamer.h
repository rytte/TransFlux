#ifndef MEDIASTREAMER_H
#define MEDIASTREAMER_H

// Cross-platform Minimal RTSP Server (Windows + Linux)

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SocketType;
#define CLOSESOCKET closesocket
#define usleep(x) Sleep((x) / 1000)
#else
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int SocketType;
#define CLOSESOCKET close
#endif

#include <iostream>
#include <thread>
#include <cstring>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

#define RTP_TCP_CHANNEL 0

class RTSPServer {
public:
    RTSPServer(int port) : port_(port), server_fd_(-1) {
        av_log_set_level(AV_LOG_QUIET);
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~RTSPServer() {
        if (server_fd_ != -1) CLOSESOCKET(server_fd_);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        bind(server_fd_, (sockaddr*)&addr, sizeof(addr));
        listen(server_fd_, 4);

        std::cout << "RTSP server started on port " << port_ << std::endl;

        while (true) {
            SocketType client_fd = accept(server_fd_, nullptr, nullptr);
            std::cout << "Client connected." << std::endl;
            std::thread(&RTSPServer::handle_session, this, client_fd).detach();
        }
    }

private:
    int port_;
    SocketType server_fd_;

    std::string generate_sdp(const AVCodecParameters* codecpar, int payload_type = 96) {
        std::ostringstream sdp;
        sdp << "v=0\r\n";
        sdp << "o=- 0 0 IN IP4 127.0.0.1\r\n";
        sdp << "s=H264 stream\r\n";
        sdp << "c=IN IP4 0.0.0.0\r\n";
        sdp << "t=0 0\r\n";
        sdp << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        sdp << "a=rtpmap:" << payload_type << " H264/90000\r\n";
        if (codecpar && codecpar->extradata) {
            std::string sprop = extract_sprop_from_extradata(codecpar->extradata, codecpar->extradata_size);
            if (!sprop.empty()) {
                sdp << "a=fmtp:" << payload_type << " packetization-mode=1; " << sprop << "\r\n";
            }
        }
        sdp << "a=control:track1\r\n";
        return sdp.str();
    }

    void send_response(SocketType client_fd, const std::string& response) {
        send(client_fd, response.c_str(), (int)response.size(), 0);
    }

    void handle_session(SocketType client_fd) {
        char buffer[4096];
        std::string session_id = "12345678";

        AVFormatContext* fmt_ctx = nullptr;
        if (avformat_open_input(&fmt_ctx, "../media/1.mp4", nullptr, nullptr) < 0) {
            std::cerr << "Failed to open media file\n";
            return;
        }
        avformat_find_stream_info(fmt_ctx, nullptr);
        int video_stream_index = -1;
        for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        std::string sdp = generate_sdp(fmt_ctx->streams[video_stream_index]->codecpar) + "\r\n";

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int len = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) break;
            std::string request(buffer);
            std::cout << "Received:\n" << request << std::endl;

            size_t cseq_pos = request.find("CSeq:");
            std::string cseq = "1";
            if (cseq_pos != std::string::npos) {
                size_t end = request.find("\r\n", cseq_pos);
                cseq = request.substr(cseq_pos + 6, end - cseq_pos - 6);
            }

            if (request.find("OPTIONS") != std::string::npos) {
                std::ostringstream resp;
                resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\n";
                resp << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n";
                send_response(client_fd, resp.str());

            } else if (request.find("DESCRIBE") != std::string::npos) {
                std::ostringstream resp;
                resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\n";
                resp << "Content-Base: rtsp://127.0.0.1:8554/test/\r\n";
                resp << "Content-Type: application/sdp\r\n";
                resp << "Content-Length: " << sdp.size() << "\r\n\r\n";
                resp << sdp;
                send_response(client_fd, resp.str());

            } else if (request.find("SETUP") != std::string::npos) {
                std::ostringstream resp;
                resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\n";
                resp << "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n";
                resp << "Session: " << session_id << "\r\n\r\n";
                send_response(client_fd, resp.str());

            } else if (request.find("PLAY") != std::string::npos) {
                std::ostringstream resp;
                resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\n";
                resp << "Session: " << session_id << "\r\n\r\n";
                send_response(client_fd, resp.str());
//                stream_media(client_fd);
                break;

            } else if (request.find("TEARDOWN") != std::string::npos) {
                std::ostringstream resp;
                resp << "RTSP/1.0 200 OK\r\nCSeq: " << cseq << "\r\n";
                resp << "Session: " << session_id << "\r\n\r\n";
                send_response(client_fd, resp.str());
                break;
            }
        }

        CLOSESOCKET(client_fd);
        std::cout << "Client disconnected." << std::endl;
    }

    void stream_media(SocketType client_fd) {
        AVFormatContext* fmt_ctx = nullptr;
        if (avformat_open_input(&fmt_ctx, "../media/1.mp4", nullptr, nullptr) < 0) {
            std::cerr << "Failed to open media file\n";
            return;
        }
        avformat_find_stream_info(fmt_ctx, nullptr);
        int video_stream_index = -1;
        for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }

        uint16_t seq = 0;
        uint32_t timestamp = 0;

        AVPacket pkt;
        while (av_read_frame(fmt_ctx, &pkt) >= 0) {
            if (pkt.stream_index == video_stream_index) {
                if (pkt.flags == AV_PKT_FLAG_KEY) {
                    std::cout << "[push] send IDR frame" << std::endl;
                } else {
                    std::cout << "[push] send PB frame" << std::endl;
                }
                send_rtp_h264(client_fd, pkt.data, pkt.size, seq, timestamp);
                timestamp += 3600; // 对应25fps, 90000/25 = 3600
                usleep(40000); // 40ms
            }
            av_packet_unref(&pkt);
        }
        avformat_close_input(&fmt_ctx);
    }

    // rtp封包函数
    void send_rtp_h264(SocketType client_fd, const uint8_t* data, int size, uint16_t& seq, uint32_t timestamp) {
        const int max_rtp_payload = 1400;  // RTP 单包最大载荷，避免超出 MTU
        const uint8_t rtp_header_size = 12;

        const uint8_t nal_unit_type = data[0] & 0x1F;
        const uint8_t nri = (data[0] & 0x60) >> 5;

        if (size <= max_rtp_payload) {
            // 单一 RTP 包发送
            uint8_t rtp[1500] = {};
            rtp[0] = 0x80;                             // RTP version 2
            rtp[1] = 96;                               // Payload type (动态:96)
            rtp[2] = seq >> 8; rtp[3] = seq & 0xFF;    // Sequence number
            rtp[4] = (timestamp >> 24) & 0xFF;
            rtp[5] = (timestamp >> 16) & 0xFF;
            rtp[6] = (timestamp >> 8) & 0xFF;
            rtp[7] = timestamp & 0xFF;
            rtp[8] = 0; rtp[9] = 0; rtp[10] = 0; rtp[11] = 1;  // SSRC (固定值)

            memcpy(rtp + rtp_header_size, data, size);
            uint16_t pkt_size = rtp_header_size + size;

            // TCP interleaved header
            uint8_t header[4] = { 0x24, RTP_TCP_CHANNEL, (uint8_t)(pkt_size >> 8), (uint8_t)(pkt_size & 0xFF) };
            send(client_fd, (char*)header, 4, 0);
            send(client_fd, (char*)rtp, pkt_size, 0);
            seq++;

        } else {
            // FU-A 分片发送
            int offset = 1; // 跳过第一个 byte（NAL header）

            while (offset < size) {
                int fragment_size = std::min(max_rtp_payload - 2, size - offset); // FU-A 有 2 字节头
                uint8_t rtp[1500] = {};

                rtp[0] = 0x80;
                rtp[1] = 96;
                rtp[2] = seq >> 8; rtp[3] = seq & 0xFF;
                rtp[4] = (timestamp >> 24) & 0xFF;
                rtp[5] = (timestamp >> 16) & 0xFF;
                rtp[6] = (timestamp >> 8) & 0xFF;
                rtp[7] = timestamp & 0xFF;
                rtp[8] = 0; rtp[9] = 0; rtp[10] = 0; rtp[11] = 1;

                // FU Indicator
                rtp[12] = (nri << 5) | 28; // Type = 28 (FU-A)
                // FU Header
                rtp[13] = (offset == 1 ? 0x80 : 0x00); // Start bit
                rtp[13] |= ((offset + fragment_size) >= size) ? 0x40 : 0x00; // End bit
                rtp[13] |= nal_unit_type;

                memcpy(rtp + 14, data + offset, fragment_size);
                int pkt_size = fragment_size + 14;

                // TCP interleaved header
                uint8_t header[4] = { 0x24, RTP_TCP_CHANNEL, (uint8_t)(pkt_size >> 8), (uint8_t)(pkt_size & 0xFF) };
                send(client_fd, (char*)header, 4, 0);
                send(client_fd, (char*)rtp, pkt_size, 0);
                seq++;

                offset += fragment_size;
            }
        }
    }

    std::string base64_encode(const uint8_t* data, size_t len) {
        static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        for (size_t i = 0; i < len; i += 3) {
            int val = (data[i] << 16) + ((i + 1 < len ? data[i + 1] : 0) << 8) + (i + 2 < len ? data[i + 2] : 0);
            for (int j = 0; j < 4; ++j)
                ret += (i * 8 + j * 6 < len * 8) ? base64_chars[(val >> (18 - 6 * j)) & 0x3F] : '=';
        }
        return ret;
    }

    std::string extract_sprop_from_extradata(const uint8_t* data, int size) {
        std::vector<std::string> sprops;

        if (size < 6 || data[0] != 1) return "";

        int num_sps = data[5] & 0x1F;
        int pos = 6;
        for (int i = 0; i < num_sps && pos + 2 < size; ++i) {
            int len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            if (pos + len > size) break;
            sprops.push_back(base64_encode(data + pos, len));
            pos += len;
        }

        int num_pps = data[pos++];
        for (int i = 0; i < num_pps && pos + 2 < size; ++i) {
            int len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            if (pos + len > size) break;
            sprops.push_back(base64_encode(data + pos, len));
            pos += len;
        }

        if (sprops.size() >= 2)
            return "sprop-parameter-sets=" + sprops[0] + "," + sprops[1];
        return "";
    }
};

#endif // MEDIASTREAMER_H
