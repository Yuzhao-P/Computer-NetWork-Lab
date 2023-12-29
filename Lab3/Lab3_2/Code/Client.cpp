#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // 套接字接口
#include <Ws2tcpip.h>  // 套接字新函数库
#include <time.h>
#include <fstream>
#include <mutex>
#include "MessageFormat.h"  // 数据报格式
#pragma comment (lib, "ws2_32.lib")  // 套接字实现
using namespace std;
#define RouterPORT 30000  // 路由器端口号
#define ClientPORT 20000  // 客户端端口号

int connect_seq = 0;  // 与前一个实验不同，这一次这个变量只与我们的连接相关
MsgFormat sendtmp;  // 该变量用于数据传输过程中校验和错误时重传
int run_flag = 1;  // 标记是否为第一次启动
bool over = false;  // 传输是否结束

mutex outputMutex;  // 全局互斥锁

// 关于滑动窗口的变量
int N = 5;  // 滑动窗口大小
int base = 0;  // 基序号
int nextseqnum = 0;  // 下一个序号
clock_t time_count;  // 标记报文发送起始时间

// 建连函数
bool myconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================第一次握手===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // 第一次握手数据报
	handshake1.SrcPort = ClientPORT;
	handshake1.DestPort = RouterPORT;
	handshake1.seq = connect_seq;
	connect_seq++;
	handshake1.Flag += SYN;
	handshake1.setCheckNum();
	int err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
	// cout << "发送handshake1.seq =" << handshake1.seq << "handshake1.ack =" << handshake1.ack << endl;
	clock_t handshake1start = clock();
	if (err == -1) {
		cout << "[错误] 连接失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第一次握手报文发送成功！" << endl;
	//============================第二次握手===========================
	// SYN = 1, ACK = 1, seq = w, ack = x
	MsgFormat handshake2;  // 第二次握手数据报
	while (recvfrom(clientSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
		//	第一次握手超时，重新发送并重新计时
		if (clock() - handshake1start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[系统提示] 第一次握手超时，正在重传..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
			handshake1start = clock();
			if (err == -1) {
				cout << "[错误] 连接失败..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((handshake2.Flag && SYN) && (handshake2.Flag && ACK) && handshake2.check() && (handshake2.ack == handshake1.seq)) {
		// cout << "接收handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
		cout << "[系统提示] 第二次握手报文确认成功！" << endl;
	}
	else {
		cout << "[错误] 连接发生错误..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	//============================第三次握手===========================
	// ACK = 1, seq = x + 1, ack = w
	MsgFormat handshake3;  // 第三次握手数据报
	handshake3.SrcPort = ClientPORT;
	handshake3.DestPort = RouterPORT;
	handshake3.seq = connect_seq;
	connect_seq++;
	handshake3.ack = handshake2.seq;
	handshake3.Flag += ACK;
	handshake3.setCheckNum();
	// cout << "发送handshake3.seq =" << handshake3.seq << "handshake3.ack =" << handshake3.ack << endl;
	err = sendto(clientSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&serverAddr, addrlen);
	clock_t handshake3start = clock();
	if (err == -1) {
		cout << "[错误] 连接失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第三次握手报文发送成功！" << endl;
	// 在上一次报告探索中我说对于TCP而言第三次ACK丢失没什么问题是能够正常发送数据的，但是经过调试发现我的程序并不可以，因此我们仿照四次挥手的处理方式增加以下代码处理重传
	//=======================等待2MSL防止ACK丢失=======================
	int waittime = clock();
	MsgFormat tmp;
	while (clock() - waittime < 2 * MAX_WAIT_TIME) {
		int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[系统提示] 连接发生错误..." << endl;
			return false;
		}
		else if (recvByte > 0) {
			sendto(clientSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[系统提示] 重传ACK" << endl;
		}
	}
	cout << "[系统提示] 连接成功！" << endl;
	return true;
}
// 报文传输辅助函数——接收线程
struct parameters {
	SOCKADDR_IN serverAddr;
	SOCKET clientSocket;
	int pkt_amount;  // 报文总数：相对序列号不会比这个大
};
DWORD WINAPI ACKRecvThread(PVOID pParam) {
	parameters* para = (parameters*)pParam;
	SOCKADDR_IN serverAddr = para->serverAddr;
	SOCKET clientSocket = para->clientSocket;
	int pkt_amount = para->pkt_amount;
	int addrlen = sizeof(serverAddr);
	
	while (1) {
		MsgFormat recvMsg;
		int recvByte = recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&serverAddr, &addrlen);
		lock_guard<mutex> lock(outputMutex);  // 加锁
		if (recvByte > 0) {  // 成功收到消息【不管是啥，收到东西了】
			if ((recvMsg.Flag && ACK) && recvMsg.check()) {  // 检查校验和
				// 注意：为了防止错误的左移滑动窗口【失序报文ACK返回】，我们需要确保当前的ack是在[base, nextseqnum - 1]内的
				if (recvMsg.ack < base);
				else
					base = recvMsg.ack + 1;  // 滑动窗口滑动
				if (base != nextseqnum)
					time_count = clock();  // 更新时间
				cout << "[传输日志] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
				if (base + N < pkt_amount) {  // 展示窗口情况
					cout << "[传输日志] [接收ACK]当前窗口情况——窗口总大小：" << N << "，已发送但未收到ACK的数据报数目：" << nextseqnum - base << "，窗口内尚未发送数据报数目：" << N - (nextseqnum - base) << endl;
				}
				else {// 展示窗口情况
					cout << "[传输日志] [接收ACK]当前窗口情况——窗口总大小：" << N << "，已发送但未收到ACK的数据报数目：" << nextseqnum - base << "，窗口内尚未发送数据报数目：" << pkt_amount - base - (nextseqnum - base) << endl;
				}
				// 判断传输结束
				if (recvMsg.ack == pkt_amount - 1) {
					cout << "[传输日志] **********传输结束！**********" << endl;
					over = true;
					return 0;
				}
			}
		}
	}
	return 0;
}
// 报文传输函数
void GBN_SendFunc(string path, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	clock_t startTime = clock();
	string filename = "";
	for (int i = path.size() - 1; i >= 0; i--) {  // 逆序获取逆序文件名
		if (path[i] == '/' || path[i] == '\\')
			break;
		filename += path[i];
	}
	filename = string(filename.rbegin(), filename.rend());  // 逆序获取正序文件名
	ifstream f(path.c_str(), ifstream::binary);  // 以二进制方式读取文件
	if (!f) {
		cout << "[传输日志] 无法打开文件..." << endl;
		return;
	}
	BYTE* fileBuffer = new BYTE[MAX_FILE_SIZE];
	unsigned int fileSize = 0;
	BYTE byte = f.get();
	while (f) {  // 将文件读取到缓冲区
		fileBuffer[fileSize++] = byte;
		byte = f.get();
	}
	f.close();

	int batchNum = fileSize / MAX_MSG_SIZE;  // 满载报文数
	int leftSize = fileSize % MAX_MSG_SIZE;  // 剩余报文大小

	//=========================创建接受消息线程========================
	int pkt_amount = leftSize > 0 ? batchNum + 2 : batchNum + 1;  // +1是由于第一个文件名字及大小的包占一个
	parameters param;
	param.serverAddr = serverAddr;
	param.clientSocket = clientSocket;
	param.pkt_amount = pkt_amount;
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ACKRecvThread, &param, 0, 0);
	//===========================主线程：发送==========================
	MsgFormat sendMsg;
	while (1) {
		lock_guard<mutex> lock(outputMutex);  // 加锁
		if (nextseqnum < base + N && nextseqnum < pkt_amount) {
			if (nextseqnum == 0) {  // 文件名字及大小
				sendMsg.SrcPort = ClientPORT;
				sendMsg.DestPort = RouterPORT;
				sendMsg.size = fileSize;
				sendMsg.Flag += FILEFLAG;
				sendMsg.seq = nextseqnum;
				for (int i = 0; i < filename.size(); i++)  // 填充报文数据段
				sendMsg.MSGDataBuf[i] = filename[i];
				sendMsg.MSGDataBuf[filename.size()] = '\0';//字符串结尾补\0
				sendMsg.setCheckNum();
			}
			else if (nextseqnum == batchNum + 1 && leftSize > 0) {  // 剩余数据
				sendMsg.SrcPort = ClientPORT;
				sendMsg.DestPort = RouterPORT;
				sendMsg.size = leftSize;
				sendMsg.Flag = FILEDATA;
				sendMsg.seq = nextseqnum;
				for (int j = 0; j < leftSize; j++) {
					sendMsg.MSGDataBuf[j] = fileBuffer[batchNum * MAX_MSG_SIZE + j];
				}
				sendMsg.setCheckNum();
			}
			else {
				sendMsg.SrcPort = ClientPORT;
				sendMsg.DestPort = RouterPORT;
				sendMsg.size = MAX_MSG_SIZE;
				sendMsg.Flag = FILEDATA;
				sendMsg.seq = nextseqnum;
				for (int j = 0; j < MAX_MSG_SIZE; j++) {
					sendMsg.MSGDataBuf[j] = fileBuffer[(nextseqnum - 1) * MAX_MSG_SIZE + j];
				}
				sendMsg.setCheckNum();
			}
			// 发送
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[传输日志] " << sendMsg.SrcPort << " -> " << sendMsg.DestPort << " size = " << sendMsg.size << "B seq = " << sendMsg.seq << " Flag = " << sendMsg.Flag << endl;
			if (base == nextseqnum) {
				time_count = clock();
			}
			nextseqnum++;
			if (base + N < pkt_amount) {  // 展示窗口情况
				cout << "[传输日志] [发送数据报]当前窗口情况——窗口总大小：" << N << "，已发送但未收到ACK的数据报数目：" << nextseqnum - base << "，窗口内尚未发送数据报数目：" << N - (nextseqnum - base) << endl;
			}
			else {// 展示窗口情况
				cout << "[传输日志] [发送数据报]当前窗口情况——窗口总大小：" << N << "，已发送但未收到ACK的数据报数目：" << nextseqnum - base << "，窗口内尚未发送数据报数目：" << pkt_amount - base - (nextseqnum - base) << endl;
			}
		}
		// 超时重传
		if (clock() - time_count > MAX_WAIT_TIME) {  // GBN——当前窗口内的所有数据报重传
			for (int i = 0; i < nextseqnum - base; i++) {
				int send_seq = base + i;
				if (send_seq == 0) {
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.size = fileSize;
					sendMsg.Flag += FILEFLAG;
					sendMsg.seq = send_seq;
					for (int i = 0; i < filename.size(); i++)  // 填充报文数据段
						sendMsg.MSGDataBuf[i] = filename[i];
					sendMsg.MSGDataBuf[filename.size()] = '\0';//字符串结尾补\0
					sendMsg.setCheckNum();
					cout << "[传输日志] 传输文件相关信息——文件名：" << filename << " 文件大小：" << fileSize << "B" << endl;
				}
				else if (send_seq == batchNum + 1 && leftSize > 0) {
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.size = leftSize;
					sendMsg.Flag = FILEDATA;
					sendMsg.seq = send_seq;
					for (int j = 0; j < leftSize; j++) {
						sendMsg.MSGDataBuf[j] = fileBuffer[batchNum * MAX_MSG_SIZE + j];
					}
					sendMsg.setCheckNum();
				}
				else {
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.size = MAX_MSG_SIZE;
					sendMsg.Flag = FILEDATA;
					sendMsg.seq = send_seq;
					for (int j = 0; j < MAX_MSG_SIZE; j++) {
						sendMsg.MSGDataBuf[j] = fileBuffer[(send_seq - 1) * MAX_MSG_SIZE + j];
					}
					sendMsg.setCheckNum();
				}
				// 发送
				sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
				cout << "[传输日志] seq = " << sendMsg.seq << "的报文段超时，正在重传..." << endl;
			}
			time_count = clock();
		}
		if (over == true) break;  // 已收到所有ACK
	}
	CloseHandle(hThread);
	cout << "[传输日志] 已发送并确认所有报文，文件传输成功！" << endl;
	clock_t endTime = clock();
	cout << "-----------------------------------------------" << endl;
	cout << "[传输日志] 总体传输时间为:" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	cout << "[传输日志] 吞吐率:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl;
	cout << "===============================================" << endl;
	return;
}
// 断连函数
bool mydisconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================第一次挥手===========================
	// FIN = 1, seq = y
	MsgFormat wave1;  // 第一次挥手数据报
	wave1.SrcPort = ClientPORT;
	wave1.DestPort = RouterPORT;
	wave1.Flag += FIN;
	wave1.seq = connect_seq;
	connect_seq++;
	wave1.setCheckNum();
	int err = sendto(clientSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&serverAddr, addrlen);
	clock_t wave1start = clock();
	if (err == -1) {
		cout << "[错误] 断连失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第一次挥手报文发送成功！" << endl;
	//============================第二次挥手===========================
	// FIN = 1, ACK = 1, seq = z, ack = y
	MsgFormat wave2;  // 第二次挥手数据报
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // 非阻塞
	while (recvfrom(clientSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
		//	第一次挥手超时，重新发送并重新计时
		if (clock() - wave1start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[系统提示] 第一次挥手超时，正在重传..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(clientSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&serverAddr, addrlen);
			wave1start = clock();
			if (err == -1) {
				cout << "[错误] 断连失败..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((wave2.Flag && FIN) && (wave2.Flag && ACK) && wave2.check() && (wave2.ack == wave1.seq)) {
		cout << "[系统提示] 第二次挥手报文确认成功！" << endl;
	}
	else {
		cout << "[错误] 断连发生错误..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	//============================第三次挥手===========================
	// ACK = 1, seq = y + 1, ack = z
	MsgFormat wave3;  // 第三次握手数据报
	wave3.SrcPort = ClientPORT;
	wave3.DestPort = RouterPORT;
	wave3.seq = connect_seq;
	connect_seq++;
	wave3.ack = wave2.seq;
	wave3.Flag += ACK;
	wave3.setCheckNum();
	clock_t wave3start = clock();
	err = sendto(clientSocket, (char*)&wave3, sizeof(wave3), 0, (sockaddr*)&serverAddr, addrlen);
	if (err == -1) {
		cout << "[错误] 断连失败..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[系统提示] 第三次挥手报文发送成功！" << endl;
	//=======================等待2MSL防止ACK丢失=======================
	int waittime = clock();
	cout << "[系统提示] 客户端2MSL等待..." << endl;
	MsgFormat tmp;
	while (clock() - waittime < 2 * MAX_WAIT_TIME) {
		int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[系统提示] 断连发生错误..." << endl;
			return false;
		}
		else if (recvByte > 0) {
			sendto(clientSocket, (char*)&wave3, sizeof(wave3), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[系统提示] 重传ACK" << endl;
		}
	}
	cout << "[系统提示] 断连成功！" << endl;
	return true;
}
int main() {
	cout << "===============================================" << endl;
	cout << "==================叡神客户端===================" << endl;
	cout << "===============================================" << endl;
	cout << "叡神客户端[版本 23.11.28.0]      开发者-Yuzhao-" << endl;
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
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  // 本次实验采用Internet协议版本4（IPv4）地址族、数据报套接字
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // 非阻塞
	if (clientSocket == INVALID_SOCKET) {
		cout << "[错误] 创建Socket失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // 释放 Socket DLL 资源
		system("pause");
		return 1;
	}
	cout << "[系统提示] 创建Socket成功！" << endl;
	//=========================服务器/路由器地址=======================
	SOCKADDR_IN serverAddr;  // 服务端地址信息的数据结构IPv4
	serverAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // 地址
	serverAddr.sin_port = htons(RouterPORT);  // 端口号
	//============================客户端地址===========================
	SOCKADDR_IN clientAddr;  // 客户端地址信息的数据结构IPv4
	clientAddr.sin_family = AF_INET;  // 协议族，IPv4地址族
	inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr.s_addr);  // 地址
	clientAddr.sin_port = htons(ClientPORT);  // 端口号
	//=============================绑定 bind===========================
	err = bind(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
	if (err == SOCKET_ERROR) {
		cout << "[错误] bind 绑定失败，错误代码: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		closesocket(clientSocket);
		WSACleanup();
		system("pause");
		return 1;
	}
	cout << "[系统提示] bind 绑定端口 " << ClientPORT << " 成功！" << endl;
	//===========================连接 connect==========================
	bool res = myconnect(clientSocket, serverAddr);
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
	cout << "    $ 组合标志位为各项加和，最终输出的为十进制数" << endl;
	cout << "-----------------------------------------------" << endl;
	// 获取时间
	time_t t = time(NULL);
	char time_str[64];
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "        时间：" << time_str;
	cout << "===============================================" << endl;
	while (res) {
		char c;
		if (run_flag) {
			run_flag = 0;
			cout << "[系统提示] 请输入选择您要使用的功能：";
		}
		else {
			cout << "[系统提示] 请输入 q/Q 关闭连接：";
		}
		cin >> c;
		if (c == 'r' || c == 'R')
		{
			string filepath;
			cout << "[系统提示] 请输入文件绝对路径：" << endl;
			cin >> filepath;
			//=============================文件传输============================
			GBN_SendFunc(filepath, clientSocket, serverAddr);
		}
		else if (c == 'q' || c == 'Q')
		{
			res = false;  // 退出循环
		}
	}
	//========================关闭连接 disconnect======================
	cout << "[系统提示] 正在断连..." << endl;
	mydisconnect(clientSocket, serverAddr);
	closesocket(clientSocket); //关闭socket
	WSACleanup();
	cout << "===============================================" << endl;
	system("pause");
	return 0;
}