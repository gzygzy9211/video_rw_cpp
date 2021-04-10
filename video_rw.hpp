#pragma once
#include "subprocess.hpp"
#include <opencv2/highgui/highgui.hpp>

namespace video_rw {
namespace {

void dump_buffer(const unsigned char *buf, const size_t len) {
    const int N = 16;
    // 00000000 00000010: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
    const size_t buffer_len = 8 + 1 + 8 + 1 + 3 * 16 + 1;
    char buffer[buffer_len];
    buffer[buffer_len - 1] = '\0';
    size_t i;
    for (i = 0; i + N <= len; i += N) {
        sprintf(buffer, "%08x %08x:", (uint32_t)(i >> 32), (uint32_t)(i & 0xffffffffULL));
        char *ptr = buffer + 8 + 1 + 8 + 1;
        #pragma unroll
        for (size_t ii = 0; ii < N; ii ++, ptr += 3) {
            sprintf(ptr, " %02x", (uint32_t)buf[i + ii]);
        }
        fprintf(stderr, "%s\n", buffer);
    }
    if (i == len) return;
    fprintf(stderr, "%08x %08x:", (uint32_t)(i >> 32), (uint32_t)(i & 0xffffffffULL));
    for (size_t ii = i; ii < len; ii ++) {
        fprintf(stderr, " %02x", (uint32_t)buf[ii]);
    }
    fprintf(stderr, "\n");
}

std::string get_ff_prefix() {
    auto val = getenv("FF_PREFIX");
    return val == nullptr ? "" : std::string(val) + "/";
}

int get_frame_rate(const char *video_path, int &num, int &den) {
    using namespace subprocess;
    auto ffprobe = get_ff_prefix() + "ffprobe";
    auto process = Popen({ffprobe, "-select_streams", "v",
                          "-of", "default=noprint_wrappers=1:nokey=1", 
                          "-show_entries", "stream=avg_frame_rate",
                          std::string(video_path)},
                          output{PIPE}, error{PIPE}, shell{false});
    auto outputs = process.communicate();
    if (2 != sscanf(outputs.first.buf.data(), "%d/%d", &num, &den)) {
        std::cerr << "fail to read fps from video " << video_path << ", guess 30 fps\n";
        std::cerr << std::string(outputs.second.buf.data(), outputs.second.length) << "\n";
        num = 30; den = 1;
        return -1;
    } else {
        return 0;
    }
}

class VideoReader {
public:
    const static std::vector<unsigned char> PNG_START;
    const static std::vector<unsigned char> PNG_TERM;
    VideoReader(const char *path) : proc_({get_ff_prefix() + "ffmpeg", "-i", std::string(path),
                                           "-vcodec", "png", "-f", "image2pipe", "-"},
                                           subprocess::input{subprocess::PIPE},
                                           subprocess::output{subprocess::PIPE},
                                           subprocess::error{subprocess::PIPE},
                                           subprocess::shell{false}),
                                    stream_(nullptr),
                                    byte_read_(0) { proc_.close_input(); }
    ~VideoReader() { proc_.kill(); }

