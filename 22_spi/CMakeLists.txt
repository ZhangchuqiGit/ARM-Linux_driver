
#[[ cmake 用来读取 Cmakelists.txt 文件语句,最终生成 Makefile 文件。
    Makefile 是有条理的 gcc 编译命令文件，利用 make工具来执行 Makefile 的编译指令。
    一般利用 cmake 和 autotools 来自动生成 cmake。    ]]

message("==============================================================")
message("打印消息，供调试 CMakeLists.txt 文件使用\n文件 CMakeLists.txt 中不区分大小写")

#[[ 限定了 CMake 的 最低版本 ]]
cmake_minimum_required(VERSION 3.20)

project(_) # 工程项目名称

#message( "${CMAKE_CXX_COMPILER}\n${CMAKE_C_COMPILER}" )
if (0)       #[[ 指定 编译器 ]]
    #    set( CMAKE_C_CXX_COMPILER_mode "/usr/bin/g++" )
    set(CMAKE_C_CXX_COMPILER_mode "/usr/bin/g++-10") # 指定 C++ 编译器
    message("修改 编译器 ${CMAKE_CXX_COMPILER} 为 ${CMAKE_C_CXX_COMPILER_mode}")
    set(CMAKE_CXX_COMPILER ${CMAKE_C_CXX_COMPILER_mode})

    #    set( CMAKE_C_CXX_COMPILER_mode "/usr/bin/gcc" )
    set(CMAKE_C_CXX_COMPILER_mode "/usr/bin/gcc-10") # 指定 C 编译器
    message("修改 编译器 ${CMAKE_C_COMPILER} 为 ${CMAKE_C_CXX_COMPILER_mode}")
    set(CMAKE_C_COMPILER ${CMAKE_C_CXX_COMPILER_mode})
    unset(CMAKE_C_CXX_COMPILER_mode)
endif ()
if (0)       #[[ 指定 编译器 构建类型 ]]
    #[[ Debug 版会生成相关调试信息，可以使用 GDB 进行调试；Release 不会生成调试信息。
        当无法进行调试时查看此处是否设置为 Debug. ]]
    #    SET(CMAKE_BUILD_TYPE "Debug")    # 指定 编译器 构建类型 （默认 Debug）
    SET(CMAKE_BUILD_TYPE "Release")    # 指定 编译器 构建类型 （默认 Debug）
    message("指定 编译器 构建类型: ${CMAKE_BUILD_TYPE}")
endif ()
if (0)
    #[[ 指定 生成的 执行文件 存放路径 ]]
    SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin_exec")
    message("指定 二进制执行文件 存放路径:\n" ${EXECUTABLE_OUTPUT_PATH})
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON) # 告诉 CMake 使用 c++
set(CMAKE_C_STANDARD_REQUIRED ON) # 告诉 CMake 使用 c

set(CMAKE_CXX_STANDARD 17)  # 指定 c++17 版本
set(CMAKE_C_STANDARD 11)  # 指定 c11 版本

###########################################################################
###########################################################################

############### ---- 手动 修改 00
#[[ 编译选项
gcc   -Wall -g     sctp_service.c    -lsctp    -o main
gcc 前面 编译选项: -Wall -g
gcc 后面 编译选项: -lsctp -lrt -lpthread -lsctp -lkqueue -lopencv_dnn -lopencv_ml -lopencv_core -lopencv_objdetect -lopencv_stitching -lopencv_calib3d
-lopencv_features2d -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
 -lopencv_video -lopencv_photo -lopencv_imgproc -lopencv_flann    ]]
#set(CMAKE_C_CXX_FLAGS_front "  ") # 修改 前面 编译选项 ！
#set(CMAKE_C_CXX_FLAGS_behind "-lpthread -lsctp -lkqueue -lrt")   # 修改 后面 编译选项 ！
set(CMAKE_C_CXX_FLAGS_front " ")    # 修改 前面 编译选项 ！
set(CMAKE_C_CXX_FLAGS_behind " ")   # 修改 后面 编译选项 ！

