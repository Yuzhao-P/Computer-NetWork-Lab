#include <iostream>
#include <cstring>
#include <string>
#include <winsock2.h>  // �׽��ֽӿ�
#include <Ws2tcpip.h>  // �׽����º�����
#include <time.h>
#include <fstream>
#include <mutex>
#include "MessageFormat.h"  // ���ݱ���ʽ
#pragma comment (lib, "ws2_32.lib")  // �׽���ʵ��
using namespace std;
#define RouterPORT 30000  // ·�����˿ں�
#define ClientPORT 20000  // �ͻ��˶˿ں�

int connect_seq = 0;  // ��ǰһ��ʵ�鲻ͬ����һ���������ֻ�����ǵ��������
MsgFormat sendtmp;  // �ñ����������ݴ��������У��ʹ���ʱ�ش�
int run_flag = 1;  // ����Ƿ�Ϊ��һ������
bool over = false;  // �����Ƿ����

mutex outputMutex;  // ȫ�ֻ�����

// ���ڻ������ڵı���
int N = 5;  // �������ڴ�С
int base = 0;  // �����
int nextseqnum = 0;  // ��һ�����
clock_t time_count;  // ��Ǳ��ķ�����ʼʱ��

// ��������
bool myconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================��һ������===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // ��һ���������ݱ�
	handshake1.SrcPort = ClientPORT;
	handshake1.DestPort = RouterPORT;
	handshake1.seq = connect_seq;
	connect_seq++;
	handshake1.Flag += SYN;
	handshake1.setCheckNum();
	int err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
	// cout << "����handshake1.seq =" << handshake1.seq << "handshake1.ack =" << handshake1.ack << endl;
	clock_t handshake1start = clock();
	if (err == -1) {
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] ��һ�����ֱ��ķ��ͳɹ���" << endl;
	//============================�ڶ�������===========================
	// SYN = 1, ACK = 1, seq = w, ack = x
	MsgFormat handshake2;  // �ڶ����������ݱ�
	while (recvfrom(clientSocket, (char*)&handshake2, sizeof(handshake2), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
		//	��һ�����ֳ�ʱ�����·��Ͳ����¼�ʱ
		if (clock() - handshake1start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[ϵͳ��ʾ] ��һ�����ֳ�ʱ�������ش�..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
			handshake1start = clock();
			if (err == -1) {
				cout << "[����] ����ʧ��..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((handshake2.Flag && SYN) && (handshake2.Flag && ACK) && handshake2.check() && (handshake2.ack == handshake1.seq)) {
		// cout << "����handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
		cout << "[ϵͳ��ʾ] �ڶ������ֱ���ȷ�ϳɹ���" << endl;
	}
	else {
		cout << "[����] ���ӷ�������..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	//============================����������===========================
	// ACK = 1, seq = x + 1, ack = w
	MsgFormat handshake3;  // �������������ݱ�
	handshake3.SrcPort = ClientPORT;
	handshake3.DestPort = RouterPORT;
	handshake3.seq = connect_seq;
	connect_seq++;
	handshake3.ack = handshake2.seq;
	handshake3.Flag += ACK;
	handshake3.setCheckNum();
	// cout << "����handshake3.seq =" << handshake3.seq << "handshake3.ack =" << handshake3.ack << endl;
	err = sendto(clientSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&serverAddr, addrlen);
	clock_t handshake3start = clock();
	if (err == -1) {
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] ���������ֱ��ķ��ͳɹ���" << endl;
	// ����һ�α���̽������˵����TCP���Ե�����ACK��ʧûʲô�������ܹ������������ݵģ����Ǿ������Է����ҵĳ��򲢲����ԣ�������Ƿ����Ĵλ��ֵĴ���ʽ�������´��봦���ش�
	//=======================�ȴ�2MSL��ֹACK��ʧ=======================
	int waittime = clock();
	MsgFormat tmp;
	while (clock() - waittime < 2 * MAX_WAIT_TIME) {
		int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[ϵͳ��ʾ] ���ӷ�������..." << endl;
			return false;
		}
		else if (recvByte > 0) {
			sendto(clientSocket, (char*)&handshake3, sizeof(handshake3), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[ϵͳ��ʾ] �ش�ACK" << endl;
		}
	}
	cout << "[ϵͳ��ʾ] ���ӳɹ���" << endl;
	return true;
}
// ���Ĵ��丨���������������߳�
struct parameters {
	SOCKADDR_IN serverAddr;
	SOCKET clientSocket;
	int pkt_amount;  // ����������������кŲ���������
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
		lock_guard<mutex> lock(outputMutex);  // ����
		if (recvByte > 0) {  // �ɹ��յ���Ϣ��������ɶ���յ������ˡ�
			if ((recvMsg.Flag && ACK) && recvMsg.check()) {  // ���У���
				// ע�⣺Ϊ�˷�ֹ��������ƻ������ڡ�ʧ����ACK���ء���������Ҫȷ����ǰ��ack����[base, nextseqnum - 1]�ڵ�
				if (recvMsg.ack < base);
				else
					base = recvMsg.ack + 1;  // �������ڻ���
				if (base != nextseqnum)
					time_count = clock();  // ����ʱ��
				cout << "[������־] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
				if (base + N < pkt_amount) {  // չʾ�������
					cout << "[������־] [����ACK]��ǰ����������������ܴ�С��" << N << "���ѷ��͵�δ�յ�ACK�����ݱ���Ŀ��" << nextseqnum - base << "����������δ�������ݱ���Ŀ��" << N - (nextseqnum - base) << endl;
				}
				else {// չʾ�������
					cout << "[������־] [����ACK]��ǰ����������������ܴ�С��" << N << "���ѷ��͵�δ�յ�ACK�����ݱ���Ŀ��" << nextseqnum - base << "����������δ�������ݱ���Ŀ��" << pkt_amount - base - (nextseqnum - base) << endl;
				}
				// �жϴ������
				if (recvMsg.ack == pkt_amount - 1) {
					cout << "[������־] **********���������**********" << endl;
					over = true;
					return 0;
				}
			}
		}
	}
	return 0;
}
// ���Ĵ��亯��
void GBN_SendFunc(string path, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	clock_t startTime = clock();
	string filename = "";
	for (int i = path.size() - 1; i >= 0; i--) {  // �����ȡ�����ļ���
		if (path[i] == '/' || path[i] == '\\')
			break;
		filename += path[i];
	}
	filename = string(filename.rbegin(), filename.rend());  // �����ȡ�����ļ���
	ifstream f(path.c_str(), ifstream::binary);  // �Զ����Ʒ�ʽ��ȡ�ļ�
	if (!f) {
		cout << "[������־] �޷����ļ�..." << endl;
		return;
	}
	BYTE* fileBuffer = new BYTE[MAX_FILE_SIZE];
	unsigned int fileSize = 0;
	BYTE byte = f.get();
	while (f) {  // ���ļ���ȡ��������
		fileBuffer[fileSize++] = byte;
		byte = f.get();
	}
	f.close();

	int batchNum = fileSize / MAX_MSG_SIZE;  // ���ر�����
	int leftSize = fileSize % MAX_MSG_SIZE;  // ʣ�౨�Ĵ�С

	//=========================����������Ϣ�߳�========================
	int pkt_amount = leftSize > 0 ? batchNum + 2 : batchNum + 1;  // +1�����ڵ�һ���ļ����ּ���С�İ�ռһ��
	parameters param;
	param.serverAddr = serverAddr;
	param.clientSocket = clientSocket;
	param.pkt_amount = pkt_amount;
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ACKRecvThread, &param, 0, 0);
	//===========================���̣߳�����==========================
	MsgFormat sendMsg;
	while (1) {
		lock_guard<mutex> lock(outputMutex);  // ����
		if (nextseqnum < base + N && nextseqnum < pkt_amount) {
			if (nextseqnum == 0) {  // �ļ����ּ���С
				sendMsg.SrcPort = ClientPORT;
				sendMsg.DestPort = RouterPORT;
				sendMsg.size = fileSize;
				sendMsg.Flag += FILEFLAG;
				sendMsg.seq = nextseqnum;
				for (int i = 0; i < filename.size(); i++)  // ��䱨�����ݶ�
				sendMsg.MSGDataBuf[i] = filename[i];
				sendMsg.MSGDataBuf[filename.size()] = '\0';//�ַ�����β��\0
				sendMsg.setCheckNum();
			}
			else if (nextseqnum == batchNum + 1 && leftSize > 0) {  // ʣ������
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
			// ����
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[������־] " << sendMsg.SrcPort << " -> " << sendMsg.DestPort << " size = " << sendMsg.size << "B seq = " << sendMsg.seq << " Flag = " << sendMsg.Flag << endl;
			if (base == nextseqnum) {
				time_count = clock();
			}
			nextseqnum++;
			if (base + N < pkt_amount) {  // չʾ�������
				cout << "[������־] [�������ݱ�]��ǰ����������������ܴ�С��" << N << "���ѷ��͵�δ�յ�ACK�����ݱ���Ŀ��" << nextseqnum - base << "����������δ�������ݱ���Ŀ��" << N - (nextseqnum - base) << endl;
			}
			else {// չʾ�������
				cout << "[������־] [�������ݱ�]��ǰ����������������ܴ�С��" << N << "���ѷ��͵�δ�յ�ACK�����ݱ���Ŀ��" << nextseqnum - base << "����������δ�������ݱ���Ŀ��" << pkt_amount - base - (nextseqnum - base) << endl;
			}
		}
		// ��ʱ�ش�
		if (clock() - time_count > MAX_WAIT_TIME) {  // GBN������ǰ�����ڵ��������ݱ��ش�
			for (int i = 0; i < nextseqnum - base; i++) {
				int send_seq = base + i;
				if (send_seq == 0) {
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.size = fileSize;
					sendMsg.Flag += FILEFLAG;
					sendMsg.seq = send_seq;
					for (int i = 0; i < filename.size(); i++)  // ��䱨�����ݶ�
						sendMsg.MSGDataBuf[i] = filename[i];
					sendMsg.MSGDataBuf[filename.size()] = '\0';//�ַ�����β��\0
					sendMsg.setCheckNum();
					cout << "[������־] �����ļ������Ϣ�����ļ�����" << filename << " �ļ���С��" << fileSize << "B" << endl;
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
				// ����
				sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
				cout << "[������־] seq = " << sendMsg.seq << "�ı��Ķγ�ʱ�������ش�..." << endl;
			}
			time_count = clock();
		}
		if (over == true) break;  // ���յ�����ACK
	}
	CloseHandle(hThread);
	cout << "[������־] �ѷ��Ͳ�ȷ�����б��ģ��ļ�����ɹ���" << endl;
	clock_t endTime = clock();
	cout << "-----------------------------------------------" << endl;
	cout << "[������־] ���崫��ʱ��Ϊ:" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	cout << "[������־] ������:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl;
	cout << "===============================================" << endl;
	return;
}
// ��������
bool mydisconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================��һ�λ���===========================
	// FIN = 1, seq = y
	MsgFormat wave1;  // ��һ�λ������ݱ�
	wave1.SrcPort = ClientPORT;
	wave1.DestPort = RouterPORT;
	wave1.Flag += FIN;
	wave1.seq = connect_seq;
	connect_seq++;
	wave1.setCheckNum();
	int err = sendto(clientSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&serverAddr, addrlen);
	clock_t wave1start = clock();
	if (err == -1) {
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] ��һ�λ��ֱ��ķ��ͳɹ���" << endl;
	//============================�ڶ��λ���===========================
	// FIN = 1, ACK = 1, seq = z, ack = y
	MsgFormat wave2;  // �ڶ��λ������ݱ�
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // ������
	while (recvfrom(clientSocket, (char*)&wave2, sizeof(wave2), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
		//	��һ�λ��ֳ�ʱ�����·��Ͳ����¼�ʱ
		if (clock() - wave1start > MAX_WAIT_TIME) {
			cout << "-----------------------------------------------" << endl;
			cout << "[ϵͳ��ʾ] ��һ�λ��ֳ�ʱ�������ش�..." << endl;
			cout << "-----------------------------------------------" << endl;
			err = sendto(clientSocket, (char*)&wave1, sizeof(wave1), 0, (sockaddr*)&serverAddr, addrlen);
			wave1start = clock();
			if (err == -1) {
				cout << "[����] ����ʧ��..." << endl;
				cout << "===============================================" << endl;
				return false;
			}
		}
	}
	if ((wave2.Flag && FIN) && (wave2.Flag && ACK) && wave2.check() && (wave2.ack == wave1.seq)) {
		cout << "[ϵͳ��ʾ] �ڶ��λ��ֱ���ȷ�ϳɹ���" << endl;
	}
	else {
		cout << "[����] ������������..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	//============================�����λ���===========================
	// ACK = 1, seq = y + 1, ack = z
	MsgFormat wave3;  // �������������ݱ�
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
		cout << "[����] ����ʧ��..." << endl;
		cout << "===============================================" << endl;
		return false;
	}
	cout << "[ϵͳ��ʾ] �����λ��ֱ��ķ��ͳɹ���" << endl;
	//=======================�ȴ�2MSL��ֹACK��ʧ=======================
	int waittime = clock();
	cout << "[ϵͳ��ʾ] �ͻ���2MSL�ȴ�..." << endl;
	MsgFormat tmp;
	while (clock() - waittime < 2 * MAX_WAIT_TIME) {
		int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &addrlen);
		if (recvByte == 0) {
			cout << "[ϵͳ��ʾ] ������������..." << endl;
			return false;
		}
		else if (recvByte > 0) {
			sendto(clientSocket, (char*)&wave3, sizeof(wave3), 0, (sockaddr*)&serverAddr, addrlen);
			cout << "[ϵͳ��ʾ] �ش�ACK" << endl;
		}
	}
	cout << "[ϵͳ��ʾ] �����ɹ���" << endl;
	return true;
}
int main() {
	cout << "===============================================" << endl;
	cout << "==================����ͻ���===================" << endl;
	cout << "===============================================" << endl;
	cout << "����ͻ���[�汾 23.11.28.0]      ������-Yuzhao-" << endl;
	cout << "===============================================" << endl;
	cout << "==================�ͻ���׼��===================" << endl;
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
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  // ����ʵ�����InternetЭ��汾4��IPv4����ַ�塢���ݱ��׽���
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // ������
	if (clientSocket == INVALID_SOCKET) {
		cout << "[����] ����Socketʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // �ͷ� Socket DLL ��Դ
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] ����Socket�ɹ���" << endl;
	//=========================������/·������ַ=======================
	SOCKADDR_IN serverAddr;  // ����˵�ַ��Ϣ�����ݽṹIPv4
	serverAddr.sin_family = AF_INET;  // Э���壬IPv4��ַ��
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // ��ַ
	serverAddr.sin_port = htons(RouterPORT);  // �˿ں�
	//============================�ͻ��˵�ַ===========================
	SOCKADDR_IN clientAddr;  // �ͻ��˵�ַ��Ϣ�����ݽṹIPv4
	clientAddr.sin_family = AF_INET;  // Э���壬IPv4��ַ��
	inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr.s_addr);  // ��ַ
	clientAddr.sin_port = htons(ClientPORT);  // �˿ں�
	//=============================�� bind===========================
	err = bind(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
	if (err == SOCKET_ERROR) {
		cout << "[����] bind ��ʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		closesocket(clientSocket);
		WSACleanup();
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] bind �󶨶˿� " << ClientPORT << " �ɹ���" << endl;
	//===========================���� connect==========================
	bool res = myconnect(clientSocket, serverAddr);
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
	while (res) {
		char c;
		if (run_flag) {
			run_flag = 0;
			cout << "[ϵͳ��ʾ] ������ѡ����Ҫʹ�õĹ��ܣ�";
		}
		else {
			cout << "[ϵͳ��ʾ] ������ q/Q �ر����ӣ�";
		}
		cin >> c;
		if (c == 'r' || c == 'R')
		{
			string filepath;
			cout << "[ϵͳ��ʾ] �������ļ�����·����" << endl;
			cin >> filepath;
			//=============================�ļ�����============================
			GBN_SendFunc(filepath, clientSocket, serverAddr);
		}
		else if (c == 'q' || c == 'Q')
		{
			res = false;  // �˳�ѭ��
		}
	}
	//========================�ر����� disconnect======================
	cout << "[ϵͳ��ʾ] ���ڶ���..." << endl;
	mydisconnect(clientSocket, serverAddr);
	closesocket(clientSocket); //�ر�socket
	WSACleanup();
	cout << "===============================================" << endl;
	system("pause");
	return 0;
}