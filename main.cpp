#define INITGUID

#include <windows.h>
#include <thread>
#include <SetupAPI.h>
#include "Cfgmgr32.h"
#include <iostream>
#include <Devpkey.h>
#include <mutex>
#pragma comment (lib, "SetupAPI")
#pragma comment (lib, "Cfgmgr32")

SERVICE_STATUS g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = nullptr;

#define SERVICE_NAME L"TrackpadHackService"
#define TABLET_MODE 0
#define LAPTOP_MODE 1

#define TOUCHPAD_HWID L"HID\\VEN_ELAN&DEV_1201&Col02"

std::mutex svcMutex;

enum State
{
	LAPTOP, 
	TABLET,
	UNKNOWN
};
	
State currentState = State::UNKNOWN;
int lastSlateValue = -1;

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
			//Check if device is already in the state we want
			ULONG currStatus, problem = 0;

			if (CM_Get_DevNode_Status(&currStatus, &problem, devData.DevInst, 0) == CR_SUCCESS)
			{
				bool isEnabled = (currStatus & DN_STARTED);

				if (isEnabled != enable)
				{
					//Enable/disable
					SP_CLASSINSTALL_HEADER ciHeader;
					ciHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
					ciHeader.InstallFunction = DIF_PROPERTYCHANGE;

					SP_PROPCHANGE_PARAMS pcParams;
					pcParams.ClassInstallHeader = ciHeader;
					pcParams.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
					pcParams.Scope = DICS_FLAG_GLOBAL;
					pcParams.HwProfile = 0;

					SetupDiSetClassInstallParams(devInfo, &devData, (PSP_CLASSINSTALL_HEADER)&pcParams, sizeof(SP_PROPCHANGE_PARAMS));
					SetupDiChangeState(devInfo, &devData);
				}

				free(devBuf);
				break;
			}
		}

		free(devBuf);
		++i;
	}
	SetupDiDestroyDeviceInfoList(devInfo);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	// TODO implement these
		
	switch (dwCtrl)
	{
		case SERVICE_CONTROL_STOP:
			// Signal the service to stop.
			SetEvent(g_ServiceStopEvent);
			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;
		case SERVICE_CONTROL_POWEREVENT:
			//Windows re-enables the trackpad, but continues to report it as disabeld (if we went to sleep wiht it off)
			//It also mis-reports whether we are in laptop or tablet mode
			//To prevent blocking the user by basing our logic on this incorrect information, go into an unknown state and
			//wait for the convertableslatemode value to change, so we know we're working with reliable data again
			if (dwEventType == PBT_APMRESUMEAUTOMATIC || dwEventType == PBT_APMRESUMESUSPEND)
			{
				svcMutex.lock();
				currentState = State::UNKNOWN;
				ToggleTouchpad(true);
				svcMutex.unlock();
			}
			break;

		default:
			break;
	}
}

void RunService()
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
		svcMutex.lock();

		if(currentState == State::UNKNOWN)
		{
			//Wait for value to change
			if(value == lastSlateValue)
			{
				svcMutex.unlock();
				return;
			}

			lastSlateValue = value;
		}
		switch (value)
		{
			case TABLET_MODE:
			{
				currentState = State::TABLET;
				ToggleTouchpad(false);
				break;
			}
			case LAPTOP_MODE:
			{
				currentState = State::LAPTOP;
				ToggleTouchpad(true);
				break;
			}
		}

		svcMutex.unlock();
	}
}

void WINAPI ServiceMain(DWORD, LPTSTR*) {
	g_StatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, (LPHANDLER_FUNCTION_EX)SvcCtrlHandler, NULL);

	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	while (true)
	{
		RunService();
		if (WaitForSingleObject(g_ServiceStopEvent, 500) == WAIT_OBJECT_0)
		{
			break;
		}
	}

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}


int wmain(int argc, wchar_t* argv[])
{
	//Uncomment to run standalone
	/*while (true)
	{
		RunService();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}*/
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ nullptr, nullptr }
	};

	StartServiceCtrlDispatcher(ServiceTable);
	return 0;
}