message("==============================================================")
#[[ 设置 编译选项 ]]
#[[ 字母表顺序比较
    IF (var1 STRLESS var2)，var1 字母顺序 小于 var2 为真
    IF (var1 STRGREATER var2)，var1 字母顺序 大于 var2 为真
    IF (var1 STREQUAL var2)，var1 和 var2 字母顺序 相等 为真               ]]
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") #[[ 构建类型 ]]
    message("编译器 Cmake 构建类型: \${CMAKE_BUILD_TYPE} = Debug ")
    set(CMAKE_CXX_FLAGS_DEBUG
            "-Wall ${CMAKE_CXX_FLAGS_DEBUG} -ggdb -std=c++${CMAKE_CXX_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
    set(CMAKE_C_FLAGS_DEBUG
            "-Wall ${CMAKE_C_FLAGS_DEBUG} -ggdb -std=c${CMAKE_C_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
    message("CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG} ")
    message("CMAKE_C_FLAGS_DEBUG: ${CMAKE_C_FLAGS_DEBUG} ")
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message("编译器 Cmake 构建类型: \${CMAKE_BUILD_TYPE} = Release ")
    set(CMAKE_CXX_FLAGS_RELEASE
            "-Wall ${CMAKE_CXX_FLAGS_RELEASE} -g -ggdb -std=c++${CMAKE_CXX_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
    set(CMAKE_C_FLAGS_RELEASE
            "-Wall ${CMAKE_C_FLAGS_RELEASE} -g -ggdb -std=c${CMAKE_C_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
    message("CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE} ")
    message("CMAKE_C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE} ")
else ()
    message("编译器 Cmake 构建类型: unknow ${CMAKE_BUILD_TYPE} ")
    if (0) #[[ 第一种 add_compile_options() 添加的 编译选项 是针对 所有 编译器的(包括C和C++编译器) ]]
        add_compile_options("-D g -ggdb ${CMAKE_C_CXX_FLAGS_front}") # 增加 源文件的 编译选项
    else ()   #[[ 第二种 ]]
        set(CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS} -g -ggdb -std=c++${CMAKE_CXX_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
        set(CMAKE_C_FLAGS "-Wall ${CMAKE_CXX_FLAGS} -g -ggdb -std=c${CMAKE_C_STANDARD} ${CMAKE_C_CXX_FLAGS_front}") #指定编译选项
        message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
        message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
    endif ()
endif ()
unset(CMAKE_C_CXX_FLAGS_front)

###########################################################################
###########################################################################

############### ---- 手动 修改 01
set(CMake_Mode_select 1)    #[[ 工作模式 ]]
message("==============================================================")

if (${CMake_Mode_select} EQUAL 1)  # 混合模式

    message("${CMake_Mode_select} 混合模式: 头文件和源文件可混合放在不同的子目录")
    message("------------------------------------------------")

    ############### ---- 手动 修改 02
    #　要 手动 添加 源文件目录 *.c *.cpp 和 头文件目录 *.h ！
    set(include_lib_file
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/arch/arm/common
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/arch/arm/include
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/arch/arm/mach-exynos/include
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/arch/arm/plat-samsung/include
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/include
            /home/zcq/ARM_Linux_iTOP-4412/zcq_itop4412_POP1G_kernel-5.3.18/kernel-5.3.18/lib
            )
    message("手动 添加 头文件/源文件 目录: ${include_lib_file}")

elseif (${CMake_Mode_select} EQUAL 2) # 根目录 / 子目录

    ############### ---- 手动 修改 03-1
    set(lib_a_so_file
            zcq
            ) # 生成静态链接库 名称 (创建共享库)

    ############### ---- 手动 修改 03-2
    set(mode_include_library 0)

    if (${mode_include_library} EQUAL 1)                 #[[ 根目录下 ]]
        message("${CMake_Mode_select} 根目录下")
        message("------------------------------------------------")

        ############### ---- 手动 修改 04-1
        #　要 手动 添加 头文件 目录 *.h  ！！！
        set(include_file
                include
                )
        message("手动 添加 头文件 目录: ${include_file}")

        ############### ---- 手动 修改 04-2
        #　要 手动 添加 源文件 目录 目录 *.c *.cpp  ！！！
        set(lib_file
                lib
                )
        message("手动 添加 源文件 目录: ${lib_file}")
    else ()                                                 #[[ 子目录下 ]]
        message("${CMake_Mode_select} 子目录下: 源文件目录")
        message("------------------------------------------------")
    endif ()

elseif (${CMake_Mode_select} EQUAL 3)

    message("${CMake_Mode_select} 单文件版，逐一调试")
    message("------------------------------------------------")

    ############### ---- 手动 修改 04-1
    #　要 手动 添加 头文件 目录 *.h  ！！！
    set(include_file


            )
    message("手动 添加 头文件 目录: ${include_file}")

endif ()

###########################################################################
###########################################################################

if (${CMake_Mode_select} EQUAL 1)
    message("------------------------------------------------")
    #[[
        工程文件结构
                file01
                        A.c
                        B.cpp
                        C.h
                file01
                        D.h
                        E.cpp
                        F.h
                file03
                        G.cpp
                        H.h
                main.c
                CMakeLists.txt               ]]

    #[[ include_directories( include ) # 添加 include 目录 ( 头文件 *.h )
    include 目录的 CMakeLists.txt 可以为空   ]]
    #include_directories( file01 file02 file03 ) #　头文件 目录 *.h
    include_directories(
            .
            #  /usr/include            #[[ 相关头文件的目录 ]]
            #  /usr/local/include      #[[ 相关头文件的目录 ]]
            #              /usr/x86_64-linux-gnux32      #[[ 相关头文件的目录 ]]
            ${include_lib_file}     #　要 手动添加 头文件 目录 *.h ！
    )

    if (0)
        set(files "")
        set(file_ary "")
        set(DIR_SRC "")
        set(files ".;${include_lib_file}") # 手动 添加 子目录
        foreach (file ${files}) # 遍历 指定 目录
            aux_source_directory(${file} DIR_SRC) # 源文件
            set(file_ary "${file_ary}${DIR_SRC};")
            set(DIR_SRC "")
        endforeach ()
        add_executable(${PROJECT_NAME} ${file_ary})    # 生成 可执行文件
        message("不采用 宏函数 添加 源文件: ${file_ary}")
        unset(files)
        unset(file_ary)
        unset(DIR_SRC)

    else () # 采用 宏函数
        # 宏函数 按照需求 查找目录名称集合
        #[[     函数名 : SUBDIRLISTINCLUDE
                变量   : value
                返回值 : result                                            ]]
        macro(func_file result value)
            set(files "")
            set(file_ary "")
            set(DIR_SRC "")
            set(files ".;${value}") # 添加 子目录
            foreach (file ${files}) # 遍历 指定 目录
                aux_source_directory(${file} DIR_SRC)  # 源文件
                set(file_ary "${file_ary}${DIR_SRC};")
                set(DIR_SRC "")
            endforeach ()
            add_executable(${PROJECT_NAME} ${file_ary})  # 生成 可执行文件
            set(result "${file_ary}")
            unset(files)
            unset(file_ary)
            unset(DIR_SRC)
            unset(value)
        endmacro()
        func_file(result "${include_lib_file}") # 手动 添加 子目录
        message("采用 宏函数 添加 源文件: ${result}")
        unset(result)
        unset(include_lib_file)
    endif ()

    #[[
    gcc   -Wall -g     sctp_service.c    -lsctp    -o main
    gcc 前面 编译选项: -Wall -g
    gcc 后面 编译选项: -lsctp
    target_link_libraries(-lsctp) : 修改 后面 编译选项 : -lsctp
    target_link_libraries(./sctp) :
        -Wl,-rpath,/media/zcq/fast-office/ZCQ_cpp_code/Clion_Project_03/sctp  ]]
    target_link_libraries(${PROJECT_NAME}
            ${CMAKE_C_CXX_FLAGS_behind}  #[[ 后面 编译选项 ]]
            )

    unset(CMAKE_C_CXX_FLAGS_behind)
    message("==============================================================")
