#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // �׽��ֽӿ�
#include <Ws2tcpip.h>  // �׽����º�����
#include <time.h>
#include <fstream>
#include "MessageFormat.h"  // ���ݱ���ʽ
#pragma comment (lib, "ws2_32.lib")  // �׽���ʵ��
using namespace std;
#define RouterPORT 30000  // ·�����˿ں�
#define ServerPORT 10000  // �������˿ں�

int relative_seq = 0;  // Ϊ�˱���ʵ�����ǲ���������������кŵķ�ʽ�����ǲ���������Wireshark�����е�������кţ����� 0 ��ʼ
MsgFormat acktmp;
// ��������
bool myconnect(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	//============================��һ������===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // ��һ���������ݱ�
	//============================�ڶ�������===========================
	// SYN = 1, ACK = 1, seq = w, ack = x
	MsgFormat handshake2;  // �ڶ����������ݱ�
	//============================����������===========================
	// ACK = 1, seq = x + 1, ack = w
	MsgFormat handshake3;  // �������������ݱ�
	while (1) {
		//============================��һ������===========================
		int recvByte = recvfrom(serverSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte == -1) {
			cout << "[����] ����ʧ��..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(handshake1.Flag && SYN) || !handshake1.check()) {
				cout << "[����] ���ӷ�������..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			// cout << "����handshake1.seq =" << handshake1.seq << endl;
			cout << "[ϵͳ��ʾ] ��һ�����ֱ��Ľ��ճɹ���" << endl;
			break;
		}
	}
	//============================�ڶ�������===========================
	handshake2.SrcPort = ServerPORT;
	handshake2.DestPort = RouterPORT;
	handshake2.seq = relative_seq;
	relative_seq++;
	handshake2.ack = handshake1.seq;
	handshake2.Flag += SYN;
	handshake2.Flag += ACK;
	handshake2.setCheckNum();
	// cout << "����handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
	int err = sendto(serverSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&clientAddr, addrlen);
	clock_t handshake2start = clock();
	if (err == -1) {
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] �ڶ������ֱ��ķ��ͳɹ���" << endl;
	//============================����������===========================
	while (recvfrom(serverSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&clientAddr, &addrlen) <= 0) {
		//	�ڶ������ֳ�ʱ�����·��Ͳ����¼�ʱ
		if (clock() - handshake2start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[ϵͳ��ʾ] �ڶ������ֳ�ʱ�������ش�..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(serverSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&clientAddr, addrlen);
			handshake2start = clock();
			if (err == -1) {
				cout << "[����] ����ʧ��..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	// cout << "����handshake3.seq =" << handshake3.seq << "handshake3.ack =" << handshake3.ack << endl;
	if ((handshake3.Flag && ACK) && handshake3.check() && handshake3.ack == handshake2.seq) {
		cout << "[ϵͳ��ʾ] ���������ֱ���ȷ�ϳɹ���" << endl;
		cout << "[ϵͳ��ʾ] ���ӳɹ���" << endl;
		return true;
	}
	else {
		cout << "[����] ���ӷ�������..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
}
// ���Ľ��ո�������
bool msgRecv(MsgFormat& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (recvMsg.check() && ((recvMsg.seq == relative_seq + 1))) {
				MsgFormat ackMsg;
				ackMsg.SrcPort = ServerPORT;
				ackMsg.DestPort = RouterPORT;
				ackMsg.Flag += ACK;
				ackMsg.seq = relative_seq;
				relative_seq++;
				ackMsg.ack = recvMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq=" << ackMsg.seq << " ack = " << ackMsg.ack << " Flag=" << ackMsg.Flag << endl;
				acktmp = ackMsg;
				return true;
			}
			// ��� seq != relative_seq + 1����ʾ���ظ��ı��ģ�������ʽ�������ñ��Ĳ��ش�ACK
			// ����ԭ�򣺿ͻ���δ���յ���������ACK����ʱ�ش�
			else if (recvMsg.check() && ((recvMsg.seq != relative_seq + 1))) {
				MsgFormat ackMsg;
				ackMsg.SrcPort = ServerPORT;
				ackMsg.DestPort = RouterPORT;
				ackMsg.Flag += ACK;
				ackMsg.seq = relative_seq;
				relative_seq++;
				ackMsg.ack = recvMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] [**�ظ����Ķ�**]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq = " << ackMsg.seq << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
			}
			else {  // У��ʹ����ش���һ��ack
				MsgFormat ackMsg;
				ackMsg = acktmp;
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] [**У��ʹ���**]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
				cout << "[������־] [**У��ʹ��󡪡��ش���һ��ACK����**]" << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq = " << ackMsg.seq << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
			}
		}
		else return false;
	}
}
// ���Ľ��պ���
void RecvFunc(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	//===========================�ļ����ʹ�С==========================
	MsgFormat rMsg;
	unsigned int filesize;
	char filename[50] = { 0 };
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&rMsg, sizeof(rMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (rMsg.check() && ((rMsg.seq == relative_seq + 1))) {
				filesize = rMsg.size;  // ��ȡ�ļ���С
				for (int i = 0; rMsg.MSGDataBuf[i]; i++)  // ��ȡ�ļ���
					filename[i] = rMsg.MSGDataBuf[i];
				cout << "-----------------------------------------------" << endl;
				cout << "[������־] �����ļ���" << filename << "���ļ���С��" << filesize << "B" << endl;
				cout << "-----------------------------------------------" << endl;
				MsgFormat ackMsg;
				ackMsg.SrcPort = ServerPORT;
				ackMsg.DestPort = RouterPORT;
				ackMsg.Flag += ACK;
				ackMsg.seq = relative_seq;
				relative_seq++;
				ackMsg.ack = rMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] " << rMsg.SrcPort << " -> " << rMsg.DestPort << " seq = " << rMsg.seq << " Flag = " << rMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq = " << ackMsg.seq << " Flag = " << ackMsg.Flag << endl;
				acktmp = ackMsg;
				break;
			}
			// ��� seq != relative_seq + 1����ʾ���ظ��ı��ģ�������ʽ�������ñ��Ĳ��ش�ACK
			// ����ԭ�򣺿ͻ���δ���յ���������ACK����ʱ�ش�
			else if (rMsg.check() && ((rMsg.seq != relative_seq + 1))) {
				MsgFormat ackMsg;
				ackMsg.SrcPort = ServerPORT;
				ackMsg.DestPort = RouterPORT;
				ackMsg.Flag += ACK;
				ackMsg.seq = relative_seq;
				relative_seq++;
				ackMsg.ack = rMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] [**�ظ����Ķ�**]" << rMsg.SrcPort << " -> " << rMsg.DestPort << " seq = " << rMsg.seq << " Flag = " << rMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq = " << ackMsg.seq << " Flag = " << ackMsg.Flag << endl;
			}
			else {  // У��ʹ����ش���һ��ack
				MsgFormat ackMsg;
				ackMsg = acktmp;
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] [**У��ʹ���**]" << rMsg.SrcPort << " -> " << rMsg.DestPort << " seq = " << rMsg.seq << " Flag = " << rMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " seq = " << ackMsg.seq << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
			}
		}
	}
	//===========================�ļ����ݲ���==========================
	int batchNum = filesize / MAX_MSG_SIZE;  // ���ر�����
	int leftSize = filesize % MAX_MSG_SIZE;  // ʣ�౨�Ĵ�С
	BYTE* fileBuffer = new BYTE[filesize];
	cout << "[������־] ��ʼ�������ݣ��� " << batchNum << " �����ر��ĶΣ������ر��Ĵ�С��" << leftSize << endl;
	for (int i = 0; i < batchNum; i++) {  // i + 1 - �ڼ������ر���
		MsgFormat dMsg;
		if (msgRecv(dMsg, serverSocket, clientAddr))
			cout << "[������־] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
		else {
			cout << "[������־] ���ݽ���ʧ��..." << endl;
			cout << "===============================================" << endl;
			return;
		}
		for (int j = 0; j < MAX_MSG_SIZE; j++) {  // ��ȡ���ݲ���
			fileBuffer[i * MAX_MSG_SIZE + j] = dMsg.MSGDataBuf[j];
		}
	}
	if (leftSize > 0) {
		MsgFormat dMsg;
		if (msgRecv(dMsg, serverSocket, clientAddr))
			cout << "[������־] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
		else {
			cout << "[������־] ���ݽ���ʧ��..." << endl;
			cout << "===============================================" << endl;
			return;
		}
		for (int j = 0; j < leftSize; j++) {
			fileBuffer[batchNum * MAX_MSG_SIZE + j] = dMsg.MSGDataBuf[j];
		}
	}
	cout << "[������־] ���ݽ��ճɹ�������д�뱾��..." << endl;
	ofstream f(filename, ofstream::binary);  // �Զ����Ʒ�ʽ��ȡ�ļ�
	if (f.is_open()) {
		f.write(reinterpret_cast<const char*>(fileBuffer), filesize);
		f.close();
		cout << "[������־] �ļ�д��ɹ���" << endl;
		cout << "===============================================" << endl;
	}
}
// ��������
bool mydisconnect(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int addrlen = sizeof(clientAddr);
	//============================��һ�λ���===========================
	// FIN = 1, seq = y
	MsgFormat wave1;  // ��һ�λ������ݱ�
	//============================�ڶ��λ���===========================
	// FIN = 1, ACK = 1, seq = z, ack = y
	MsgFormat wave2;  // �ڶ��λ������ݱ�
	//============================�����λ���===========================
	// ACK = 1, seq = y + 1, ack = z
	MsgFormat wave3;  // �����λ������ݱ�
	while (1) {
		//============================��һ�λ���===========================
		int recvByte = recvfrom(serverSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte == -1) {
			cout << "[����] ����ʧ��..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(wave1.Flag && FIN) || !wave1.check()) {
				cout << "[����] ������������..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			// cout << "����wave1.seq=" << wave1.seq << endl;
			cout << "[ϵͳ��ʾ] ��һ�λ��ֱ��Ľ��ճɹ���" << endl;
			break;
		}
	}
	//============================�ڶ��λ���===========================
	wave2.SrcPort = ServerPORT;
	wave2.DestPort = RouterPORT;
	wave2.seq = relative_seq;
	relative_seq++;
	wave2.ack = wave1.seq;
	wave2.Flag += FIN;
	wave2.Flag += ACK;
	wave2.setCheckNum();
	// cout << "����wave2.ack=" << wave2.ack << " wave2.seq=" << wave2.seq << endl;
	int err = sendto(serverSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&clientAddr, addrlen);
	clock_t wave2start = clock();
	if (err == -1) {
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] �ڶ��λ��ֱ��ķ��ͳɹ���" << endl;
	//============================�����λ���===========================
	while (recvfrom(serverSocket, (char*)&wave3, sizeof(wave3), 0, (sockaddr*)&clientAddr, &addrlen) <= 0) {
		//	�ڶ��λ��ֳ�ʱ�����·��Ͳ����¼�ʱ
		if (clock() - wave2start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[ϵͳ��ʾ] �ڶ��λ��ֳ�ʱ�������ش�..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(serverSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&clientAddr, addrlen);
			wave2start = clock();
			if (err == -1) {
				cout << "[����] ����ʧ��..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	// cout << "����wave3.ack=" << wave3.ack << " wave3.seq=" << wave3.seq << endl;
	if ((wave3.Flag && ACK) && wave3.check() && (wave3.ack == wave2.seq)) {
		cout << "[ϵͳ��ʾ] �����λ��ֱ���ȷ�ϳɹ���" << endl;
		cout << "[ϵͳ��ʾ] �����ɹ���" << endl;
		return true;
	}
	else {
		cout << "[����] ������������..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
}
int main() {
	cout << "===============================================" << endl;
	cout << "==================���������===================" << endl;
	cout << "===============================================" << endl;
	cout << "����ͻ���[�汾 23.11.15.0]      ������-Yuzhao-" << endl;
	cout << "===============================================" << endl;
	cout << "==================������׼��===================" << endl;
	//========================��ʼ��Socket DLL=========================
	WORD wVersionRequested = MAKEWORD(2, 2);  // �汾����MAKEWORD(�ΰ汾��, ���汾��)
	WSAData wsadata;  // �׽���ʵ�ֵ���ϸ��Ϣ
	int err = WSAStartup(wVersionRequested, &wsadata);  // err��WSAStartup�����ķ���ֵ�����������
	if (err != NO_ERROR) {
		cout << "[����] ��ʼ��Socket DLLʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] ��ʼ��Socket DLL�ɹ���" << endl;
	//============================����Socket===========================
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);  // ����ʵ�����InternetЭ��汾4��IPv4����ַ�塢���ݱ��׽���
	if (serverSocket == INVALID_SOCKET) {
		cout << "[����] ����Socketʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // �ͷ� Socket DLL ��Դ
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] ����Socket�ɹ���" << endl;
	//============================��������ַ===========================
	SOCKADDR_IN serverAddr;  // ��������ַ��Ϣ�����ݽṹIPv4
	serverAddr.sin_family = AF_INET;  // Э���壬IPv4��ַ��
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // ��ַ
	serverAddr.sin_port = htons(ServerPORT);  // �˿ں�
	//=========================�ͻ���/·������ַ=======================
	SOCKADDR_IN clientAddr;  // �ͻ��˵�ַ��Ϣ�����ݽṹIPv4
	clientAddr.sin_family = AF_INET;  // Э���壬IPv4��ַ��
	inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr.s_addr);  // ��ַ
	clientAddr.sin_port = htons(RouterPORT);  // �˿ں�
	//=============================�� bind===========================
	err = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (err == SOCKET_ERROR) {
		cout << "[����] bind ��ʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		closesocket(serverSocket);
		WSACleanup();
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] bind �󶨶˿� " << ServerPORT << " �ɹ���" << endl;
	cout << "[ϵͳ��ʾ] �ȴ��ͻ�������..." << endl;
	//===========================���� connect==========================
	bool res = myconnect(serverSocket, clientAddr);
	if (!res) {  // ����ʧ��
		system("pause");
		return 1;
	}
	//=============================ϵͳ���============================
	cout << "===============================================" << endl;
	cout << "               ��ӭʹ�Å�����                " << endl;
	cout << "***********************************************" << endl;
	cout << "  * ʹ��˵����" << endl;
	cout << "    $ ���� q/Q �ر�����" << endl;
	cout << "    $ ���� r/R �����ļ�" << endl;
	cout << "  * ��־λ˵����" << endl;
	cout << "    $ FIN = 0000_0000_0000_0001" << endl;
	cout << "    $ SYN = 0000_0000_0000_0010" << endl;
	cout << "    $ ACK = 0000_0000_0000_0100" << endl;
	cout << "    $ FILEFLAG = 0000_0000_0000_1000" << endl;
	cout << "    $ ��ϱ�־λΪ����Ӻͣ����������Ϊʮ������" << endl;
	cout << "-----------------------------------------------" << endl;
	// ��ȡʱ��
	time_t t = time(NULL);
	char time_str[64];
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "        ʱ�䣺" << time_str;
	cout << "===============================================" << endl;
	//=============================�ļ�����============================
	int addrlen = sizeof(clientAddr);
	MsgFormat qMsg;
	while (res) {
		recvfrom(serverSocket, (char*)&qMsg, sizeof(qMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (!(qMsg.Flag && QUIT))
			RecvFunc(serverSocket, clientAddr);
		//========================�ر����� disconnect======================
		else {
			cout << "[ϵͳ��ʾ] ���ڶ���..." << endl;
			mydisconnect(serverSocket, clientAddr);
			closesocket(serverSocket); //�ر�socket
			WSACleanup();
			res = false;
		}
	}
	system("pause");
	return 0;
}