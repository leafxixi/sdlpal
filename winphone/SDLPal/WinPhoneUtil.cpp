#include <wrl.h>
#include <string>
#include <DXGI.h>
#include <ppltasks.h>
#include <unordered_map>
#include "AsyncHelper.h"

#define PAL_PATH_NAME	"SDLPAL"

static std::string g_savepath, g_basepath;
static Windows::Storage::StorageFolder^ g_root;

static void ConvertString(Platform::String^ src, std::string& dst)
{
	int len = WideCharToMultiByte(CP_ACP, 0, src->Begin(), -1, nullptr, 0, nullptr, nullptr);
	dst.resize(len - 1);
	WideCharToMultiByte(CP_ACP, 0, src->Begin(), -1, (char*)dst.data(), len, nullptr, nullptr);
}

static bool CheckGamePath(Windows::Storage::StorageFolder^ root)
{
	Platform::String^ required_files[] = {
		L"ABC.MKF", L"BALL.MKF", L"DATA.MKF", L"F.MKF", L"FBP.MKF",
		L"FIRE.MKF", L"GOP.MKF", L"MAP.MKF", L"MGO.MKF", L"PAT.MKF",
		L"RGM.MKF", L"RNG.MKF", L"SSS.MKF"
	};
	Platform::String^ optional_required_files[] = {
		L"VOC.MKF", L"SOUNDS.MKF"
	};
	/* The words.dat & m.msg may be configurable in the future, so not check here */

	try
	{
		/* Check the access right of necessary files */
		auto folder = AWait(root->GetFolderAsync(PAL_PATH_NAME));
		for (int i = 0; i < 13; i++)
		{
			try {
				auto file = AWait(folder->GetFileAsync(required_files[i]));
			}
			catch (Platform::Exception^ e) {
				return false;
			}
		}
		for (int i = 0; i < 2; i++)
		{
			try
			{
				auto filetask = AWait(folder->GetFileAsync(optional_required_files[i]));
				g_root = folder;
				return true;
			}
			catch (Platform::Exception^ e) {}
		}
	}
	catch(Platform::Exception^ e)
	{ /* Accessing SD card failed, or required file is missing, or access is denied */	}
	return false;
}

extern "C"
LPCSTR UTIL_BasePath(VOID)
{
	if (g_basepath.empty())
	{
		auto folderiter = AWait(Windows::Storage::KnownFolders::PicturesLibrary->GetFoldersAsync())->First();
		while (folderiter->HasCurrent)
		{
			if (CheckGamePath(folderiter->Current))
			{
				/* Folder examination succeeded */
				auto folder = folderiter->Current->Path;
				if (folder->End()[-1] != L'\\') folder += "\\";
				folder += PAL_PATH_NAME "\\";
				ConvertString(folder, g_basepath);

				g_savepath = g_basepath;
				break;
			}
			folderiter->MoveNext();
		}

		if (g_basepath.empty())
		{
			auto path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path+"\\Assets\\Data\\";
			ConvertString(path, g_basepath);
		}
	}
	return g_basepath.c_str();
}

extern "C"
LPCSTR UTIL_SavePath(VOID)
{
	if (g_savepath.empty())
	{
		auto localfolder = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
		if (localfolder->End()[-1] != L'\\') localfolder += "\\";
		ConvertString(localfolder, g_savepath);
	}
	return g_savepath.c_str();
}

extern "C"
BOOL UTIL_GetScreenSize(DWORD *pdwScreenWidth, DWORD *pdwScreenHeight)
{
	DXGI_OUTPUT_DESC desc;
	IDXGIFactory1* pFactory = nullptr;
	IDXGIAdapter1* pAdapter = nullptr;
	IDXGIOutput* pOutput = nullptr;
	DWORD retval = FALSE;

	if (!pdwScreenWidth || !pdwScreenHeight) return FALSE;

	if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory1, (void**)&pFactory))) goto UTIL_WP_GetScreenSize_exit;

	if (FAILED(pFactory->EnumAdapters1(0, &pAdapter))) goto UTIL_WP_GetScreenSize_exit;

	if (FAILED(pAdapter->EnumOutputs(0, &pOutput))) goto UTIL_WP_GetScreenSize_exit;

	if (SUCCEEDED(pOutput->GetDesc(&desc)))
	{
		*pdwScreenWidth = (desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
		*pdwScreenHeight = (desc.DesktopCoordinates.right - desc.DesktopCoordinates.left);
		retval = TRUE;
	}

UTIL_WP_GetScreenSize_exit:
	if (pOutput) pOutput->Release();
	if (pAdapter) pAdapter->Release();
	if (pFactory) pFactory->Release();

	return retval;
}