endif (${CMake_Mode_select} EQUAL 1)

###########################################################################
###########################################################################

if (${CMake_Mode_select} EQUAL 2)    # 根目录 / 子目录

    #[[
        工程文件结构
                lib 文件夹 ( 可执行文件 *.c  *.cpp )
                        libA.cpp
                        libB.c
                        CMakeLists.txt # 第02层 CMakeLists.txt
                include 文件夹 ( 头文件 *.h )
                        includeA.h
                        inclueeB.h
                main.c
                CMakeLists.txt # 第01层 CMakeLists.txt                  ]]

    if (${mode_include_library} EQUAL 1)                 #[[ 根目录下 ]]

        #[[ include_directories( include ) # 添加 include 目录 ( 头文件 *.h )
            include 目录的 CMakeLists.txt 可以为空  ]]
        # include_directories( include )    #　要 手动 添加 头文件 目录 *.h  ！！！
        include_directories(${include_file})    #　要 手动 添加 头文件 目录 *.h  ！！！

        #[[ aux_source_directory( . DIR_SRC )
        将当前目录下的 源文件 名字 都添加到 DIR_LIB 变量下 ]]
        set(DIR_SRC "")
        aux_source_directory(. DIR_SRC) # 源文件

        #[[ add_executable( yyy  ${xxx} )
            把 变量 xxx ( 文件 *.c *.cpp ) 编译成一个名称为 yyy 的 可执行文件       ]]
        add_executable(${PROJECT_NAME} ${DIR_SRC}) # 生成 可执行文件

        #[[ add_subdirectory ( source_dir [binary_dir] [EXCLUDE_FROM_ALL] )
            添加一个 子目录 source_dir 并 构建该子目录  ( 可执行文件 *.c  *.cpp )
        命令解析:
        source_dir
            必选参数。该参数指定一个子目录，子目录下应该包含 CMakeLists.txt 文件 和 代码文件。
            子目录可以是相对路径也可以是绝对路径。
        binary_dir
            可选参数。该参数指定一个目录，用于存放输出文件。可以是相对路径也可以是绝对路径。
            如果该参数没有指定，则默认的输出目录使用 source_dir。
        EXCLUDE_FROM_ALL
            可选参数。当指定了该参数，则子目录下的目标不会被父目录下的目标文件包含进去，
            父目录的 CMakeLists.txt 不会构建子目录的目标文件，必须在子目录下显式去构建。
            例外情况: 当父目录的目标依赖于子目录的目标，则子目录的目标仍然会被构建出来以满足依赖关系
            （ 例如使用了 target_link_libraries() ）。        ]]
        # add_subdirectory( lib )   # 要 手动 添加 源文件 子目录 *.c  *.cpp  ！！！
        add_subdirectory(${lib_file})   # 要 手动 添加 源文件 子目录 *.c  *.cpp  ！！！

        #[[ 将文件与库相连，${lib_a_so_file} 是 子目录 lib 第02层 CMakeLists.txt 生成的
        add_library( ${lib_a_so_file}  ${DIR_LIB} ) # 生成静态链接库 (创建共享库)
        -------------------------------------------------------------------
        gcc   -Wall -g     sctp_service.c    -lsctp    -o main
        gcc 前面 编译选项: -Wall -g
        gcc 后面 编译选项: -lsctp
        target_link_libraries(-lsctp) : 修改 后面 编译选项 : -lsctp
        target_link_libraries(./sctp) :
            -Wl,-rpath,/media/zcq/fast-office/ZCQ_cpp_code/Clion_Project_03/sctp  ]]
        target_link_libraries(${PROJECT_NAME}
                ${lib_a_so_file}
                #[[ 依赖的库文件 (子目录 lib 生成的共享库) ]]
                ${CMAKE_C_CXX_FLAGS_behind} #[[ 后面 编译选项 ]]
                )

        unset(DIR_SRC)
        unset(include_file)
        unset(lib_file)
        message("==============================================================")
    else ()                                                 #[[ 子目录下 ]]
        if (0)
            ##[[ 指定 动态/静态 链接库 存放路径 ]]
            SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib_share_static")
            message("指定 动态/静态 链接库 存放路径:\n" ${LIBRARY_OUTPUT_PATH})
        endif ()

        #[[ aux_source_directory( . DIR_LIB )
        将当前目录下的 源文件 名字 都添加到 DIR_LIB 变量下 ]]
        set(DIR_LIB "" Posix_shared_memory/mman_test.cpp Posix_shared_memory/mman_test.h System_V_shared_memory/shm_test.cpp System_V_shared_memory/shm_test.h)
        aux_source_directory(. DIR_LIB) # 源文件

        #[[ add_library( ${lib_a_so_file}  ${DIR_LIB} )
            生成 静态链接库 命名 为 ${lib_a_so_file}
            第01层 CMakeLists.txt 将 第02层 CMakeLists.txt
            生成的 ALL_Project 文件 与 动态库(源文件) 相连
            target_link_libraries( ALL_Project lib_Project )  # 与 动态库 相连
            创建共享库（把工程内的cpp文件都创建成共享库文件，方便通过头文件来调用）
            这时候只需要cpp，不需要有主函数 ]]
        add_library(${lib_a_so_file} SHARED ${DIR_LIB}) # 生成 动态 链接库 (创建共享库)
        #        add_library( ${lib_a_so_file} STATIC ${DIR_LIB} ) # 生成 静态 链接库 (创建共享库)

        unset(DIR_LIB)
        message("==============================================================")
    endif ()

    unset(lib_a_so_file)
    unset(mode_include_library)