    bool next_frame(cv::Mat &im) {
        start_if_nullstream();
        if (stream_ == nullptr) return false;
        size_t start_idx = 0;
        for (start_idx = next_png_start(start_idx);
             start_idx + PNG_START.size() > buffer_.size() && add_data_segment();
             start_idx = next_png_start(start_idx));
        if (start_idx + PNG_START.size() > buffer_.size()) return false;

        size_t end_chunk = start_idx + PNG_START.size();
        for (end_chunk = next_png_term(end_chunk);
             end_chunk + PNG_TERM.size() > buffer_.size() && add_data_segment();
             end_chunk = next_png_term(end_chunk));
        if (end_chunk + PNG_TERM.size() > buffer_.size()) return false;

        cv::Mat im_buffer(1, end_chunk + PNG_TERM.size() - start_idx, CV_8U, buffer_.data() + start_idx);
        // dump_buffer(im_buffer.data, 32);
        // dump_buffer(im_buffer.data + im_buffer.cols - 32, 32);
        im = cv::imdecode(im_buffer, cv::IMREAD_COLOR);
        buffer_ = std::move(std::vector<unsigned char>(buffer_.begin() + end_chunk + PNG_TERM.size(), buffer_.end()));
        return !im.empty();
    }
private:
    void start_if_nullstream() {
        if (stream_ == nullptr) {
            stream_ = proc_.output();
        }
    }
    bool add_data_segment() {
        // return true if stream_ is not opened or reaches EOF
        if (stream_ == nullptr) return false;
        if (feof(stream_) != 0) return false;
        cv::Mat cur_buffer(1, seg_len_, CV_8U);
        size_t count = fread(cur_buffer.data, 1, seg_len_, stream_);
        if (count == 0) return true;
        buffer_.resize(buffer_.size() + count);
        memcpy(buffer_.data() + buffer_.size() - count, cur_buffer.data, count);
        byte_read_ += count;
        return true;
    }
    size_t next_png_start(size_t idx) {
        uint64_t query;
        if (idx + sizeof(query) > buffer_.size()) return idx;
        
        memcpy(&query, PNG_START.data(), sizeof(query));
        for (size_t i = idx; i <= buffer_.size() - sizeof(query); i ++) {
            uint64_t value;
            memcpy(&value, buffer_.data() + i, sizeof(query));
            if (query == value) return i;
        }
        return buffer_.size() - sizeof(query) + 1;
    }
    size_t next_png_term(size_t idx) {
        typedef struct {
            uint32_t k0; uint32_t k1; uint32_t k2;
            inline const uint64_t& k1_64() const { *reinterpret_cast<const uint64_t*>(this); }
        } query_t;
        query_t query;
        if (idx + sizeof(query) > buffer_.size()) return idx;

        memcpy(&query, PNG_TERM.data(), sizeof(query));
        for (size_t i = idx; i <= buffer_.size() - sizeof(query); i ++) {
            query_t value;
            memcpy(&value, buffer_.data() + i, sizeof(query));
            if (query.k1_64() == value.k1_64() && query.k2 == value.k2) return i;
        }
        return buffer_.size() - sizeof(query) + 1;
    }
    subprocess::Popen proc_;
    FILE* stream_;
    std::vector<unsigned char> buffer_;
    size_t byte_read_;
    const static int seg_len_ = 1024;
};

inline constexpr unsigned char operator "" _uchar( char arg ) noexcept { return static_cast<unsigned char>(arg); }
const std::vector<unsigned char> VideoReader::PNG_START = {'\x89'_uchar, 'P', 'N', 'G', '\x0d', '\x0a', '\x1a', '\x0a'};
const std::vector<unsigned char> VideoReader::PNG_TERM  = {'\0', '\0', '\0', '\0', 'I', 'E', 'N', 'D',
                                                           '\xae'_uchar, '\x42'_uchar, '\x60'_uchar, '\x82'_uchar};

class VideoWriter {
public:
    VideoWriter(const char *path, const int fps_num, const int fps_den)
        : proc_({get_ff_prefix() + "ffmpeg", "-f", "image2pipe", "-r",
                 std::to_string(fps_num) + "/" + std::to_string(fps_den),
                 "-i", "-", "-y", "-pix_fmt", "yuv420p", "-vcodec", "h264", std::string(path)},
                subprocess::input{subprocess::PIPE},
                subprocess::output{subprocess::PIPE},
                subprocess::error{subprocess::PIPE},
                subprocess::shell{false}),
          stream_(nullptr) {}
    ~VideoWriter() { if (stream_ != nullptr) proc_.close_input(); }
    bool write_frame(const cv::Mat &im) {
        start_if_nullstream();
        std::vector<unsigned char> buffer;
        auto ret = cv::imencode(".png", im, buffer);
        if (!ret) return ret;
        return buffer.size() == fwrite(buffer.data(), 1, buffer.size(), stream_);
    }
private:
    void start_if_nullstream() {
        if (stream_ == nullptr) {
            stream_ = proc_.input();
        }
    }
    subprocess::Popen proc_;
    FILE* stream_;
};
}
} // namespace video_rw
