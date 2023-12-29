#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // 套接字接口
#include <Ws2tcpip.h>  // 套接字新函数库
#include <time.h>
#include <fstream>
#include <unordered_map>
#include "MessageFormat.h"  // 数据报格式
#pragma comment (lib, "ws2_32.lib")  // 套接字实现
using namespace std;
#define RouterPORT 30000  // 路由器端口号
#define ServerPORT 10000  // 服务器端口号

int connect_seq = 0;  // 与前一个实验不同，这一次这个变量只与我们的连接相关
MsgFormat acktmp;  // 该变量用于数据传输过程中校验和错误时重传

// 关于滑动窗口的变量
const int N = 5;  // 滑动窗口大小
int base = 0;  // 基序号
int nextseqnum = 0;  // 下一个序号
bool* acked;  // 确认标记

// 建连函数
bool myconnect(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	//============================第一次握手===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // 第一次握手数据报
	//============================第二次握手===========================
	// SYN = 1, ACK = 1, seq = w, ack = x
	MsgFormat handshake2;  // 第二次握手数据报
	//============================第三次握手===========================
	// ACK = 1, seq = x + 1, ack = w
	MsgFormat handshake3;  // 第三次握手数据报
	while (1) {
		//============================第一次握手===========================
		int recvByte = recvfrom(serverSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[错误] 连接失败..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(handshake1.Flag && SYN) || !handshake1.check() || !(handshake1.seq == connect_seq)) {
				cout << "[错误] 连接发生错误..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			// cout << "接收handshake1.seq =" << handshake1.seq << "handshake1.ack =" << handshake1.ack << endl;
			cout << "[系统提示] 第一次握手报文接收成功！" << endl;
			break;
		}
	}
	//============================第二次握手===========================
	handshake2.SrcPort = ServerPORT;
	handshake2.DestPort = RouterPORT;
	handshake2.seq = connect_seq;
	connect_seq++;
	handshake2.ack = handshake1.seq;
	handshake2.Flag += SYN;
	handshake2.Flag += ACK;
	handshake2.setCheckNum();
	int err = sendto(serverSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&clientAddr, addrlen);
	// cout << "发送handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
	clock_t handshake2start = clock();
	if (err == -1) {
		cout << "[错误] 连接失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第二次握手报文发送成功！" << endl;
	//============================第三次握手===========================
	while (recvfrom(serverSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&clientAddr, &addrlen) <= 0) {
		//	第二次握手超时，重新发送并重新计时
		if (clock() - handshake2start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[系统提示] 第二次握手超时，正在重传..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(serverSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&clientAddr, addrlen);
			handshake2start = clock();
			if (err == -1) {
				cout << "[错误] 连接失败..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((handshake3.Flag && ACK) && handshake3.check() && (handshake3.ack == handshake2.seq) && (handshake3.seq == connect_seq)) {
		// cout << "接收handshake3.seq =" << handshake3.seq << "handshake3.ack =" << handshake3.ack << endl;
		cout << "[系统提示] 第三次握手报文确认成功！" << endl;
		cout << "[系统提示] 连接成功！" << endl;
		return true;
	}
	else {
		cout << "[错误] 连接发生错误..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
}
// 报文接收辅助函数
// bool msgRecv(MsgFormat& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr, int& expectedseqnum) {
bool msgRecv(MsgFormat& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr, unordered_map<int, MsgFormat>& receivedPackets) {
	int addrlen = sizeof(clientAddr);
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (recvMsg.check()) {
				if (recvMsg.seq >= base && recvMsg.seq <= base + N - 1) {  // 在窗口内
					if (recvMsg.seq == base) {  // 接收左边界
						//收到顺序的数据包
						receivedPackets[recvMsg.seq] = recvMsg;
						MsgFormat ackMsg;
						ackMsg.SrcPort = ServerPORT;
						ackMsg.DestPort = RouterPORT;
						ackMsg.Flag += ACK;
						ackMsg.ack = recvMsg.seq;
						ackMsg.setCheckNum();
						sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
						cout << "[传输日志] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
						cout << "[传输日志] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
						// expectedseqnum++;
						acked[base] = true;
						while (acked[base]) {
							base++;
						}
						return true;
					}
					else {  // 窗口内其他包
						if (!acked[recvMsg.seq]) {
							acked[recvMsg.seq] = true;
							//收到顺序的数据包
							receivedPackets[recvMsg.seq] = recvMsg;
							MsgFormat ackMsg;
							ackMsg.SrcPort = ServerPORT;
							ackMsg.DestPort = RouterPORT;
							ackMsg.Flag += ACK;
							ackMsg.ack = recvMsg.seq;
							ackMsg.setCheckNum();
							sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
							cout << "[传输日志] [失序缓存]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
							cout << "[传输日志]  " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
							return true;
						}
					}
				}
				else if (recvMsg.seq >= base - N && recvMsg.seq <= base - 1) {  // 前一个窗口
					MsgFormat ackMsg;
					ackMsg.SrcPort = ServerPORT;
					ackMsg.DestPort = RouterPORT;
					ackMsg.Flag += ACK;
					ackMsg.ack = recvMsg.seq;
					ackMsg.setCheckNum();
					sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
					cout << "[传输日志] [已缓存] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
					cout << "[传输日志] [回复ACK]" << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
				}
			}
		}
		else if (recvByte == 0) {
			return false;
		}
	}
}
// 报文接收函数
void RecvFunc(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	// int expectedseqnum = 0;  // 期待接收的序列号
	int addrlen = sizeof(clientAddr);
	// int expected_seq = 0;
	// 接收端缓冲区
	unordered_map<int, MsgFormat> receivedPackets;
	//===========================文件名和大小==========================
	MsgFormat rMsg;
	unsigned int filesize;
	char filename[50] = { 0 };
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&rMsg, sizeof(rMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (rMsg.check() && ((rMsg.seq == base))) {
				filesize = rMsg.size;  // 获取文件大小
				for (int i = 0; rMsg.MSGDataBuf[i]; i++)  // 获取文件名
					filename[i] = rMsg.MSGDataBuf[i];
				cout << "-----------------------------------------------" << endl;
				cout << "[传输日志] 接收文件：" << filename << "，文件大小：" << filesize << "B" << endl;
				cout << "-----------------------------------------------" << endl;
				MsgFormat ackMsg;
				ackMsg.SrcPort = ServerPORT;
				ackMsg.DestPort = RouterPORT;
				ackMsg.Flag += ACK;
				ackMsg.ack = rMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[传输日志] " << rMsg.SrcPort << " -> " << rMsg.DestPort << " size = " << rMsg.size << "B seq = " << rMsg.seq << " Flag = " << rMsg.Flag << endl;
				cout << "[传输日志] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
				// expectedseqnum++;
				base++;
				break;
			}
		}
	}
	//===========================文件数据部分==========================
	int batchNum = filesize / MAX_MSG_SIZE;  // 满载报文数
	int leftSize = filesize % MAX_MSG_SIZE;  // 剩余报文大小
	BYTE* fileBuffer = new BYTE[filesize];
	cout << "[传输日志] 开始接收数据，共 " << batchNum << " 个满载报文段，非满载报文大小：" << leftSize << endl;
	int pkt_amount = leftSize > 0 ? batchNum + 2 : batchNum + 1;
	acked = new bool[pkt_amount];
	memset(acked, false, pkt_amount);
	int i = 0;
	while (i < pkt_amount - 1) {
		MsgFormat dMsg;
		if (msgRecv(dMsg, serverSocket, clientAddr, receivedPackets))
			i++;
			// cout << "[传输日志] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
		else {
			cout << "[传输日志] 数据接收失败..." << endl;
			cout << "===============================================" << endl;
			return;
		}
		if (dMsg.seq == batchNum + 1 && leftSize > 0) {
			for (int j = 0; j < leftSize; j++) {
				fileBuffer[batchNum * MAX_MSG_SIZE + j] = receivedPackets[dMsg.seq].MSGDataBuf[j];
			}
		}
		else {
			for (int j = 0; j < MAX_MSG_SIZE; j++) {  // 读取数据部分
				fileBuffer[(dMsg.seq - 1) * MAX_MSG_SIZE + j] = receivedPackets[dMsg.seq].MSGDataBuf[j];
			}
		}
			
	//	while (pkt_num > 0) {
	//		for (int j = 0; j < MAX_MSG_SIZE; j++) {  // 读取数据部分
	//			fileBuffer[i * MAX_MSG_SIZE + j] = receivedPackets[base - pkt_num].MSGDataBuf[j];
	//		}
	//		pkt_num--;
	//		i++;
	//	}
	//}
	//if (leftSize > 0) {
	//	MsgFormat dMsg;
	//	if (msgRecv(dMsg, serverSocket, clientAddr, receivedPackets));
	//		// cout << "[传输日志] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
	//	else {
	//		cout << "[传输日志] 数据接收失败..." << endl;
	//		cout << "===============================================" << endl;
	//		return;
	//	}

	//	for (int j = 0; j < leftSize; j++) {
	//		fileBuffer[batchNum * MAX_MSG_SIZE + j] = dMsg.MSGDataBuf[j];
	//	}
	}
	cout << "[传输日志] 数据接收成功！正在写入本地..." << endl;
	ofstream f(filename, ofstream::binary);  // 以二进制方式读取文件
	if (f.is_open()) {
		f.write(reinterpret_cast<const char*>(fileBuffer), filesize);
		f.close();
		cout << "[传输日志] 文件写入成功！" << endl;
		cout << "===============================================" << endl;
	}
}
// 断连函数
bool mydisconnect(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	//============================第一次挥手===========================
	// FIN = 1, seq = y
	MsgFormat wave1;  // 第一次挥手数据报
	//============================第二次挥手===========================
	// FIN = 1, ACK = 1, seq = z, ack = y
	MsgFormat wave2;  // 第二次挥手数据报
	//============================第三次挥手===========================
	// ACK = 1, seq = y + 1, ack = z
	MsgFormat wave3;  // 第三次挥手数据报
	while (1) {
		//============================第一次挥手===========================
		int recvByte = recvfrom(serverSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[错误] 断连失败..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(wave1.Flag && FIN) || !wave1.check() || !(wave1.seq == connect_seq + 1)) {
				cout << "[错误] 断连发生错误..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			cout << "[系统提示] 第一次挥手报文接收成功！" << endl;
			break;
		}
	}
	//============================第二次挥手===========================
	wave2.SrcPort = ServerPORT;
	wave2.DestPort = RouterPORT;
	wave2.seq = connect_seq;
	connect_seq++;
	wave2.ack = wave1.seq;
	wave2.Flag += FIN;
	wave2.Flag += ACK;
	wave2.setCheckNum();
	int err = sendto(serverSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&clientAddr, addrlen);
	clock_t wave2start = clock();
	if (err == -1) {
		cout << "[错误] 断连失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第二次挥手报文发送成功！" << endl;
	//============================第三次挥手===========================
	while (recvfrom(serverSocket, (char*)&wave3, sizeof(wave3), 0, (sockaddr*)&clientAddr, &addrlen) <= 0) {
		//	第二次挥手超时，重新发送并重新计时
		if (clock() - wave2start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[系统提示] 第二次挥手超时，正在重传..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(serverSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&clientAddr, addrlen);
			wave2start = clock();
			if (err == -1) {
				cout << "[错误] 断连失败..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((wave3.Flag && ACK) && wave3.check() && (wave3.ack == wave2.seq)) {
		cout << "[系统提示] 第三次挥手报文确认成功！" << endl;
		cout << "[系统提示] 断连成功！" << endl;
		return true;
	}
	else {
		cout << "[错误] 断连发生错误..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
}
int main() {
	cout << "===============================================" << endl;
	cout << "==================叡神服务器===================" << endl;
	cout << "===============================================" << endl;
	cout << "叡神客户端[版本 23.12.25.9]      开发者-Yuzhao-" << endl;
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
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);  // 本次实验采用Internet协议版本4（IPv4）地址族、数据报套接字
	u_long mode = 1;
	ioctlsocket(serverSocket, FIONBIO, &mode);  // 非阻塞
	if (serverSocket == INVALID_SOCKET) {
		cout << "[错误] 创建Socket失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // 释放 Socket DLL 资源
		system("pause");
		return 1;
	}
	cout << "[系统提示] 创建Socket成功！" << endl;
	//============================服务器地址===========================
	SOCKADDR_IN serverAddr;  // 服务器地址信息的数据结构IPv4
	serverAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // 地址
	serverAddr.sin_port = htons(ServerPORT);  // 端口号
	//=========================客户端/路由器地址=======================
	SOCKADDR_IN clientAddr;  // 客户端地址信息的数据结构IPv4
	clientAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr.s_addr);  // 地址
	clientAddr.sin_port = htons(RouterPORT);  // 端口号
	//=============================绑定 bind===========================
	err = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (err == SOCKET_ERROR) {
		cout << "[错误] bind 绑定失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		closesocket(serverSocket);
		WSACleanup();
		system("pause");
		return 1;
	}
	cout << "[系统提示] bind 绑定端口 " << ServerPORT << " 成功！" << endl;
	cout << "[系统提示] 等待客户端连接..." << endl;
	//===========================连接 connect==========================
	bool res = myconnect(serverSocket, clientAddr);
	if (!res) {  // 连接失败
		system("pause");
		return 1;
	}
	//=============================系统简介============================
	cout << "===============================================" << endl;
	cout << "               欢迎使用叡神传输                " << endl;
	cout << "***********************************************" << endl;
	cout << "  * 使用说明：" << endl;
	cout << "    $ 输入 q/Q 关闭连接" << endl;
	cout << "    $ 输入 r/R 传输文件" << endl;
	cout << "  * 标志位说明：" << endl;
	cout << "    $ FIN = 0000_0000_0000_0001" << endl;
	cout << "    $ SYN = 0000_0000_0000_0010" << endl;
	cout << "    $ ACK = 0000_0000_0000_0100" << endl;
	cout << "    $ FILEFLAG = 0000_0000_0000_1000" << endl;
	cout << "    $ FILEDATA = 0000_0000_0001_0000" << endl;
	cout << "    $ 组合标志位为各项加和，最终输出的为十进制数" << endl;
	cout << "-----------------------------------------------" << endl;
	// 获取时间
	time_t t = time(NULL);
	char time_str[64];
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "        时间：" << time_str;
	cout << "===============================================" << endl;
	//=============================文件传输============================
	int addrlen = sizeof(clientAddr);
	MsgFormat qMsg;
	RecvFunc(serverSocket, clientAddr);
	//========================关闭连接 disconnect======================
	cout << "[系统提示] 文件写入完毕等待用户断开连接..." << endl;
	mydisconnect(serverSocket, clientAddr);
	closesocket(serverSocket); //关闭socket
	WSACleanup();
	system("pause");
	return 0;
}