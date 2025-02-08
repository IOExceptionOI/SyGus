#!/bin/bash

# 删除旧的build目录
rm -rf build

# 创建新的build目录
mkdir build

# 进入build目录
cd build

# 运行cmake配置项目
cmake ..

# 编译项目
make

# 进入/build/tasks/
cd tasks/

# 执行task_enumeration
./task_enumeration