endif (${CMake_Mode_select} EQUAL 2)  # 根目录 / 子目录

###########################################################################
###########################################################################

if (${CMake_Mode_select} EQUAL 3)   # 单文件版
    message("${CMake_Mode_select} 描述: 单文件版，逐一调试，自动遍历工程项目的所有源文件。
    缺点: 这种方法要求所有源文件 *.c *.cpp 文件命名 不重复，
    不能含有中文，不能含有‘/’等字符！因为它就是直接 copy 源码文件名的。")

    #[[ include_directories( include ) # 添加 include 目录 ( 头文件 *.h )
    include 目录的 CMakeLists.txt 可以为空  ]]
    # include_directories( include )    #　要 手动 添加 头文件 目录 *.h  ！！！
    include_directories(${include_file})    #　要 手动 添加 头文件 目录 *.h  ！！！
    unset(include_file)

    # 宏函数 按照需求 查找目录名称集合
    #[[     函数名 : SUBDIRLISTINCLUDE
            变量   : value
            返回值 : result                                            ]]
    macro(func_file result value)
        message("源文件 后缀名 : ${value}")
        set(files "")
        set(exec "")
        file(GLOB_RECURSE files "*.${value}") # 遍历项目 根目录下 所有目录的 .cpp 文件
        foreach (file ${files}) # 遍历 指定 目录
            string(REGEX REPLACE ".+/(.+)\\..*" "\\1" exec ${file})
            add_executable(${value}_${exec} ${file})   # 生成 可执行文件
            target_link_libraries(${value}_${exec}
                    ${CMAKE_C_CXX_FLAGS_behind}  #[[ 后面 编译选项 ]]
                    )
            message("add_executable( ${value}_${exec}\t${file} )")
            # message ( \ add_executable\(\ ${exec}\ \t\ ${exec}.cpp\  \) )
        endforeach ()
        set(result "")
        unset(files)
        unset(exec)
    endmacro()

    func_file(result "c") # 源文件 后缀名
    func_file(result "cpp") # 源文件 后缀名

    unset(result)
    unset(CMAKE_C_CXX_FLAGS_behind)
    message("==============================================================")
