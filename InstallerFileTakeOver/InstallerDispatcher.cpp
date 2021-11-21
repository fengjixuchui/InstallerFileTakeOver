#include "InstallerDispatcher.h"
#include <Windows.h>
#include <strsafe.h>
#include <Objbase.h>
#include "resource.h"
#include <string>

struct Internal {
	WCHAR* targetdir;
	WCHAR* msi_pkg;
};

bool InternalRecursiveRemoveDirectory(std::wstring dir)
{

	DWORD fst_attr = GetFileAttributes(dir.c_str());
	if (fst_attr & FILE_ATTRIBUTE_NORMAL)
		return DeleteFile(dir.c_str());
	if (fst_attr & FILE_ATTRIBUTE_REPARSE_POINT)
		return RemoveDirectoryW(dir.c_str());
	std::wstring search_path = std::wstring(dir) + L"\\*.*";
	std::wstring s_p = std::wstring(dir) + std::wstring(L"\\");
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(search_path.c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			{
				continue;
			}
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				DeleteFile(std::wstring(s_p + fd.cFileName).c_str());
				continue;
			}
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
				RemoveDirectory(std::wstring(s_p + fd.cFileName).c_str());
				continue;
			}
			InternalRecursiveRemoveDirectory(s_p + fd.cFileName);
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
	if (RemoveDirectoryW(dir.c_str()) != 0) {
		return false;
	}
	return true;
}


InstallerDispatcher::InstallerDispatcher() {

	GUID gd;
	HRESULT hs = CoCreateGuid(&gd);
	WCHAR mx[MAX_PATH];
	int x = StringFromGUID2(gd, mx, MAX_PATH);
	StringCchCat(mx, MAX_PATH, L".msi");
	WCHAR temp_dir[MAX_PATH] = L"%TEMP%\\";
	StringCchCat(temp_dir, MAX_PATH, mx);
	ExpandEnvironmentStrings(temp_dir, msi_file, MAX_PATH);

	HANDLE hmsi_file = CreateFile(msi_file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hmsi_file == INVALID_HANDLE_VALUE)
		throw(GetLastError());
	HMODULE hMod = GetModuleHandle(NULL);
	HRSRC msires = FindResource(hMod, MAKEINTRESOURCE(IDR_MSI1), L"msi");
	DWORD msiSize = SizeofResource(hMod, msires);
	void* msiBuff = LoadResource(hMod, msires);
	DWORD nb = 0;
	WriteFile(hmsi_file, msiBuff, msiSize, &nb, NULL);
	if (nb != msiSize)
		throw(GetLastError());
	FreeResource(msiBuff);
	CloseHandle(hmsi_file);
}
DWORD WINAPI RunAdminInstallInternal(void* __internal) {
	
	Internal* _int = (Internal*)__internal;

	WCHAR cmdline[1024] = L"ACTION=ADMIN TARGETDIR=";
	StringCchCat(cmdline, 1024, _int->targetdir);
	MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
	MsiInstallProduct(_int->msi_pkg, cmdline);

	return ERROR_SUCCESS;
}
void InstallerDispatcher::RunAdminInstall(WCHAR* targetdir) {
	wcscpy_s(InternalInstallDir, targetdir);
	Internal _int = { targetdir, msi_file };
	DWORD tid = 0;
	this->InstallerDispatcherThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)RunAdminInstallInternal, (void*)&_int, NULL, &tid);
}
InstallerDispatcher::~InstallerDispatcher() {

	if (this->InstallerDispatcherThread) {
		if (WaitForSingleObject(this->InstallerDispatcherThread, 5000) == WAIT_TIMEOUT) {
			TerminateThread(this->InstallerDispatcherThread, 1);
		}
		CloseHandle(this->InstallerDispatcherThread);
	}
	DeleteFile(msi_file);
	InternalRecursiveRemoveDirectory(InternalInstallDir);
}