const globalConfig = {
    MAIN_SERVER: {
        PORT: 8000,
        STUDENT_NUMBER: 0,
        ADDRESS: `http://172.20.10.3:8000`, // 172.20.10.3
    },
    LOCAL_SERVER_1: {
        PORT: 8001,
        STUDENT_NUMBER: 1,
        ADDRESS: `http://localhost:8001`,
    },
    LOCAL_SERVER_2: {
        PORT: 8002,
        STUDENT_NUMBER: 2,
        ADDRESS: `http://localhost:8002`,
    },
};

module.exports = globalConfig;