endif (${CMake_Mode_select} EQUAL 3)   # 单文件版

###########################################################################
###########################################################################
if (0)
    message("------------ 预定义变量 -----------")

    message("project() 工程项目名称:\t" ${PROJECT_NAME})
    message("工程的根目录:\n" ${PROJECT_SOURCE_DIR})
    message("工程运行 cmake 命令的目录:\n" ${PROJECT_BINARY_DIR})
    message("项目 CMakeLists.txt 目录:\n" ${CMAKE_SOURCE_DIR})
    message("当前处理的 CMakeLists.txt 路径:\n" ${CMAKE_CURRENT_SOURCE_DIR})
    message("当前处理的 CMakeLists.txt 编译目录:\n" ${CMAKE_CURRENT_BINARY_DIR})
    message("当前处理的 CMakeLists.txt 完整路径:\n" ${CMAKE_CURRENT_LIST_DIR})
    message("当前所在的行:\t" ${CMAKE_CURRENT_LIST_LINE})

    SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cppexec")
    message("自定义 cmake 模块所在的路径:\n" ${CMAKE_MODULE_PATH})

    #[[ 表示生成的执行文件所在路径 ]]
    SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/cppexec")
    message("指定可二进制执行文件的存放路径:\n" ${EXECUTABLE_OUTPUT_PATH})

    SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/cppexec")
    message("重新定义目标动态/静态链接库文件的存放路径:\n" ${LIBRARY_OUTPUT_PATH})

    SET(CMAKE_SOURCR_DIR "${PROJECT_SOURCE_DIR}")
    message("内部构建指的是工程顶层目录:\n" ${CMAKE_SOURCR_DIR})

    SET(CMAKE_BINARY_DIR "${PROJECT_SOURCE_DIR}")
    message("外部构建指的是工程发生编译的目录:\n" ${CMAKE_BINARY_DIR})

    message("------------ 系统信息 -----------")

    message("cmake 主版本号，比如 3.17.5 中的 3:\t" ${CMAKE_MAJOR_VERSION})
    message("cmake 次版本号，比如 3.17.5 中的 17:\t" ${CMAKE_MINOR_VERSION})
    message("cmake 补丁等级，比如 3.17.5 中的 5:\t" ${CMAKE_PATCH_VERSION})

    message("含版本的系统名称，比如 Linux-5.10.0-1019-oem:\t" ${CMAKE_SYSTEM})
    message("不含版本的系统名，比如 Linux:\t" ${CMAKE_SYSTEM_NAME})
    message("系统版本，比如 5.10.0-1019-oem:\t" ${CMAKE_SYSTEM_VERSION})
    message("处理器名称:\t" ${CMAKE_SYSTEM_PROCESSOR})

    #[[ 平台类型 ]]
    IF (WIN32)
        MESSAGE(STATUS "System is windows")
    ELSEIF (APPLE)
        MESSAGE(STATUS "System is Apple.")
    ELSEIF (UNIX)
        MESSAGE(STATUS "System is UNIX.")
    ENDIF ()

    message("==============================================================")
