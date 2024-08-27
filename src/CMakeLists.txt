# 将当前目录下所有的cpp文件都当做要参与编译的文件
#aux_source_directory(. SRC_LIST)

# 手动设置变量 SRC_LIST，包含构建 mprpc 库所需的源文件列表
set(SRC_LIST 
    mprpcapplication.cc 
    mprpcconfig.cc 
    rpcheader.pb.cc 
    rpcprovider.cc 
    mprpcchannel.cc
    mprpccontroller.cc
    logger.cc
    zookeeperutil.cc)

#创建一个名为 mprpc 的库，默认是共享库（动态库）（.so 或 .dll）
add_library(mprpc ${SRC_LIST})

# 将外部库与 mprpc 进行链接
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)