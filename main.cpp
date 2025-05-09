#define INITGUID

#include <windows.h>
#include <thread>
#include <SetupAPI.h>
#include "Cfgmgr32.h"
#include <iostream>
#include <Devpkey.h>
#pragma comment (lib, "SetupAPI")
#pragma comment (lib, "Cfgmgr32")

SERVICE_STATUS g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = nullptr;

#define SERVICE_NAME L"TrackpadHackService"
#define TABLET_MODE 0
#define LAPTOP_MODE 1

#define TOUCHPAD_HWID L"HID\\VEN_ELAN&DEV_1201&Col02"

void ToggleTouchpad(bool enable)
{
	HDEVINFO devInfo;
	devInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);

	if (devInfo == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD bufSize;
	SP_DEVINFO_DATA devData;
	DEVPROPTYPE devProptype;
	devData.cbSize = sizeof(SP_DEVINFO_DATA);
	LPWSTR devBuf;


	int i = 0;
	while (true)
	{
		SetupDiEnumDeviceInfo(devInfo, i, &devData);
		if (GetLastError() == ERROR_NO_MORE_ITEMS)
		{
			break;
		}

		//Get buf size
		SetupDiGetDeviceProperty(devInfo, &devData, &DEVPKEY_Device_HardwareIds, &devProptype, NULL, 0, &bufSize, 0);


		//Alloc
		devBuf = (LPWSTR)malloc(bufSize);
		if (!devBuf)
		{
			break;
		}

		SetupDiGetDeviceProperty(devInfo, &devData, &DEVPKEY_Device_HardwareIds, &devProptype, (PBYTE)devBuf, bufSize, NULL, 0);

		if (!wcscmp(devBuf, TOUCHPAD_HWID)) //Found our HWID
		{
			ULONG desiredStatus = enable ? DICS_ENABLE : DICS_DISABLE;
			//Check if device is already in the state we want
			ULONG currStatus, problem = 0;

			CM_Get_DevNode_Status(&currStatus, &problem, devData.DevInst, 0);

			//if (currStatus != desiredStatus) 
			//if ((problem == 22 && enable) || !enable)
			{
				//Enable/disable
				SP_CLASSINSTALL_HEADER ciHeader;
				ciHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
				ciHeader.InstallFunction = DIF_PROPERTYCHANGE;

				SP_PROPCHANGE_PARAMS pcParams;
				pcParams.ClassInstallHeader = ciHeader;
				pcParams.StateChange = desiredStatus;
				pcParams.Scope = DICS_FLAG_GLOBAL;
				pcParams.HwProfile = 0;

				SetupDiSetClassInstallParams(devInfo, &devData, (PSP_CLASSINSTALL_HEADER)&pcParams, sizeof(SP_PROPCHANGE_PARAMS));
				SetupDiChangeState(devInfo, &devData);
			}
			//else
			{
				//std::cout << "check is working";
			}

			break;
		}

		free(devBuf);
		++i;
	}
	SetupDiDestroyDeviceInfoList(devInfo);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	// Handle the requested control code. 

	switch (dwCtrl)
	{
		case SERVICE_CONTROL_STOP:
			//ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

			// Signal the service to stop.

			SetEvent(g_ServiceStopEvent);
			//ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}

}

void RunService()
{
	while (true)
	{
		DWORD value = 0;
		DWORD size = sizeof(DWORD);
		LRESULT result = RegGetValue(HKEY_LOCAL_MACHINE,
			L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl",
			L"ConvertibleSlateMode",
			RRF_RT_REG_DWORD,
			nullptr,
			&value,
			&size);

		if (result == ERROR_SUCCESS)
		{
			switch (value)
			{
				case TABLET_MODE:
				{
					ToggleTouchpad(false);
					break;
				}
				case LAPTOP_MODE:
				{
					ToggleTouchpad(true);
					break;
				}
			}
		}
		
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

void WINAPI ServiceMain(DWORD, LPTSTR*) {
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, SvcCtrlHandler);

	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	RunService();


	WaitForSingleObject(g_ServiceStopEvent, INFINITE);

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}


int wmain(int argc, wchar_t* argv[])
{
	//RunService();
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ nullptr, nullptr }
	};

	StartServiceCtrlDispatcher(ServiceTable);
	return 0;
}