endif ()

###########################################################################
###########################################################################

#[[ 局部变量:                     设置变量 : set( 变量  值 )
    CMakeLists.txt 相当于一个函数，第一个执行的 CMakeLists.txt 相当于主函数，
    正常设置的变量不能跨越 CMakeLists.txt 文件，
    相当于 局部变量 只在当前函数域里面作用一样。
    设置局部变量: set( Value  "123abc" )
    访问局部变量: ${Value}                                              ]]

#[[ 缓存变量，相当于 全局变量:
    都是在第一个执行的 CMakeLists.txt 里面被设置的，
    不过在 子项目 的 CMakeLists.txt 文件里面也是 可以修改 这个变量
    此时 会影响 父目录 的 CMakeLists.txt，
    这些变量用来配置整个工程，配置好之后对整个工程使用。
    设置缓存变量: set( Cache_Value "cache_value" CACHE INTERNAL "This is my cache value" )
    字符串 "This is my cache value" 相当于 变量的 描述说明，不能省略，但可以随便定义
    访问缓存变量: ${Cache_Value}                                              ]]

#[[ 环境变量:
    设置环境变量: set( ENV{variable_name}  value )
    获取环境变量: $ENV{variable_name}                      ]]

###########################################################################
###########################################################################

#[[     ======  编译选项  ====== gcc / g++ 参数
------------------------------------------------------
-g: 在编译的时候，产生调试信息。
    Debug 版本默认只有一个 -g 选项，包含调试信息
-gstabs: 以 stabs 格式声称调试信息,不包含 gdb 调试信息.
-gstabs+:　以 stabs 格式声称调试信息,包含 gdb 调试信息.
-ggdb: 将尽可能的生成 gdb 的可以使用的调试信息.
------------------------------------------------------
-w: 不生成任何警告信息
-Wall: 选项可以 打印出 编译时所有的错误或者警告 信息。
    这个选项很容易被遗忘，编译的时候，没有错误或者警告提示，
    以为自己的程序很完美，其实有可能隐藏着许多陷阱: 变量没有初始化、
    类型不匹配、类型转换错误等警告提示需要重点注意。
    没有使用的变量也需要注意，去掉无用的代码，让整个程序显得干净一点。
------------------------------------------------------
-O: 编译器工具链 gcc / g++ 的优化级别: 默认优化级别为零，选项 -O 或 -O0（根本不提供优化）
    -O0: 表示编译时没有优化。默认
    -O1: 表示编译时使用默认优化。
    -Os: 相当于 -O1.5 优化
    -O2: 表示编译时使用二级优化。
    -O3: 表示编译时使用最高级优化。Release 版本默认 -O3 选项
