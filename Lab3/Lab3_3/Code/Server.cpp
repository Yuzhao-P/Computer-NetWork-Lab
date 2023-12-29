#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // �׽��ֽӿ�
#include <Ws2tcpip.h>  // �׽����º�����
#include <time.h>
#include <fstream>
#include <unordered_map>
#include "MessageFormat.h"  // ���ݱ���ʽ
#pragma comment (lib, "ws2_32.lib")  // �׽���ʵ��
using namespace std;
#define RouterPORT 30000  // ·�����˿ں�
#define ServerPORT 10000  // �������˿ں�

int connect_seq = 0;  // ��ǰһ��ʵ�鲻ͬ����һ���������ֻ�����ǵ��������
MsgFormat acktmp;  // �ñ����������ݴ��������У��ʹ���ʱ�ش�

// ���ڻ������ڵı���
const int N = 5;  // �������ڴ�С
int base = 0;  // �����
int nextseqnum = 0;  // ��һ�����
bool* acked;  // ȷ�ϱ��

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
		if (recvByte == 0) {
			cout << "[����] ����ʧ��..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(handshake1.Flag && SYN) || !handshake1.check() || !(handshake1.seq == connect_seq)) {
				cout << "[����] ���ӷ�������..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			// cout << "����handshake1.seq =" << handshake1.seq << "handshake1.ack =" << handshake1.ack << endl;
			cout << "[ϵͳ��ʾ] ��һ�����ֱ��Ľ��ճɹ���" << endl;
			break;
		}
	}
	//============================�ڶ�������===========================
	handshake2.SrcPort = ServerPORT;
	handshake2.DestPort = RouterPORT;
	handshake2.seq = connect_seq;
	connect_seq++;
	handshake2.ack = handshake1.seq;
	handshake2.Flag += SYN;
	handshake2.Flag += ACK;
	handshake2.setCheckNum();
	int err = sendto(serverSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&clientAddr, addrlen);
	// cout << "����handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
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
	if ((handshake3.Flag && ACK) && handshake3.check() && (handshake3.ack == handshake2.seq) && (handshake3.seq == connect_seq)) {
		// cout << "����handshake3.seq =" << handshake3.seq << "handshake3.ack =" << handshake3.ack << endl;
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
// bool msgRecv(MsgFormat& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr, int& expectedseqnum) {
bool msgRecv(MsgFormat& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr, unordered_map<int, MsgFormat>& receivedPackets) {
	int addrlen = sizeof(clientAddr);
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (recvMsg.check()) {
				if (recvMsg.seq >= base && recvMsg.seq <= base + N - 1) {  // �ڴ�����
					if (recvMsg.seq == base) {  // ������߽�
						//�յ�˳������ݰ�
						receivedPackets[recvMsg.seq] = recvMsg;
						MsgFormat ackMsg;
						ackMsg.SrcPort = ServerPORT;
						ackMsg.DestPort = RouterPORT;
						ackMsg.Flag += ACK;
						ackMsg.ack = recvMsg.seq;
						ackMsg.setCheckNum();
						sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
						cout << "[������־] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
						cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
						// expectedseqnum++;
						acked[base] = true;
						while (acked[base]) {
							base++;
						}
						return true;
					}
					else {  // ������������
						if (!acked[recvMsg.seq]) {
							acked[recvMsg.seq] = true;
							//�յ�˳������ݰ�
							receivedPackets[recvMsg.seq] = recvMsg;
							MsgFormat ackMsg;
							ackMsg.SrcPort = ServerPORT;
							ackMsg.DestPort = RouterPORT;
							ackMsg.Flag += ACK;
							ackMsg.ack = recvMsg.seq;
							ackMsg.setCheckNum();
							sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
							cout << "[������־] [ʧ�򻺴�]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
							cout << "[������־]  " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
							return true;
						}
					}
				}
				else if (recvMsg.seq >= base - N && recvMsg.seq <= base - 1) {  // ǰһ������
					MsgFormat ackMsg;
					ackMsg.SrcPort = ServerPORT;
					ackMsg.DestPort = RouterPORT;
					ackMsg.Flag += ACK;
					ackMsg.ack = recvMsg.seq;
					ackMsg.setCheckNum();
					sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
					cout << "[������־] [�ѻ���] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " size = " << recvMsg.size << "B seq = " << recvMsg.seq << " Flag = " << recvMsg.Flag << endl;
					cout << "[������־] [�ظ�ACK]" << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
				}
			}
		}
		else if (recvByte == 0) {
			return false;
		}
	}
}
// ���Ľ��պ���
void RecvFunc(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	// int expectedseqnum = 0;  // �ڴ����յ����к�
	int addrlen = sizeof(clientAddr);
	// int expected_seq = 0;
	// ���ն˻�����
	unordered_map<int, MsgFormat> receivedPackets;
	//===========================�ļ����ʹ�С==========================
	MsgFormat rMsg;
	unsigned int filesize;
	char filename[50] = { 0 };
	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&rMsg, sizeof(rMsg), 0, (sockaddr*)&clientAddr, &addrlen);
		if (recvByte > 0) {
			if (rMsg.check() && ((rMsg.seq == base))) {
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
				ackMsg.ack = rMsg.seq;
				ackMsg.setCheckNum();
				sendto(serverSocket, (char*)&ackMsg, sizeof(ackMsg), 0, (sockaddr*)&clientAddr, addrlen);
				cout << "[������־] " << rMsg.SrcPort << " -> " << rMsg.DestPort << " size = " << rMsg.size << "B seq = " << rMsg.seq << " Flag = " << rMsg.Flag << endl;
				cout << "[������־] " << ackMsg.SrcPort << " -> " << ackMsg.DestPort << " ack = " << ackMsg.ack << " Flag = " << ackMsg.Flag << endl;
				// expectedseqnum++;
				base++;
				break;
			}
		}
	}
	//===========================�ļ����ݲ���==========================
	int batchNum = filesize / MAX_MSG_SIZE;  // ���ر�����
	int leftSize = filesize % MAX_MSG_SIZE;  // ʣ�౨�Ĵ�С
	BYTE* fileBuffer = new BYTE[filesize];
	cout << "[������־] ��ʼ�������ݣ��� " << batchNum << " �����ر��ĶΣ������ر��Ĵ�С��" << leftSize << endl;
	int pkt_amount = leftSize > 0 ? batchNum + 2 : batchNum + 1;
	acked = new bool[pkt_amount];
	memset(acked, false, pkt_amount);
	int i = 0;
	while (i < pkt_amount - 1) {
		MsgFormat dMsg;
		if (msgRecv(dMsg, serverSocket, clientAddr, receivedPackets))
			i++;
			// cout << "[������־] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
		else {
			cout << "[������־] ���ݽ���ʧ��..." << endl;
			cout << "===============================================" << endl;
			return;
		}
		if (dMsg.seq == batchNum + 1 && leftSize > 0) {
			for (int j = 0; j < leftSize; j++) {
				fileBuffer[batchNum * MAX_MSG_SIZE + j] = receivedPackets[dMsg.seq].MSGDataBuf[j];
			}
		}
		else {
			for (int j = 0; j < MAX_MSG_SIZE; j++) {  // ��ȡ���ݲ���
				fileBuffer[(dMsg.seq - 1) * MAX_MSG_SIZE + j] = receivedPackets[dMsg.seq].MSGDataBuf[j];
			}
		}
			
	//	while (pkt_num > 0) {
	//		for (int j = 0; j < MAX_MSG_SIZE; j++) {  // ��ȡ���ݲ���
	//			fileBuffer[i * MAX_MSG_SIZE + j] = receivedPackets[base - pkt_num].MSGDataBuf[j];
	//		}
	//		pkt_num--;
	//		i++;
	//	}
	//}
	//if (leftSize > 0) {
	//	MsgFormat dMsg;
	//	if (msgRecv(dMsg, serverSocket, clientAddr, receivedPackets));
	//		// cout << "[������־] " << dMsg.SrcPort << " -> " << dMsg.DestPort << " size = " << dMsg.size << "B seq = " << dMsg.seq << " Flag = " << dMsg.Flag << endl;
	//	else {
	//		cout << "[������־] ���ݽ���ʧ��..." << endl;
	//		cout << "===============================================" << endl;
	//		return;
	//	}

	//	for (int j = 0; j < leftSize; j++) {
	//		fileBuffer[batchNum * MAX_MSG_SIZE + j] = dMsg.MSGDataBuf[j];
	//	}
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
		if (recvByte == 0) {
			cout << "[����] ����ʧ��..." << endl;
			cout << "===============================================" << endl;
			return false;
		}
		else if (recvByte > 0) {
			if (!(wave1.Flag && FIN) || !wave1.check() || !(wave1.seq == connect_seq + 1)) {
				cout << "[����] ������������..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
			cout << "[ϵͳ��ʾ] ��һ�λ��ֱ��Ľ��ճɹ���" << endl;
			break;
		}
	}
	//============================�ڶ��λ���===========================
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
	cout << "����ͻ���[�汾 23.12.25.9]      ������-Yuzhao-" << endl;
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
	u_long mode = 1;
	ioctlsocket(serverSocket, FIONBIO, &mode);  // ������
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
	cout << "    $ FILEDATA = 0000_0000_0001_0000" << endl;
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
	RecvFunc(serverSocket, clientAddr);
	//========================�ر����� disconnect======================
	cout << "[ϵͳ��ʾ] �ļ�д����ϵȴ��û��Ͽ�����..." << endl;
	mydisconnect(serverSocket, clientAddr);
	closesocket(serverSocket); //�ر�socket
	WSACleanup();
	system("pause");
	return 0;
}