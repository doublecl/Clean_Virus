// killvirus.c : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>
#include <tchar.h>


void cleanFile(char *filename, int size) {
	HANDLE hMapObject, hFile; 			//File Mapping Object.
	LPVOID lpBase; 						//Pointer to the base memory of mapped file
	PIMAGE_DOS_HEADER dosHeader; 		//Pointer to Dos Header
	PIMAGE_NT_HEADERS ntHeader;			//Pointer to Nt Header
	PIMAGE_OPTIONAL_HEADER PopHeader;		//Optional Header of PE files present in NT Header structure
											//Open the File 
	hFile = CreateFile((LPCWSTR)filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	};
	//Mapping Exe File to Memory
	hMapObject = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	//Get Pointer to address of Exe File in Memory
	lpBase = MapViewOfFile(hMapObject, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);

	//Get Dos Header
	dosHeader = (PIMAGE_DOS_HEADER)lpBase;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		UnmapViewOfFile(lpBase);
		CloseHandle(hMapObject);
		CloseHandle(hFile);
		return;
	}
	//Get Nt Header
	ntHeader = (PIMAGE_NT_HEADERS)((DWORD)lpBase + dosHeader->e_lfanew);
	if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
		UnmapViewOfFile(lpBase);
		CloseHandle(hMapObject);
		CloseHandle(hFile);
		return;
	}
	//Get Optional Header
	PopHeader = &(ntHeader->OptionalHeader);
	PIMAGE_SECTION_HEADER pSecHeader;	//Section Header or Section Table Header
	PIMAGE_SECTION_HEADER pTextSecHeader;	//Section Header or Section Table Header
	int i;
	pSecHeader = IMAGE_FIRST_SECTION(ntHeader);
	pTextSecHeader = IMAGE_FIRST_SECTION(ntHeader);
	for (i = 0; i < ntHeader->FileHeader.NumberOfSections - 1; i++, pSecHeader++) {
		if (!strcmp((char*)(pSecHeader->Name), ".text")) {
			pTextSecHeader = pSecHeader;
			if (pSecHeader->VirtualAddress <= PopHeader->AddressOfEntryPoint && PopHeader->AddressOfEntryPoint <= (pSecHeader->VirtualAddress + pSecHeader->SizeOfRawData)) {
				UnmapViewOfFile(lpBase);
				CloseHandle(hMapObject);
				CloseHandle(hFile);
				return;
			}
		}
	}
	//pSecHeader is last section
	if (strcmp((char*)pSecHeader->Name, ".text")) {
		UnmapViewOfFile(lpBase);
		CloseHandle(hMapObject);
		CloseHandle(hFile);
		return;
	}
	BYTE *rawData = (BYTE*)((DWORD)lpBase + pSecHeader->PointerToRawData);
	if (memcmp(rawData, "\x60\xe8\x00\x00\x00\x00\x5d\x8b\xc5\x81\xed\xa8\xa6\x01\x20\x2b", 16)) {
		UnmapViewOfFile(lpBase);
		CloseHandle(hMapObject);
		CloseHandle(hFile);
		return;
	}
	DWORD entrypoint = PopHeader->AddressOfEntryPoint - *(DWORD*)(rawData + 1905);
	if (pTextSecHeader->VirtualAddress >= entrypoint || entrypoint >= (pTextSecHeader->VirtualAddress + pTextSecHeader->SizeOfRawData)) {
		UnmapViewOfFile(lpBase);
		CloseHandle(hMapObject);
		CloseHandle(hFile);
		return;
	}
	ntHeader->FileHeader.NumberOfSections -= 1;
	PopHeader->AddressOfEntryPoint = entrypoint;
	DWORD sizeofRaw = pSecHeader->SizeOfRawData;
	PopHeader->SizeOfImage = pSecHeader->VirtualAddress;
	memset(rawData, 0, sizeofRaw);
	pSecHeader->Characteristics = 0;
	pSecHeader->Misc.VirtualSize = 0;
	pSecHeader->VirtualAddress = 0;
	pSecHeader->PointerToRawData = 0;
	pSecHeader->SizeOfRawData = 0;
	pSecHeader->NumberOfRelocations = 0;
	pSecHeader->NumberOfLinenumbers = 0;
	pSecHeader->PointerToRelocations = 0;
	pSecHeader->PointerToLinenumbers = 0;
	printf("Killed %s\n", filename);
	UnmapViewOfFile(lpBase);
	CloseHandle(hMapObject);
	CloseHandle(hFile);
	return;
}

void findFile(char *dir) {
	//Scan file in folder
	WIN32_FIND_DATA ffd;
	LARGE_INTEGER filesize;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0;
	char tmp[256];
	sprintf(tmp, "%s%s", dir, "/*");
	hFind = FindFirstFile((LPCWSTR)tmp, &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf("ERROR!\n");
		return;
	}

	// List all the files in the directory with some info about them.

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	//Directory
		{
			if (strcmp((char*)ffd.cFileName, ".") && strcmp((char*)ffd.cFileName, "..")) {
				char tmp1[256];
				strcpy(tmp1, dir);
				strcat(tmp1, "/");
				strcat(tmp1, (const char*)ffd.cFileName);
				//printf("%s\n", tmp1);
				findFile(tmp1);
			}
		}
		else
		{
			filesize.LowPart = ffd.nFileSizeLow;
			filesize.HighPart = ffd.nFileSizeHigh;
			int size = filesize.QuadPart;
			char filename[1024];
			sprintf(filename, "%s/%s", dir, ffd.cFileName);
			cleanFile(filename, size);
			//printf("%s - size %u byte\n", filename, size);
		}
	} while (FindNextFile(hFind, &ffd) != 0);
}

int main(int argc, char ** argv) {
	if (argc == 2) {
		findFile(argv[1]);
	}
	else {
		findFile(".");
	}
}