------------------------------------------------------
-x [language] [filename]
    设定文件所使用的语言,使后缀名无效,对以后的多个源文件或头文件有效。
    也就是根据约定C语言的后缀名称是.c的，而C++的后缀名是.C或者.cpp,
    用这个参数决定你的C代码文件的后缀名，如 mycpp.zcq 。
    [language]: c, objective-c, c-header, c++, cpp-output, assembler, assembler-with-cpp.
    [filename]: 源文件或头文件。
    例子: gcc -x c hello.pig   // hello.c -> hello.pig
-x none [filename]
　　关掉上一个选项( -x [language] [filename] )，也就是根据文件名后缀，自动识别文件类型
    例子: gcc -x c hello.pig -x none hello2.c
------------------------------------------------------
-S: 只激活预处理和编译，就是指把文件编译成为汇编代码。
    例子: gcc -S hello.c  // 将生成 .s 的汇编代码
------------------------------------------------------
-E: 只激活预处理,这个不生成文件, 你需要把它重定向到一个输出文件里面。
    例子: gcc -E hello.c > pianoapan.txt
          gcc -E hello.c | more
------------------------------------------------------
-o [file.exe]: 生成可执行文件,缺省的时候,gcc 编译出来的文件是 a.out
    例子: gcc -o hello.exe hello.c (哦,windows用习惯了)
    　    gcc -S hello.c -o hello.asm
------------------------------------------------------
-pipe: 使用管道代替编译中临时文件,在使用 非gnu 汇编工具的时候,可能有些问题
    例子: gcc -pipe hello.c -o hello.exe
