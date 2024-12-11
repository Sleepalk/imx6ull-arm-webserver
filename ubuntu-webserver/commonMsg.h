//HTTP请求方法
enum METHOD {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT
};

//解析客户端请求，主状态机状态
enum CHECK_STATE {
    CHECK_STATE_REQUESTHEAD = 0,    //请求头
    CHECK_STATE_REQUESTLINE,        //请求行
    CHECK_STATE_REQUESTBODY,        //请求体
};

//解析客户端请求，从状态机状态
enum LINE_STATE {
    LINE_OK = 0,    //读取到完整行
    LINE_BAD,       //行出错
    LINE_OPEN       //行数据不完整
};

//服务端处理请求可能的结果
enum HTTP_RESULT {
    NO_REQUEST = 0,         //请求不完整
    GET_REQUEST,            //获取到了一个完整的客户请求
    BAD_REQUEST,            //客户端请求语法错误
    NO_RESOURCE,            //服务器没有资源
    FORBIDDEN_REQUEST,      //客户端对资源没有足够权限
    FILE_REQUEST,           //文件请求
    INTERNAL_ERROR,         //服务器内部错误
    CLOSEED_CONNECTION      //客户端已关闭连接
};