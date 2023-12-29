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
#define ClientPORT 20000  // �ͻ��˶˿ں�

int relative_seq = 0;  // Ϊ�˱���ʵ�����ǲ���������������кŵķ�ʽ�����ǲ���������Wireshark�����е�������кţ����� 0 ��ʼ
MsgFormat sendtmp;
// ��������
bool myconnect(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int addrlen = sizeof(serverAddr);
	//============================��һ������===========================
	// SYN = 1, seq = x
	MsgFormat handshake1;  // ��һ���������ݱ�
	handshake1.SrcPort = ClientPORT;
	handshake1.DestPort = RouterPORT;
	handshake1.seq = relative_seq;
	relative_seq++;
	handshake1.Flag += SYN;
	handshake1.setCheckNum();
	// cout << "����handshake1.seq =" << handshake1.seq << endl;
	int err = sendto(clientSocket, (char*)&handshake1, sizeof(handshake1), 0, (sockaddr*)&serverAddr, addrlen);
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
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);  // ������
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
	// cout << "����handshake2.seq =" << handshake2.seq << "handshake2.ack =" << handshake2.ack << endl;
	if ((handshake2.Flag && SYN) && (handshake2.Flag && ACK) && handshake2.check() && (handshake2.ack == handshake1.seq)) {
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
	handshake3.seq = relative_seq;
	relative_seq++;
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
	cout << "[ϵͳ��ʾ] ���ӳɹ���" << endl;
	return true;
}
int num = 0;
// ���Ĵ��丨������
bool msgSend(MsgFormat& sendMsg, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	if (sendMsg.seq == 3) {  // ����Ϊ�����Ҫ
		num = sendMsg.CheckNum;
		sendMsg.CheckNum = 0;
	}
	int addrlen = sizeof(serverAddr);
	sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
	cout << "[������־] " << sendMsg.SrcPort << " -> " << sendMsg.DestPort << " size = " << sendMsg.size << "B seq = " << sendMsg.seq << " Flag=" << sendMsg.Flag << endl;
	int resendTimes = 0;  // �ش�����
	MsgFormat recvMsg;
	int msgStart = clock();  // ��¼����ʱ��
	while (1) {
		u_long mode = 1;
		ioctlsocket(clientSocket, FIONBIO, &mode);  // ������
		while (recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&serverAddr, &addrlen) <= 0) {
			// ͣ�Ȼ��ƣ�����δ�յ����ݰ�������ִ���������ȴ�
			if (clock() - msgStart > MAX_WAIT_TIME) {  // ��ʱ��δ�յ����ݰ���ACK�������·������ݰ������¼�ʱ
				cout << "[������־] seq = " << sendMsg.seq << "�ı��Ķ� ��" << ++resendTimes << "�γ�ʱ�������ش�..." << endl;
				sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
				msgStart = clock();
			}
			if (resendTimes == MAX_RESEND_TIMES) {
				cout << "[������־] ��ʱ�ش���������ش�������" << MAX_RESEND_TIMES << "������ʧ��..." << endl;
				return false;
			}
		}
		if ((recvMsg.Flag && ACK) && (recvMsg.ack == sendtmp.seq)) {
			cout << "[������־] [У��ʹ��󡪡����յ���һ��ACK����]" << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
			cout << "[������־] seq = " << sendMsg.seq << "�ı��ĶΣ���У��ʹ��������ش�..." << endl;
			sendMsg.CheckNum = num;
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, addrlen);
			msgStart = clock();
		}
		if ((recvMsg.Flag && ACK) && (recvMsg.ack == sendMsg.seq)) {  // ����������޶�ʧ
			cout << "[������־] " << recvMsg.SrcPort << " -> " << recvMsg.DestPort << " seq = " << recvMsg.seq << " ack = " << recvMsg.ack << " Flag = " << recvMsg.Flag << endl;
			sendtmp = sendMsg;
			return true;
		}
	}
	u_long mode = 0;
	ioctlsocket(clientSocket, FIONBIO, &mode);//�Ļ�����ģʽ
	return false;
}
// ���Ĵ��亯��
void SendFunc(string path, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int startTime = clock();
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
	//===========================�ļ����ʹ�С==========================
	MsgFormat rMsg;
	rMsg.SrcPort = ClientPORT;
	rMsg.DestPort = RouterPORT;
	rMsg.size = fileSize;
	rMsg.Flag += FILEFLAG;
	rMsg.seq = relative_seq;
	relative_seq++;
	for (int i = 0; i < filename.size(); i++)  // ��䱨�����ݶ�
		rMsg.MSGDataBuf[i] = filename[i];
	rMsg.MSGDataBuf[filename.size()] = '\0';//�ַ�����β��\0
	rMsg.setCheckNum();
	if (!msgSend(rMsg, clientSocket, serverAddr)) {
		cout << "[������־] ����ʧ��..." << endl;
		return;
	}
	cout << "[������־] �ɹ������ļ������Ϣ�����ļ�����" << filename << " �ļ���С��" << fileSize << "B" << endl;
	//===========================�ļ����ݲ���==========================
	int batchNum = fileSize / MAX_MSG_SIZE;  // ���ر�����
	int leftSize = fileSize % MAX_MSG_SIZE;  // ʣ�౨�Ĵ�С
	for (int i = 0; i < batchNum; i++) {  // i + 1 - �ڼ������ر���
		MsgFormat dMsg;
		dMsg.SrcPort = ClientPORT;
		dMsg.DestPort = RouterPORT;
		dMsg.size = MAX_MSG_SIZE;
		dMsg.seq = relative_seq;
		relative_seq++;
		for (int j = 0; j < MAX_MSG_SIZE; j++)
			dMsg.MSGDataBuf[j] = fileBuffer[i * MAX_MSG_SIZE + j];  // �� i + 1 �����ر���װ��
		dMsg.setCheckNum();
		if (!msgSend(dMsg, clientSocket, serverAddr)) {
			cout << "[������־] ����ʧ��..." << endl;
			return;
		}
		cout << "[������־] �ɹ������ " << i << " �����װ�ر��Ķ�" << endl;
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
			cout << "[������־] ����ʧ��..." << endl;
			return;
		}
		cout << "[������־] �ɹ�����ʣ�ಿ�ֵı��Ķ�" << endl;
	}
	//���㴫��ʱ���������
	int endTime = clock();
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
	wave1.seq = relative_seq;
	relative_seq++;
	wave1.setCheckNum();
	// cout << "����wave1.seq=" << wave1.seq << endl;
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
	// cout << "����wave2.ack=" << wave2.ack << " wave2.seq=" << wave2.seq << endl;
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
	wave3.seq = relative_seq;
	relative_seq++;
	wave3.ack = wave2.seq;
	wave3.Flag += ACK;
	wave3.setCheckNum();
	clock_t wave3start = clock();
	// cout << "����wave3.ack=" << wave3.ack << " wave3.seq=" << wave3.seq << endl;
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
			cout << "[ϵͳ��ʾ] �������ʹ���..." << endl;
			return false;
		}
		else if (recvByte > 0)
		{
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
	cout << "����ͻ���[�汾 23.11.15.0]      ������-Yuzhao-" << endl;
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
		cout << "[ϵͳ��ʾ] ������ѡ����Ҫʹ�õĹ��ܣ�";
		cin >> c;
		if (c == 'r' || c == 'R')
		{
			string filepath;
			cout << "[ϵͳ��ʾ] �������ļ�����·����" << endl;
			cin >> filepath;
			MsgFormat qMsg;
			qMsg.SrcPort = ClientPORT;
			qMsg.DestPort = RouterPORT;
			qMsg.setCheckNum();
			sendto(clientSocket, (char*)&qMsg, sizeof(qMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
			//=============================�ļ�����============================
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