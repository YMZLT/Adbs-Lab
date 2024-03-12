# 设置cmake最低版本
cmake_minimum_required(VERSION 3.12)
# set(CMAKE_CXX_STANDARD 17)
# 设置项目名称 可以与根目录名称不同
project(simple-adbs-lab)

# 配置openMP多线程
find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
    message("OPENMP FOUND")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()
set(ignoreMe "${CMAKE_C_COMPILER}")

# 调试模式 Release|Debug
SET(CMAKE_BUILD_TYPE "Debug")  
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")  
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")  

# 设置可执行文件与链接库保存的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置头文件目录使得系统可以找到对应的头文件
include_directories(${PROJECT_SOURCE_DIR}/include)
# 设置需要编译的源文件
set(SOURCES
    src/main.cpp
    src/DSMgr.cpp 
    src/BMgr.cpp 
    src/ReplaceAlg.cpp 
)
add_executable(main ${SOURCES})
