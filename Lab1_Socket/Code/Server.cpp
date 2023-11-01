#include<iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <winsock2.h>  // 套接字接口
#include <Ws2tcpip.h>  // 套接字新函数库
#include <time.h>
#include<map>
#include<thread>
#pragma comment (lib, "ws2_32.lib")  // 套接字实现

#define PORT 8000  // 监听端口
#define MaxBufSize 1024  // 最大缓冲区大小
using namespace std;
time_t t;
char time_str[64];
// 用户状态
map<SOCKET, int> user_map;  // 其中的int如果为1则为在线
SOCKET sockSrv;
bool isServerRunning = true;
// 线程处理函数声明
DWORD WINAPI handlerRequest(LPVOID lparam);

// 退出处理线程函数
void ServerInputThread() {
	string input;
	while (isServerRunning) {
		getline(cin, input);
		if (input == "exit" || input == "Exit" || input == "EXIT" 
			|| input == "quit" || input == "Quit" || input == "QUIT")
		{  // 退出判断
			isServerRunning = false;
			closesocket(sockSrv); // 关闭服务器socket
			break;
		}
	}
}

int main()
{
	cout << "===============================================" << endl;
	cout << "==================叡神服务器===================" << endl;
	cout << "===============================================" << endl;
	cout << "叡神服务器[版本 23.10.18.0]      开发者-Yuzhao-" << endl;
	cout << "===============================================" << endl;
	cout << "==================服务器准备===================" << endl;
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
	sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 本次实验采用Internet协议版本4（IPv4）地址族、流式套接字以及TCP协议
	if (sockSrv == INVALID_SOCKET) {
		cout << "[错误] 创建Socket失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // 释放 Socket DLL 资源
		system("pause");
		return 1;
	}
	cout << "[系统提示] 创建Socket成功！" << endl;
	//===================初始化服务器地址·bind（绑定）=================
	SOCKADDR_IN serverAddr;  // 服务端地址信息的数据结构IPv4
	serverAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // 地址
	//inet_pton(AF_INET, "192.168.192.1", &serverAddr.sin_addr.s_addr);  // 地址
	serverAddr.sin_port = htons(PORT);  // 端口号
	err = bind(sockSrv, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (err == SOCKET_ERROR) {
		cout << "[错误] bind 绑定失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		closesocket(sockSrv);
		WSACleanup();
		system("pause");
		return 1;
	}
	cout << "[系统提示] bind 绑定端口 " << PORT << " 成功！" << endl;
	//============================监听 listen==========================
	err = listen(sockSrv, SOMAXCONN); // 最大监听队列长度为SOMAXCONN
	if (err == SOCKET_ERROR)
	{
		cout << "[错误] 监听失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		system("pause");
		return 1;
	}
	cout << "[系统提示] 监听成功！" << endl;
	// 输出服务器准备完成提示
	cout << "===============================================" << endl;
	// 获取时间
	t = time(NULL);
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "[叡神] 启动！！！" << endl;
	cout << "[系统提示] 服务器准备完成！等待客户端连接......" << endl;
	cout << "[系统时间] " << time_str;
	cout << "===============================================" << endl;
	thread inputThread(ServerInputThread);
	cout << "***********************************************" << endl;
	cout << "        退出说明：" << endl;
	cout << "        $ exit/Exit/EXIT" << endl;
	cout << "        $ quit/Quit/QUIT" << endl;
	cout << "        $ 关闭程序" << endl;
	cout << "-----------------------------------------------" << endl;
	cout << "===================系统日志====================" << endl;
	//============================处理Client===========================
	//while (true) {
	//	SOCKADDR_IN clientAddr;
	//	int len = sizeof(SOCKADDR);
	//	// 接收函数accept()
	//	SOCKET sockAccept = accept(sockSrv, (SOCKADDR*)&clientAddr, &len);
	//	if (sockAccept == INVALID_SOCKET) {
	//		cout << "[错误] accept 失败，错误代码: " << WSAGetLastError() << endl;
	//		cout << "===============================================" << endl;
	//		closesocket(sockSrv);
	//		WSACleanup();
	//		system("pause");
	//		return 1;
	//	}
	//	else {
	//		// 获取时间
	//		t = time(NULL);
	//		ctime_s(time_str, sizeof(time_str), &t);
	//		// 将Socket转为字符串作为用户ID
	//		char user_id[10];
	//		strcpy_s(user_id, to_string(sockAccept).data());
	//		cout << "[系统日志] " << time_str;
	//		cout << "[系统日志] 用户[" << user_id << "]已加入聊天!"<< endl;
	//		cout << "-----------------------------------------------" << endl;
	//		// 创建线程处理Client
	//		HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(sockAccept), 0, NULL);
	//		// 关闭线程引用
	//		CloseHandle(hThread);
	//	}
	//}
	while (isServerRunning) {
		SOCKADDR_IN clientAddr;
		int len = sizeof(SOCKADDR);
		// 接收函数accept()
		SOCKET sockAccept = accept(sockSrv, (SOCKADDR*)&clientAddr, &len);
		if (!isServerRunning) {
			// Exit command received, don't accept new connections
			break;
		}
		if (sockAccept == INVALID_SOCKET) {
			cout << "[错误] accept 失败，错误代码: " << WSAGetLastError() << endl;
			cout << "===============================================" << endl;
			closesocket(sockSrv);
			WSACleanup();
			system("pause");
			return 1;
		}
		else {
			// 获取时间
			t = time(NULL);
			ctime_s(time_str, sizeof(time_str), &t);
			// 将Socket转为字符串作为用户ID
			char user_id[10];
			strcpy_s(user_id, to_string(sockAccept).data());
			cout << "[系统日志] " << time_str;
			cout << "[系统日志] 用户[" << user_id << "]已加入聊天!" << endl;
			cout << "-----------------------------------------------" << endl;
			// 创建线程处理Client
			HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(sockAccept), 0, NULL);
			// 关闭线程引用
			CloseHandle(hThread);
		}
	}

	inputThread.join(); // 等待输入线程
	//============================关闭Socket===========================
	closesocket(sockSrv);  // 【真的不想写错误处理函数了（多半不会出现）】
	WSACleanup();
	return 0;
}


