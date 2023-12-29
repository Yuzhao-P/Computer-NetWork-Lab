#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // 套接字接口
#include <Ws2tcpip.h>  // 套接字新函数库
#include <time.h>
#include <fstream>
#include "MessageFormat.h"  // 数据报格式
#pragma comment (lib, "ws2_32.lib")  // 套接字实现
using namespace std;
#define RouterPORT 30000  // 路由器端口号
#define ClientPORT 20000  // 客户端端口号

int relative_seq = 0;  // 为了便于实现我们不采用随机分配序列号的方式，而是采用类似于Wireshark捕获中的相对序列号，即从 0 开始
MsgFormat sendtmp;
// 建连函数
bool myconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================第一次握手===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // 第一次握手数据报
	handshake1.SrcPort = ClientPORT;
	handshake1.DestPort = RouterPORT;
	handshake1.seq = relative_seq;
	relative_seq++;
	handshake1.Flag += SYN;
	handshake1.setCheckNum();
	// cout << "发送handshake1.seq =" << handshake1.seq << endl;
	int err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
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
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // 非阻塞
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
	// cout << "接收handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
	if ((handshake2.Flag && SYN) && (handshake2.Flag && ACK) && handshake2.check() && (handshake2.ack == handshake1.seq)) {
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
	handshake3.seq = relative_seq;
	relative_seq++;
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
	cout << "[系统提示] 连接成功！" << endl;
	return true;
}
int num = 0;
// 报文传输辅助函数
bool msgSend(MsgFormat& sendMsg, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	if (sendMsg.seq == 3) {  // 仅作为检测需要
		num = sendMsg.CheckNum;
		sendMsg.CheckNum = 0;
	}
	int addrlen = sizeof(serverAddr);
	sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
	cout << "[传输日志] " << sendMsg.SrcPort << " -> " << sendMsg.DestPort << " size = " << sendMsg.size << "B seq = " << sendMsg.seq << " Flag=" << sendMsg.Flag << endl;
	int resendTimes = 0;  // 重传次数
	MsgFormat recvMsg;
	int msgStart = clock();  // 记录发送时间
	while (1) {
		u_long mode = 1;
		ioctlsocket(clientSocket, FIONBIO, &mode);  // 非阻塞
		while (recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
			// 停等机制：若尚未收到数据包或检查出现错误，则继续等待
			if (clock() - msgStart > MAX_WAIT_TIME) {  // 超时仍未收到数据包（ACK），重新发送数据包并重新计时
				cout << "[传输日志] seq = " << sendMsg.seq << "的报文段 第" << ++resendTimes << "次超时，正在重传..." << endl;
				sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
				msgStart = clock();
			}
			if (resendTimes == MAX_RESEND_TIMES) {
				cout << "[传输日志] 超时重传超过最大重传次数：" << MAX_RESEND_TIMES << "，传输失败..." << endl;
				return false;
			}
		}
		if ((recvMsg.Flag && ACK) && (recvMsg.ack == sendtmp.seq)) {
			cout << "[传输日志] [校验和错误——接收到上一个ACK报文]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
			cout << "[传输日志] seq = " << sendMsg.seq << "的报文段，因校验和错误正在重传..." << endl;
			sendMsg.CheckNum = num;
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
			msgStart = clock();
		}
		if ((recvMsg.Flag && ACK) && (recvMsg.ack == sendMsg.seq)) {  // 理想情况：无丢失
			cout << "[传输日志] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
			sendtmp = sendMsg;
			return true;
		}
	}
	u_long mode = 0;
	ioctlsocket(clientSocket, FIONBIO, &mode);//改回阻塞模式
	return false;
}
// 报文传输函数
void SendFunc(string path, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int startTime = clock();
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
	//===========================文件名和大小==========================
	MsgFormat rMsg;
	rMsg.SrcPort = ClientPORT;
	rMsg.DestPort = RouterPORT;
	rMsg.size = fileSize;
	rMsg.Flag += FILEFLAG;
	rMsg.seq = relative_seq;
	relative_seq++;
	for (int i = 0; i < filename.size(); i++)  // 填充报文数据段
		rMsg.MSGDataBuf[i] = filename[i];
	rMsg.MSGDataBuf[filename.size()] = '\0';//字符串结尾补\0
	rMsg.setCheckNum();
	if (!msgSend(rMsg, clientSocket, serverAddr)) {
		cout << "[传输日志] 传输失败..." << endl;
		return;
	}
	cout << "[传输日志] 成功传输文件相关信息——文件名：" << filename << " 文件大小：" << fileSize << "B" << endl;
	//===========================文件数据部分==========================
	int batchNum = fileSize / MAX_MSG_SIZE;  // 满载报文数
	int leftSize = fileSize % MAX_MSG_SIZE;  // 剩余报文大小
	for (int i = 0; i < batchNum; i++) {  // i + 1 - 第几个满载报文
		MsgFormat dMsg;
		dMsg.SrcPort = ClientPORT;
		dMsg.DestPort = RouterPORT;
		dMsg.size = MAX_MSG_SIZE;
		dMsg.seq = relative_seq;
		relative_seq++;
		for (int j = 0; j < MAX_MSG_SIZE; j++)
			dMsg.MSGDataBuf[j] = fileBuffer[i * MAX_MSG_SIZE + j];  // 第 i + 1 个满载报文装载
		dMsg.setCheckNum();
		if (!msgSend(dMsg, clientSocket, serverAddr)) {
			cout << "[传输日志] 传输失败..." << endl;
			return;
		}
		cout << "[传输日志] 成功传输第 " << i << " 个最大装载报文段" << endl;
	}
	if (leftSize > 0) {
		MsgFormat dMsg;
		dMsg.SrcPort = ClientPORT;
		dMsg.DestPort = RouterPORT;
		dMsg.size = leftSize;
		dMsg.seq = relative_seq;
		relative_seq++;
		for (int j = 0; j < leftSize; j++) {
			dMsg.MSGDataBuf[j] = fileBuffer[batchNum * MAX_MSG_SIZE + j];
		}
		dMsg.setCheckNum();
		if (!msgSend(dMsg, clientSocket, serverAddr)) {
			cout << "[传输日志] 传输失败..." << endl;
			return;
		}
		cout << "[传输日志] 成功发送剩余部分的报文段" << endl;
	}
	//计算传输时间和吞吐率
	int endTime = clock();
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
	wave1.seq = relative_seq;
	relative_seq++;
	wave1.setCheckNum();
	// cout << "发送wave1.seq=" << wave1.seq << endl;
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
	// cout << "接收wave2.ack=" << wave2.ack << " wave2.seq=" << wave2.seq << endl;
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
	wave3.seq = relative_seq;
	relative_seq++;
	wave3.ack = wave2.seq;
	wave3.Flag += ACK;
	wave3.setCheckNum();
	clock_t wave3start = clock();
	// cout << "发送wave3.ack=" << wave3.ack << " wave3.seq=" << wave3.seq << endl;
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
			cout << "[系统提示] 断连发送错误..." << endl;
			return false;
		}
		else if (recvByte > 0)
		{
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
	cout << "叡神客户端[版本 23.11.15.0]      开发者-Yuzhao-" << endl;
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
		cout << "[系统提示] 请输入选择您要使用的功能：";
		cin >> c;
		if (c == 'r' || c == 'R')
		{
			string filepath;
			cout << "[系统提示] 请输入文件绝对路径：" << endl;
			cin >> filepath;
			MsgFormat qMsg;
			qMsg.SrcPort = ClientPORT;
			qMsg.DestPort = RouterPORT;
			qMsg.setCheckNum();
			sendto(clientSocket, (char*)&qMsg, sizeof(qMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
			//=============================文件传输============================
			SendFunc(filepath, clientSocket, serverAddr);
		}
		else if (c == 'q' || c == 'Q')
		{
			MsgFormat qMsg;
			qMsg.SrcPort = ClientPORT;
			qMsg.DestPort = RouterPORT;
			qMsg.Flag += QUIT;
			qMsg.setCheckNum();
			sendto(clientSocket, (char*)&qMsg, sizeof(qMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
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