extern "C" {
	std::hash<std::string> hasher;
	std::unordered_map<FILE*, std::wstring> fileMap;
	std::unordered_map<FILE*, size_t> fileSizeMap;
	std::unordered_map<FILE*, long> fileOffsetMap;
	std::unordered_map<FILE*, Platform::Array<uint8>^ > fileContentMap;
	char outputStr[512];

	auto openFile(const std::wstring &wid_str) {
		std::wstring last = wid_str.substr(wid_str.find_last_of(L'\\') + 1, wid_str.length());
		const wchar_t* w_filename = last.c_str();
		Platform::String ^nFilename = ref new Platform::String(w_filename);
		auto storageFile = AWait(g_root->GetFileAsync(nFilename));
		return storageFile;
	}
	size_t file_length(FILE *fp) {
		auto sizeIter = fileSizeMap.find(fp);
		if (sizeIter == fileSizeMap.end()) {
			auto storageFile = openFile(fileMap[fp]);
			auto basicInfo = AWait(storageFile->GetBasicPropertiesAsync());
			fileSizeMap[fp] = basicInfo->Size;
			return basicInfo->Size;
		}
		else {
			return sizeIter->second;
		}
	}

	FILE *fopen_uwp(const char *filename, const char *mode) {
		size_t newsize = strlen(filename) + 1;
		wchar_t * wcstring = new wchar_t[newsize];
		size_t convertedChars = 0;
		setlocale(LC_ALL, "chs");
		mbstowcs_s(&convertedChars, wcstring, newsize, filename, _TRUNCATE);
		std::wstring wid_str = std::wstring(wcstring);
		try {
			auto storageFile = openFile(wid_str);
			FILE *result = reinterpret_cast<FILE*>(hasher(filename));
			fileMap[result] = wid_str;
			fileOffsetMap[result] = 0;

			auto buffer = AWait(Windows::Storage::FileIO::ReadBufferAsync(storageFile));
			auto dataReader = Windows::Storage::Streams::DataReader::FromBuffer(buffer);
			auto length = dataReader->UnconsumedBufferLength;
			auto origLength = length;
			auto bytes = ref new Platform::Array<uint8>(length);
			dataReader->ReadBytes(bytes);
			fileContentMap[result] = bytes;
			return result;
		}
		catch (Platform::Exception^ e){
			return nullptr;
		}
	}
	long ftell_uwp(FILE *fp) {
		//auto storageFile = openFile(fileMap[fp]);
		return fileOffsetMap[fp];
	}
	int fseek_uwp(FILE *fp, long offset, int whence) {
		//auto storageFile = openFile(fileMap[fp]);
		long length = file_length(fp);
		switch (whence) {
		case SEEK_CUR:
				offset += ftell_uwp(fp);
				break;
		case SEEK_END:
			offset += length;
			break;
		}
		offset = min(max(0, offset), length);
		fileOffsetMap[fp] = offset;
		return offset;
	}
	void rewind_uwp(FILE *fp) {
		fseek_uwp(fp,0,SEEK_SET);
	}
	int fgetc_uwp(FILE *fp){
		unsigned char buf;
		fread_uwp(&buf,1,1,fp);
		return buf;
	}
	char *fgets_uwp(char *ptr, size_t length, FILE *fp) {
		auto storageFile = openFile(fileMap[fp]);
		auto origOffset = fileOffsetMap[fp];
		auto texts = AWait(Windows::Storage::FileIO::ReadLinesAsync(storageFile));
		std::string msg;
		bool eof;
		long offset = min(texts->Size-1, origOffset);
		ConvertString(texts->GetView()->GetAt(offset), msg);
		fileOffsetMap[fp] = offset+1;
		eof = (offset >= texts->Size-1);
		if( !eof )
			strcpy(ptr, msg.c_str());
		return eof ? nullptr : ptr;
	}
	size_t fread_uwp(void *ptr, size_t size, size_t nitems, FILE *fp) {
		auto length = size*nitems;
		auto fileSize = file_length(fp);
		auto bytes = fileContentMap[fp];
		auto offset = fileOffsetMap[fp];
		length = min(fileSize - offset, length);
		std::copy(bytes->begin()+offset, bytes->begin()+offset+length, (uint8*)ptr);
		fileOffsetMap[fp] = offset + length;
		return length;
	}
	size_t fwrite_uwp(const void *ptr, size_t size, size_t nitems, FILE *fp) {
		auto length = size * nitems;
		auto storageFile = openFile(fileMap[fp]);
		auto bytes = ref new Platform::Array<uint8>(length);
		std::copy((uint8*)ptr, (uint8*)ptr + length, bytes->begin());
		AWait(Windows::Storage::FileIO::WriteBytesAsync(storageFile, bytes));
		return length;
	}
	int fclose_uwp(FILE *fp) {
		auto storageFile = openFile(fileMap[fp]);
		return 0;
	}
}