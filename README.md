# video_rw_cpp
convenient interface for simple video r/w in cpp using ffmpeg/ffprob executable

# build sample
build ``video_test`` sample by following steps
```shell
make build && cd build
cmake ..
make
```

# run sample
```shell
cd build
FF_PREFIX=../ ./video_test input.mp4 rgb_reversed.mp4
```

# thirdparty
subprocess.hpp from [cpp_subprocess](https://github.com/arun11299/cpp-subprocess) (version 2.0)

