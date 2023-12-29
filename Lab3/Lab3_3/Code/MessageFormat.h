#pragma once
#include<iostream>
using namespace std;

// �궨�壺һЩ�����ļ�����ĳ���
#define MAX_WAIT_TIME CLOCKS_PER_SEC  // ��ʱʱ������Ϊ1000����
#define MAX_RESEND_TIMES 10  // ��ʱ�ش���������Ϊ10��
#define MAX_FILE_SIZE 100 * 1000000  // ����ļ���СΪ100���ֽ�
#define MAX_MSG_SIZE 10000  // ����ļ���СΪ10000�ֽ�

// ���峣���������еı�־λ
/*
	���ڱ�־λ�����ر�˵������־λ��16λ����ֻʹ���˵���λ
	(��λ)| Unused | FILEDATA(1 bit) | FILEFLAG(1 bit) | ACK(1 bit) | SYN(1 bit) | FIN(1 bit) |(��λ)
	FILEFLAG	��ʾ�ð��Ǵ����ļ������Ϣ���ļ������ļ���С�������ݰ�
*/
const unsigned short int FIN = 0x1;  // 0000_0000_0000_0001
const unsigned short int SYN = 0x2;  // 0000_0000_0000_0010
const unsigned short int ACK = 0x4;  // 0000_0000_0000_0100
const unsigned short int FILEFLAG = 0x8;  // 0000_0000_0000_1000
const unsigned short int FILEDATA = 0x10;  // 0000_0000_0001_0000

// Ĭ������£�������ͨ����ʹ��һЩ������Խ��������Ż�����������ϣ����֤�ṹ���ڵ�����ǿ�ư�����Ƹ�ʽ����
#pragma pack(1)  // ���ö����Ż�
struct MsgFormat {
	uint32_t SrcIP, DestIP;	 // Դip��ַ��1-4�ֽڣ���Ŀ��ip��ַ��5-8�ֽڣ���������������ǲ���UDP���ĸ�ʽ�������ԣ����Ƕ������Ǳ���ʵ����Զ��ԣ������Ǳ��ػػ������������ݲ�δʹ��
	uint16_t SrcPort, DestPort;  // Դ�˿ںţ�9-10�ֽڣ���Ŀ�Ķ˿ںţ�11-12�ֽڣ�
	uint32_t seq, ack, size;  // �������кţ�13-16�ֽڣ���ȷ�����кţ�17-20�ֽڣ������ݴ�С��21-24�ֽڣ�
	uint16_t CheckNum, Flag;  // У��ͣ�25-26�ֽڣ�����־λ��27-28�ֽڣ�
	//====== �ײ� ===== ���� ===== �ֽ��� =====
	BYTE MSGDataBuf[MAX_MSG_SIZE];  // ���ݲ��֣������ļ����ƺ��ļ���С
	MsgFormat();  // ���Ĺ��캯��
	void setCheckNum();  // ����У��͡������Ͷ�
	bool check();  // У�麯���������ն�
};
#pragma pack()  // ʹ�ö����Ż�
MsgFormat::MsgFormat() {
	SrcIP = 0;
	DestIP = 0;
	seq = 0;
	ack = 0;
	size = 0;
	Flag = 0;
	memset(&MSGDataBuf, 0, sizeof(MSGDataBuf));  // ��ʼ�����ݲ���
}

void MsgFormat::setCheckNum() {
	this->CheckNum = 0;  // У����������
	unsigned int sum = 0;  // ���ֵ
	unsigned short* MSGBuf = (unsigned short*)this;
	// ���������е�ÿ�����ֽ����
	for (int i = 0; i < sizeof(*this); i += 2) {
		sum += *MSGBuf++;
		if (sum & 0xFFFF0000) {  // ���ڽ�λ����ؼӵ����ֵ��
			sum &= 0xFFFF;  // �������ֵ��16λ
			sum++;  // �ؼӽ�λ
		}
	}
	this->CheckNum = ~(sum & 0xFFFF);  // �������ֵ��16λ��ȡ���õ�У���
}

bool MsgFormat::check() {
	unsigned int sum = 0;  // ���ֵ
	unsigned short* MSGBuf = (unsigned short*)this;
	// ���������е�ÿ�����ֽ����
	for (int i = 0; i < sizeof(*this); i += 2) {
		sum += *MSGBuf++;
		if (sum & 0xFFFF0000) {  // ���ڽ�λ����ؼӵ����ֵ��
			sum &= 0xFFFF;  // �������ֵ��16λ
			sum++;  // �ؼӽ�λ
		}
	}
	// �����������õ�У��������ֵ�ķ��룬�����������ʵ���ϵõ��� sum �� У���+У��ͷ���
	// ��Ȼ�����������Ļ�ԭ��ͷ�����ӵõ�����ȫ1�����ݣ��������ǵõ�ȫ1�����ݱ�ʾУ������
	if ((sum & 0xFFFF) == 0xFFFF) {  // �������ֵ��16λ����ȫ1���ݱȽ�
		return true;  // ȫ1����У������
	}
	else {
		return false;  // ������ڴ���
	}
}