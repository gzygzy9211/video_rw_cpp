#include "video_rw.hpp"
#include <opencv2/imgproc/imgproc.hpp>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "%s {input} {output}\n", argv[0]);
        return argc;
    }

    int fps_num, fps_den;
    if (0 != video_rw::get_frame_rate(argv[1], fps_num, fps_den)) return -1;
    video_rw::VideoReader reader(argv[1]);
    video_rw::VideoWriter wirter(argv[2], fps_num, fps_den);

    cv::Mat frame;

    for (size_t count = 0; reader.next_frame(frame); count ++) {
        std::cerr << count << ": " << frame.cols << " x " << frame.rows << "\n";
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
        wirter.write_frame(frame);
    }
}