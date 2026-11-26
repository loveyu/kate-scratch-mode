cmake_minimum_required(VERSION 3.16)
project(katescratchmode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# 先探测 KF6（内含 QT_MAJOR_VERSION=6），再 include KDEInstallDirs，
# 否则 ECM 会退回 Qt5 的安装路径查询并失败
find_package(KF6 REQUIRED COMPONENTS TextEditor SyntaxHighlighting)
include(KDEInstallDirs)

add_library(katescratchmode MODULE katescratchmode.cpp)
# 与官方 ktexteditor 插件命名保持一致：无 lib 前缀
set_target_properties(katescratchmode PROPERTIES PREFIX "")
target_link_libraries(katescratchmode PRIVATE KF6::TextEditor KF6::SyntaxHighlighting)

install(TARGETS katescratchmode DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/ktexteditor)
