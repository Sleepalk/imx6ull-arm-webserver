初始化框架，选择并发模型(异步IO并发模型Proactor,使用epoll多路复用机制)

服务端创建socket步骤

1.socket()      //创建socket
2.setsockopt()  //设置地址复用(可选),服务器关闭时，会出现一个time_wait时间，这段时间内，端口不能立即重新绑定和使用，设置地址端口复用后，当服务器崩溃或重启时，可以允许服务器在time_wait时间段内绑定 到相同端口，实现快速重启

3.bind()        //绑定IP和端口
4.listen()      //监听
5.accept()      //等待连接，阻塞
6.while(1){
    read();     //读数据
    sleep(1);
    write();    //发数据
}
7.close();      //关闭套接字