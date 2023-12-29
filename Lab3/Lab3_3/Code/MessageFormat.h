#pragma once
#include<iostream>
using namespace std;

// 宏定义：一些关于文件传输的常量
#define MAX_WAIT_TIME CLOCKS_PER_SEC  // 超时时间上限为1000毫秒
#define MAX_RESEND_TIMES 10  // 超时重传次数上限为10次
#define MAX_FILE_SIZE 100 * 1000000  // 最大文件大小为100兆字节
#define MAX_MSG_SIZE 10000  // 最大文件大小为10000字节

// 定义常量：报文中的标志位
/*
	对于标志位做出特别说明：标志位共16位，现只使用了低四位
	(高位)| Unused | FILEDATA(1 bit) | FILEFLAG(1 bit) | ACK(1 bit) | SYN(1 bit) | FIN(1 bit) |(低位)
	FILEFLAG	表示该包是传输文件相关信息【文件名、文件大小】的数据包
*/
const unsigned short int FIN = 0x1;  // 0000_0000_0000_0001
const unsigned short int SYN = 0x2;  // 0000_0000_0000_0010
const unsigned short int ACK = 0x4;  // 0000_0000_0000_0100
const unsigned short int FILEFLAG = 0x8;  // 0000_0000_0000_1000
const unsigned short int FILEDATA = 0x10;  // 0000_0000_0001_0000

// 默认情况下，编译器通常会使用一些对齐策略进行性能优化，但是我们希望保证结构体内的数据强制按照设计格式排列
#pragma pack(1)  // 禁用对齐优化
struct MsgFormat {
	uint32_t SrcIP, DestIP;	 // 源ip地址（1-4字节）、目的ip地址（5-8字节）——本部分设计是参照UDP报文格式的完整性，但是对于我们本次实验测试而言，由于是本地回环，这两个数据并未使用
	uint16_t SrcPort, DestPort;  // 源端口号（9-10字节）、目的端口号（11-12字节）
	uint32_t seq, ack, size;  // 发送序列号（13-16字节）、确认序列号（17-20字节）、数据大小（21-24字节）
	uint16_t CheckNum, Flag;  // 校验和（25-26字节）、标志位（27-28字节）
	//====== 首部 ===== 数据 ===== 分界线 =====
	BYTE MSGDataBuf[MAX_MSG_SIZE];  // 数据部分：包括文件名称和文件大小
	MsgFormat();  // 报文构造函数
	void setCheckNum();  // 设置校验和——发送端
	bool check();  // 校验函数——接收端
};
#pragma pack()  // 使用对齐优化
MsgFormat::MsgFormat() {
	SrcIP = 0;
	DestIP = 0;
	seq = 0;
	ack = 0;
	size = 0;
	Flag = 0;
	memset(&MSGDataBuf, 0, sizeof(MSGDataBuf));  // 初始化数据部分
}

void MsgFormat::setCheckNum() {
	this->CheckNum = 0;  // 校验和域段清零
	unsigned int sum = 0;  // 求和值
	unsigned short* MSGBuf = (unsigned short*)this;
	// 将缓冲区中的每两个字节相加
	for (int i = 0; i < sizeof(*this); i += 2) {
		sum += *MSGBuf++;
		if (sum & 0xFFFF0000) {  // 存在进位，则回加到求和值中
			sum &= 0xFFFF;  // 保留求和值低16位
			sum++;  // 回加进位
		}
	}
	this->CheckNum = ~(sum & 0xFFFF);  // 保留求和值低16位并取反得到校验和
}

bool MsgFormat::check() {
	unsigned int sum = 0;  // 求和值
	unsigned short* MSGBuf = (unsigned short*)this;
	// 将缓冲区中的每两个字节相加
	for (int i = 0; i < sizeof(*this); i += 2) {
		sum += *MSGBuf++;
		if (sum & 0xFFFF0000) {  // 存在进位，则回加到求和值中
			sum &= 0xFFFF;  // 保留求和值低16位
			sum++;  // 回加进位
		}
	}
	// 由于我们设置的校验和是求和值的反码，因此现在我们实际上得到的 sum 是 校验和+校验和反码
	// 显然如果不出意外的话原码和反码相加得到的是全1的数据，即当我们得到全1的数据表示校验无误
	if ((sum & 0xFFFF) == 0xFFFF) {  // 保留求和值低16位并与全1数据比较
		return true;  // 全1——校验无误
	}
	else {
		return false;  // 否则存在错误
	}
}