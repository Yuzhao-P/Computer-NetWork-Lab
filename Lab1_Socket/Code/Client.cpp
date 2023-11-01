#include<iostream>
#include<cstring>
#include <winsock2.h>  // 套接字接口
#include <Ws2tcpip.h>  // 套接字新函数库
#include <time.h>
#pragma comment (lib, "ws2_32.lib")  // 套接字实现

#define PORT 8000  // 端口
#define MaxBufSize 1024  // 最大缓冲区大小

using namespace std;
char user_id[10];  // 用户ID
SOCKET clientSocket;  //客户端socket
time_t t;
char time_str[64];
// 接收消息的进程处理函数声明
DWORD WINAPI recvThread();

int main()
{
	cout << "===============================================" << endl;
	cout << "==================叡神客户端===================" << endl;
	cout << "===============================================" << endl;
	cout << "叡神客户端[版本 23.10.18.0]      开发者-Yuzhao-" << endl;
	cout << "===============================================" << endl;
	cout << "==================客户端准备===================" << endl;
	//========================初始化Socket DLL=========================
	WORD wVersionRequested = MAKEWORD(2, 2);  // 版本请求：MAKEWORD(次版本号, 主版本号)
	WSAData wsadata;  // 套接字实现的详细信息
	int err = WSAStartup(wVersionRequested, &wsadata);  // err是WSAStartup函数的返回值——错误代码
	if (err != NO_ERROR) {
		cout << "[错误] 初始化Socket DLL失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		system("pause");
		return 1;
	}
	cout << "[系统提示] 初始化Socket DLL成功！" << endl;
	//============================创建Socket===========================
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 本次实验采用Internet协议版本4（IPv4）地址族、流式套接字以及TCP协议
	if (clientSocket == INVALID_SOCKET) {
		cout << "[错误] 创建Socket失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // 释放 Socket DLL 资源
		system("pause");
		return 1;
	}
	cout << "[系统提示] 创建Socket成功！" << endl;
	//============================服务器地址===========================
	SOCKADDR_IN serverAddr;  // 服务端地址信息的数据结构IPv4
	serverAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // 地址
	//inet_pton(AF_INET, "192.168.192.1", &serverAddr.sin_addr.s_addr);  // 地址
	serverAddr.sin_port = htons(PORT);  // 端口号
	//===========================连接 connect==========================
	err = connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {
		cout << "[错误] 服务器连接失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		system("pause");
		return 1;
	}
	cout << "[系统提示] 服务器连接成功！" << endl;

	//========================打印进入聊天的标志========================
	recv(clientSocket, user_id, 10, 0);
	// 获取时间
	t = time(NULL);
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "===============================================" << endl;
	cout << "             欢迎    用户[" << user_id << "]" << endl;
	cout << "***********************************************" << endl;
	cout << "        退出说明：" << endl;
	cout << "        $ exit/Exit/EXIT" << endl;
	cout << "        $ quit/Quit/QUIT" << endl;
	cout << "        $ 关闭程序" << endl;
	cout << "-----------------------------------------------" << endl;
	cout << "        时间：" << time_str;
	cout << "===============================================" << endl;
	//========================创建接收消息的线程=======================
	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recvThread, LPVOID(clientSocket), 0, NULL);
	//==============================主线程=============================
	while (true) {
		cout << "用户[" << user_id << "]>";
		char buf[MaxBufSize];
		cin.getline(buf, sizeof(buf));
		if (strcmp(buf, "exit") == 0 || strcmp(buf, "Exit") == 0 || strcmp(buf, "EXIT") == 0
			|| strcmp(buf, "quit") == 0 || strcmp(buf, "Quit") == 0 || strcmp(buf, "QUIT") == 0)
		{  // 退出判断
			break;
		}
		// 发送
		send(clientSocket, buf, sizeof(buf), 0);
	}
	CloseHandle(hThread);
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

// 接收消息的进程处理函数
DWORD WINAPI recvThread()
{
	while (1)
	{
		char RecvBuf[MaxBufSize];  // 接收消息的缓冲区
		int recvResult = recv(clientSocket, RecvBuf, sizeof(RecvBuf), 0);
		if (recvResult > 0)
		{
			cout << "\r";
			cout << "用户[" << user_id << "]<" << " 收到消息! " << endl;
			cout << "$$ " << RecvBuf << endl;
			// 获取时间
			t = time(NULL);
			ctime_s(time_str, sizeof(time_str), &t);
			cout << "$$ 【接收消息时间】:" << time_str;
			cout << "用户[" << user_id << "]>";
		}
		else if (recvResult < 0)  // 断开连接
		{
			cout << "[警告] 断开连接..." << endl;
			cout << "或许是服务器端程序出现了什么问题？" << endl;
			break;
		}
	}
	return 0;
}