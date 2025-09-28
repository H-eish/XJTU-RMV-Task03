# 1. 项目基本设置
cmake_minimum_required(VERSION 3.10.0)
project(xjtu-rmv-task03 VERSION 0.1.0 LANGUAGES C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找所有需要的外部库 (这是关键步骤，必须在 add_executable 和 target_link_libraries 之前)
# 这一步执行后，CMake 才知道 ceres::ceres 和 ${OpenCV_LIBS} 是什么
find_package(OpenCV REQUIRED)
find_package(Ceres REQUIRED)

# 3. 定义你自己的可执行文件
# 这一步执行后，CMake 才知道有一个目标叫做 "main"
add_executable(main main.cpp)

# 4. 将你的可执行文件与找到的库链接起来
# 此时，CMake 既知道 "main" 也知道 "ceres::ceres"，所以可以成功链接
target_link_libraries(main  
    ${OpenCV_LIBS}
    ${CERES_LIBRARIES}
)