------------------------------------------------------
-ansi: 关闭 gnu c 中与 ansi c 不兼容的特性,激活 ansi c 的专有特性
    (包括禁止一些 asm inline typeof 关键字,以及 UNIX,vax 等预处理宏,
-fno-asm: 此选项实现 -ansi 选项功能的一部分，它禁止将 asm,inline 和 typeof 用作关键字。
------------------------------------------------------
-fno-strict-prototype: 只对 g++ 起作用,使用这个选项,g++ 将对不带参数的函数,
    都认为是没有显式的对参数的个数和类型说明,而不是没有参数.
    而 gcc 无论是否使用这个参数,都将对没有带参数的函数,认为是没有显式说明的类型
------------------------------------------------------　　
-fthis-is-varialble: 就是向传统 c++ 看齐,可以使用 this 当一般变量使用.
------------------------------------------------------
-fcond-mismatch: 允许条件表达式的第二和第三参数类型不匹配,表达式的值将为 void类型
------------------------------------------------------
-funsigned-char 、-fno-signed-char、-fsigned-char 、-fno-unsigned-char
    这四个参数是对 char 类型进行设置,
    决定将 char 类型设置成 unsigned char(前两个参数)或者 signed char(后两个参数)。
------------------------------------------------------
-include [filename]: 功能相当于在代码中使用 #include<filename>
    例子: gcc hello.c -include /usr/include/stdio.h -o hello.exe
------------------------------------------------------
-imacros [file]: 将 file 文件的宏, 扩展到 gcc/g++ 的输入文件,
    宏定义本身并不出现在输入文件中。　　
------------------------------------------------------
-Dmacro: 相当于 C 语言中的 #define macro
-Dmacro=defn: 相当于 C 语言中的 #define macro=defn
-Umacro: 相当于 C 语言中的 #undef macro
-undef: 取消对任何非标准宏的定义
------------------------------------------------------
-M: 生成文件关联的信息（目标文件所依赖的所有源代码）
    可以用 gcc -M hello.c 来测试一下。
-MM: 和 -M 相同，但忽略由 #include<file> 造成的依赖关系。
-MD: 和 -M 相同，但输出将导入到 .d 的文件里面　
-MMD: 和 -MM 相同，但输出将导入到 .d 的文件里面
------------------------------------------------------
-Idir: 头文件的搜索路径，
    用 #include "file.h" 的时候,gcc/g++会先在 当前目录 dir 查找自定义头文件，
    如果没有找到，再按常规的顺序去找。
-I-: 取消前一个参数的功能,所以一般在 -Idir 之后使用
-idirafter dir: 在 -I 的 dir 目录 查找失败，就到 这个目录 dir 里面查找。
-iprefix prefix, -iwithprefix dir: 一般一起使用，
     在 -I 的 dir 目录 查找失败,会到 prefix + dir 下查找
-nostdinc: 使编译器不再系统缺省的头文件目录里面找头文件,
    一般和-I联合使用,明确限定头文件的位置
------------------------------------------------------
-c: 只激活预处理,编译,和汇编,也就是他只把程序做成 obj 文件
    例子: gcc -c hello.c  // 将生成 hello.o 的 obj 文件
------------------------------------------------------
-C: 预处理时保留注释信息,不删除注释信息,一般和-E使用,有时候分析程序，用这个很方便的
------------------------------------------------------
-m32, -m64: 生成32/64位的 动态/静态 链接库
------------------------------------------------------
-fPIC(或fpic): 生成使用相对地址无关的目标代码
------------------------------------------------------
-static: 强制所有的库都使用静态库版本，禁止使用动态库
-shared: 进行动态库编译，链接动态库
------------------------------------------------------
-llibrary: 制定编译的时候使用的库
　　例子: gcc -lpthread          hello.c -o hello.exe
-lname: 链接 静态库(libname.a)或 动态库(libname.so)的库文件
-Ldir: 制定编译的时候，搜索库的路径。
    比如你自己的库，可以用它制定目录，不然编译器将只在标准库的目录找。
　　例子: gcc -L/usr/local/lib   hello.c -o hello.exe
------------------------------------------------------
编译器 g++ 与 gcc 的使用是相通的
------------------------------------------------------
动态库 ( Windows: *.dll ; Linux : *.so ) 是在运行时动态加载，当删除库文件时会报错
使用 gcc 生成 动态库 及 使用动态库 的方法:
第一步：生成
    方法一: 将生成 zcq.o 的 obj 文件，再使用 -shared 参数 生成 动态库
           gcc -c zcq.c
           gcc -shared -fPIC  zcq.o  -o libzcq.so
    方法二：使用 -shared 参数 生成 动态库
           gcc -shared -fPIC  zcq.c  -o libzcq.so
第二步：使用
    方法一：把 libzcq.so 复制到 /usr/lib64/ 系统路径下
           注意：生成的动态库的名字必须是 lib+名字.so，之后可以直接使用 -lzcq 来引用动态库。
           编译 main.c，使用 libzcq.so 动态库
           gcc -L/usr/lib64/ -lzcq    main.c -o main
    方法二：直接使用 当前目录下的 libtest.so 文件,
           编译 main.c，链接 libtest.so 的时候，
           gcc ./lib/libzcq.so        main.c -o main
           gcc -L./lib       -lzcq    main.c -o main
注意：当编译链接完生成可执行文件后，在运行时会出现 "无法找到 libzcq.so" 等类似错误，
g++ 在链接时搜索到 第三方库时，链接成功。但是在执行过程中，当调用相应的外部函数时，
程序再一次会 去找第三方库，因为默认搜索目录没有，所以会出现找不到库的情况。
这时需要我们修改编译链接命令：
    g++  *.cpp  -I./include/  -L./lib  -lzcq  -Wl,-rpath=./  -o main
-Wl,-rpath=“具体目录”，将第三方库路径显性加入程序运行时搜索目录
------------------------------------------------------
静态库 ( Windows: *.lib ; Linux : *.a ) 是在链接的时候直接把库文件复制到程序中，
    运行的时候不再依赖库文件
使用 gcc 生成 静态库 及 使用静态库 的方法:
第一步：生成
    方法一: 将生成 zcq.o 的 obj 文件，再将 obj 文件 生成 libzcq.a 静态库
           gcc -c  zcq.c
           ar  -crv  libzcq.a   zcq.o
    方法二：生成 libzcq.a 静态库
           gcc  -fPIC  zcq.c  -o libzcq.a
第二步：使用
       gcc  -L./lib  -lzcq   main.c -o main
------------------------------------------------------
clion 添加 c++17 std::thread 的方法 :
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread -std=c++17 " )      ]]
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Wall -pthread -lsctp -lm -std=c++17 -fPIC" ) # 指定编译选项
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g1 -Wall -lpthread -lsctp -lm -std=c17 -fPIC" ) # 指定编译选项
# gcc               -Wall -g sctp_service.c            -lrt   -lsctp   -o main
# gcc 前面 编译选项: -Wall -g sctp_service.c  后面 编译选项: -lsctp   -o main
#/usr/lib/x86_64-linux-gnu/libsctp.a
#/usr/lib/x86_64-linux-gnu/libsctp.so.1.0.18


