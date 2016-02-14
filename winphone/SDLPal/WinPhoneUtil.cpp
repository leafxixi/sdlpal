#include <wrl.h>
#include <string>
#include <DXGI.h>
#include <ppltasks.h>
#include <unordered_map>
#include <sstream>
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

	auto resourceContent = Windows::ApplicationModel::Resources::Core::ResourceContext::GetForCurrentView();
	auto qualifiedValues = resourceContent->QualifierValues;
	Platform::String ^DeviceFamily = L"DeviceFamily", ^Mobile = L"Mobile";
	if (!qualifiedValues->HasKey("DeviceFamily") || !Mobile->Equals(qualifiedValues->Lookup(DeviceFamily)))
		goto UTIL_WP_GetScreenSize_exit;

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
	struct EmulatedFileProperties {
		std::wstring filename;
		size_t size;
		long offset;
		std::istringstream *pstream;
		uint8 *buf;
	};
	std::unordered_map<FILE*, EmulatedFileProperties> filePropertiesMap;
	char outputStr[512];

	auto openFile(const std::wstring &wid_str) {
		std::wstring last = wid_str.substr(wid_str.find_last_of(L'\\') + 1, wid_str.length());
		const wchar_t* w_filename = last.c_str();
		Platform::String ^nFilename = ref new Platform::String(w_filename);
		auto storageFile = AWait(g_root->GetFileAsync(nFilename));
		return storageFile;
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

			auto buffer = AWait(Windows::Storage::FileIO::ReadBufferAsync(storageFile));
			auto dataReader = Windows::Storage::Streams::DataReader::FromBuffer(buffer);
			auto length = dataReader->UnconsumedBufferLength;
			auto bytes = ref new Platform::Array<uint8>(length);
			dataReader->ReadBytes(bytes);

			uint8 *buf = new uint8[length];
			std::copy(bytes->begin(), bytes->end(), buf);
			auto ss = new std::istringstream((char*)buf);

			filePropertiesMap[result] = { wid_str,length,0,ss,buf };
			return result;
		}
		catch (Platform::Exception^ e){
			return nullptr;
		}
	}
	long ftell_uwp(FILE *fp) {
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			return p.offset;
		}
		return 0;
	}
	int fseek_uwp(FILE *fp, long offset, int whence) {
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			long length = p.size;
			switch (whence) {
			case SEEK_CUR:
				offset += ftell_uwp(fp);
				break;
			case SEEK_END:
				offset += length;
				break;
			}
			offset = min(max(0, offset), length);
			p.offset = offset;
		}
		return offset;
	}
	void rewind_uwp(FILE *fp) {
		fseek_uwp(fp,0,SEEK_SET);
	}
	char *fgets_uwp(char *ptr, size_t length, FILE *fp) {
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			std::istringstream *ss = p.pstream;
			if (!ss->eof()) {
				ss->getline(ptr, length);
				return ptr;
			}
		}
		return nullptr;
	}
	size_t fread_uwp(void *ptr, size_t size, size_t nitems, FILE *fp) {
		auto length = size*nitems;
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			length = min(p.size - p.offset, length);
			std::copy(p.buf + p.offset, p.buf + p.offset + length, (uint8*)ptr);
			p.offset += length;
		}
		return length;
	}
	int fgetc_uwp(FILE *fp) {
		unsigned char buf;
		size_t readed = fread_uwp((void*)&buf, 1, 1, fp);
		return readed == 0 ? EOF : buf;
	}
	size_t fwrite_uwp(const void *ptr, size_t size, size_t nitems, FILE *fp) {
		auto length = size * nitems;
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			auto storageFile = openFile(p.filename);
			auto bytes = ref new Platform::Array<uint8>(length);
			std::copy((uint8*)ptr, (uint8*)ptr + length, bytes->begin());
			AWait(Windows::Storage::FileIO::WriteBytesAsync(storageFile, bytes));
		}
		return length;
	}
	int fclose_uwp(FILE *fp) {
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			delete p.pstream;
			delete[] p.buf;
		}
		filePropertiesMap.erase(fp);
		return 0;
	}
	int feof_uwp(FILE *fp) {
		EmulatedFileProperties &p = filePropertiesMap[fp];
		if (&p != nullptr) {
			std::istringstream *ss = p.pstream;
			if (ss->eof())
				return 1;
		}
		return 0;
	}
}