// 线程处理函数
DWORD WINAPI handlerRequest(LPVOID lparam)
{
	SOCKET ClientSocket = (SOCKET)lparam;
	user_map[ClientSocket] = 1;  // 置1，为在线
	char RecvBuf[MaxBufSize];  // 接收消息的缓冲区
	char SendBuf[MaxBufSize];  // 发送消息的缓冲区
	// 将Socket转为字符串作为用户ID
	char user_id[10];
	strcpy_s(user_id, to_string(ClientSocket).data());

	//=============================发送 send===========================
	send(ClientSocket, user_id, 10, 0);

	// 循环接受客户端数据
	int recvResult;
	int sendResult;
	//==============================循环处理===========================
	do {
		// 接收 recv
		recvResult = recv(ClientSocket, RecvBuf, MaxBufSize, 0);
		if (recvResult > 0) {
			strcpy_s(SendBuf, "用户[");
			string ClientID = to_string(ClientSocket);
			strcat_s(SendBuf, ClientID.data());
			strcat_s(SendBuf, "]: ");
			strcat_s(SendBuf, RecvBuf);

			// 获取时间
			t = time(NULL);
			ctime_s(time_str, sizeof(time_str), &t);
			cout << "[系统日志] " << time_str;
			cout << "[系统日志] 用户[" << ClientID << "]的消息：" << RecvBuf << endl;
			cout << "-----------------------------------------------" << endl;
			for (auto it : user_map) {
				if (it.first != ClientSocket && it.second == 1) {
					sendResult = send(it.first, SendBuf, MaxBufSize, 0);
					if (sendResult == SOCKET_ERROR) {
						cout << "[错误] 信息发送失败，错误代码: " << WSAGetLastError() << endl;
						cout << "-----------------------------------------------" << endl;
					}
				}
			}
		}
	} while (recvResult != SOCKET_ERROR);

	// 获取时间
	t = time(NULL);
	ctime_s(time_str, sizeof(time_str), &t);
	// 获取用户ID
	string ClientID = to_string(ClientSocket);
	user_map[ClientSocket] = 0;
	cout << "[系统日志] " << time_str;
	cout << "[系统日志] 用户[" << ClientID << "]已退出聊天..." << endl;
	cout << "-----------------------------------------------" << endl;
	closesocket(ClientSocket);
	return 0;
}

