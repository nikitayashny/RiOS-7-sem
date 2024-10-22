#include "Header.h"

DWORD WINAPI SyncGlobalUnixTime(void* lpar)
{
	WSAData wsaData;
	SOCKET s;
	SOCKADDR_IN server;

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("178.124.134.106");		//88.147.254.229 178.124.134.106
	server.sin_port = htons(123);

	int h = CLOCKS_PER_SEC;
	clock_t t = clock();
	int d = 613608 * 3600;			// 613608 * 3600 сек NTP - UNIX
	time_t ttime; time(&ttime);		// количество сек с 1.1.1970:00:00

	NTP_packet out_buf, in_buf;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) throw WSAGetLastError();

	while (true)
	{
		ZeroMemory(&out_buf, sizeof(out_buf));
		ZeroMemory(&in_buf, sizeof(in_buf));
		out_buf.head[0] = 0x1B;
		out_buf.head[1] = 0x00;
		out_buf.head[2] = 4;
		out_buf.head[3] = 0xEC;
		DWORD64 tms = 0, oms = 0;
		try {
			if ((s = socket(AF_INET, SOCK_DGRAM, NULL)) == INVALID_SOCKET) throw WSAGetLastError();

			DWORD timeout = 20000;
			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD));

			int lenout = 0, lenin = 0, lensockaddr = sizeof(server);
			clock_t qtime = clock();



			if ((lenout = sendto(s, (char*)&out_buf, sizeof(out_buf), NULL, (sockaddr*)&server, sizeof(server))) == SOCKET_ERROR)
				throw GetLastError(WSAGetLastError());

			if ((lenin = recvfrom(s, (char*)&in_buf, sizeof(in_buf), NULL, (sockaddr*)&server, &lensockaddr)) == SOCKET_ERROR)
				throw GetLastError(WSAGetLastError());



			clock_t transmissionTime = clock() - qtime;
			DWORD64 localUnixTime = GetLocalUnixTime();

			in_buf.TransmitTimestamp[0] = ntohl(in_buf.TransmitTimestamp[0]) - d;
			in_buf.OriginateTimestamp[0] = ntohl(in_buf.OriginateTimestamp[0]) - d;
			in_buf.TransmitTimestamp[1] = ntohl(in_buf.TransmitTimestamp[1]);
			in_buf.OriginateTimestamp[1] = ntohl(in_buf.OriginateTimestamp[1]);

			//считаем время обработки запроса
			tms = (DWORD64)((1000.0 * ((double)(in_buf.TransmitTimestamp[1]) / (double)0xffffffff)) + (DWORD64)in_buf.TransmitTimestamp[0] * 1000);	// мс
			oms = (DWORD64)((1000.0 * ((double)(in_buf.OriginateTimestamp[1]) / (double)0xffffffff)) + (DWORD64)in_buf.OriginateTimestamp[0] * 1000);	// мс

			DWORD64 transmissionCorrection = (transmissionTime - ((DWORD64)tms - (DWORD64)oms)) / 2;//в пути
			tms = (DWORD64)tms + transmissionCorrection;

			GETSINCHRO* t = (GETSINCHRO*)lpar;
			EnterCriticalSection(&csServerTime);
			lastSync = clock();
			t->mTime = (DWORD64)tms;
			LeaveCriticalSection(&csServerTime);

			cout << "синхронизация NTP c глобальным" << endl;

			cout << "локальное UNIX время: " << localUnixTime << " Глобальное NTP: " << tms << endl;

			if (tms > localUnixTime)
			{
				cout << "Глобальное больше на " << tms - localUnixTime << endl;
			}
			else
			{
				cout << "Локальное больше на " << localUnixTime - tms << endl;
			}
			cout << endl;
			if (closesocket(s) == INVALID_SOCKET) throw WSAGetLastError();

			Sleep(10000);
		}
		catch (int e) {
			std::cout << "error:" << e << std::endl;
		}
		catch (string errorMsgText)
		{
			cout << errorMsgText;
		}

	}
	if (WSACleanup() == SOCKET_ERROR) throw WSAGetLastError();
	return 0;
}

int main()
{
	setlocale(LC_ALL, "Russian");
	cout << "NTP сервер запущен" << endl;

	InitializeCriticalSection(&csServerTime);
	GETSINCHRO ExTime;

	HANDLE hSyncUnixTime = CreateThread(0, 0, SyncGlobalUnixTime, &ExTime, 0, 0);

	try
	{
		SOCKET serverSocket;
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
			throw GetLastError(WSAGetLastError());

		if ((serverSocket = socket(AF_INET, SOCK_DGRAM, NULL)) == INVALID_SOCKET)
			throw GetLastError(WSAGetLastError());

		SOCKADDR_IN serverParameters;
		serverParameters.sin_family = AF_INET;
		serverParameters.sin_port = htons(2000);
		serverParameters.sin_addr.s_addr = INADDR_ANY;

		if (bind(serverSocket, (LPSOCKADDR)&serverParameters, sizeof(serverParameters)) == SOCKET_ERROR)
			throw GetLastError(WSAGetLastError());

		while (true)
		{
			SOCKADDR_IN clientParameters;
			int clientParametersLength = sizeof(clientParameters);

			GETSINCHRO gs;
			if ((recvfrom(serverSocket, (char*)&gs, sizeof(gs), NULL, (sockaddr*)&clientParameters, &clientParametersLength)) == SOCKET_ERROR)
				throw GetLastError(WSAGetLastError());

			SETSINCHRO ss;

			EnterCriticalSection(&csServerTime);
			int delta = clock() - lastSync;

			//время + тик синх - время клиента
			ss.corrTime = ExTime.mTime + delta - gs.mTime;
			LeaveCriticalSection(&csServerTime);

			sendto(serverSocket, (char*)&ss, sizeof(ss), 0, (sockaddr*)&clientParameters, sizeof(clientParameters));
		}

		if (closesocket(serverSocket) == SOCKET_ERROR)
			throw GetLastError(WSAGetLastError());
		if (WSACleanup() == SOCKET_ERROR)
			throw GetLastError(WSAGetLastError());
	}
	catch (string errorMsgText)
	{
		cout << endl << errorMsgText;
	}

	WaitForSingleObject(hSyncUnixTime, INFINITE);
	DeleteCriticalSection(&csServerTime);
	return 0;
}