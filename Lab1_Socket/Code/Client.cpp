#include<iostream>
#include<cstring>
#include <winsock2.h>  // �׽��ֽӿ�
#include <Ws2tcpip.h>  // �׽����º�����
#include <time.h>
#pragma comment (lib, "ws2_32.lib")  // �׽���ʵ��

#define PORT 8000  // �˿�
#define MaxBufSize 1024  // ��󻺳�����С

using namespace std;
char user_id[10];  // �û�ID
SOCKET clientSocket;  //�ͻ���socket
time_t t;
char time_str[64];
// ������Ϣ�Ľ��̴���������
DWORD WINAPI recvThread();

int main()
{
	cout << "===============================================" << endl;
	cout << "==================����ͻ���===================" << endl;
	cout << "===============================================" << endl;
	cout << "����ͻ���[�汾 23.10.18.0]      ������-Yuzhao-" << endl;
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
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // ����ʵ�����InternetЭ��汾4��IPv4����ַ�塢��ʽ�׽����Լ�TCPЭ��
	if (clientSocket == INVALID_SOCKET) {
		cout << "[����] ����Socketʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		WSACleanup();  // �ͷ� Socket DLL ��Դ
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] ����Socket�ɹ���" << endl;
	//============================��������ַ===========================
	SOCKADDR_IN serverAddr;  // ����˵�ַ��Ϣ�����ݽṹIPv4
	serverAddr.sin_family = AF_INET;  // Э���壬IPv4��ַ��
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);  // ��ַ
	//inet_pton(AF_INET, "192.168.192.1", &serverAddr.sin_addr.s_addr);  // ��ַ
	serverAddr.sin_port = htons(PORT);  // �˿ں�
	//===========================���� connect==========================
	err = connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {
		cout << "[����] ����������ʧ�ܣ��������: " << WSAGetLastError() << endl;
		cout << "===============================================" << endl;
		system("pause");
		return 1;
	}
	cout << "[ϵͳ��ʾ] ���������ӳɹ���" << endl;

	//========================��ӡ��������ı�־========================
	recv(clientSocket, user_id, 10, 0);
	// ��ȡʱ��
	t = time(NULL);
	ctime_s(time_str, sizeof(time_str), &t);
	cout << "===============================================" << endl;
	cout << "             ��ӭ    �û�[" << user_id << "]" << endl;
	cout << "***********************************************" << endl;
	cout << "        �˳�˵����" << endl;
	cout << "        $ exit/Exit/EXIT" << endl;
	cout << "        $ quit/Quit/QUIT" << endl;
	cout << "        $ �رճ���" << endl;
	cout << "-----------------------------------------------" << endl;
	cout << "        ʱ�䣺" << time_str;
	cout << "===============================================" << endl;
	//========================����������Ϣ���߳�=======================
	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recvThread, LPVOID(clientSocket), 0, NULL);
	//==============================���߳�=============================
	while (true) {
		cout << "�û�[" << user_id << "]>";
		char buf[MaxBufSize];
		cin.getline(buf, sizeof(buf));
		if (strcmp(buf, "exit") == 0 || strcmp(buf, "Exit") == 0 || strcmp(buf, "EXIT") == 0
			|| strcmp(buf, "quit") == 0 || strcmp(buf, "Quit") == 0 || strcmp(buf, "QUIT") == 0)
		{  // �˳��ж�
			break;
		}
		// ����
		send(clientSocket, buf, sizeof(buf), 0);
	}
	CloseHandle(hThread);
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

// ������Ϣ�Ľ��̴�����
DWORD WINAPI recvThread()
{
	while (1)
	{
		char RecvBuf[MaxBufSize];  // ������Ϣ�Ļ�����
		int recvResult = recv(clientSocket, RecvBuf, sizeof(RecvBuf), 0);
		if (recvResult > 0)
		{
			cout << "\r";
			cout << "�û�[" << user_id << "]<" << " �յ���Ϣ! " << endl;
			cout << "$$ " << RecvBuf << endl;
			// ��ȡʱ��
			t = time(NULL);
			ctime_s(time_str, sizeof(time_str), &t);
			cout << "$$ ��������Ϣʱ�䡿:" << time_str;
			cout << "�û�[" << user_id << "]>";
		}
		else if (recvResult < 0)  // �Ͽ�����
		{
			cout << "[����] �Ͽ�����..." << endl;
			cout << "�����Ƿ������˳��������ʲô���⣿" << endl;
			break;
		}
	}
	return 0;
}