
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include "AntMessage.h"

#include <string>
#include <vector>
#include <locale>
#include <fstream>
#include <sstream>

// Put include files outside the pragma section.
#pragma warning (push, 4)

namespace {
	using namespace AntfsTool;

  std::string g_BaseDirectory;

  const char *g_KeyFileName = "auth_key.dat";
  const char *g_AppName = "AntfsTool";

  struct FileTypeMap {
    AntfsFileSybType m_Type;
    const char *m_Name;
  } g_AntDirectory[] = {
    { FST_DEVICE, "." },
    { FST_SETTING, "Settings" },
    { FST_SPORT, "Sports" },
    { FST_ACTIVITY, "Activities" },
    { FST_WORKOUT, "Workouts" },
    { FST_COURSE, "Courses" },
    { FST_SCHEDULES, "Schedules" },
    { FST_WEIGHT, "Weight" },
    { FST_TOTALS, "Totals" },
    { FST_GOALS, "Goals" },
    { FST_BLOOD_PRESSURE, "Blood Pressure" },
    { FST_MONITORING_A, "Monitoring" },
    { FST_ACTIVITY_SUMMARY, "Activities" },
    { FST_MONITORING_DAILY, "Monitoring" },
    { FST_MONITORING_B, "Monitoring" } };

  const int g_NumAntDirectoryEntries = 
    sizeof(g_AntDirectory) / sizeof (g_AntDirectory[0]);


  std::string wstr2str(const std::wstring &w)
  {
	  std::locale locale("");
	  typedef std::codecvt<wchar_t, char, std::mbstate_t> converter_type;
	  const converter_type& converter = std::use_facet<converter_type>(locale);
	  std::vector<char> to(w.length() * converter.max_length());
	  std::mbstate_t state;
	  const wchar_t* from_next = nullptr;
	  char* to_next = nullptr;
	  const converter_type::result result = 
		  converter.out(state, 
		  w.data(), w.data() + w.length(), from_next, 
		  &to[0], &to[0] + to.size(), to_next);
	  if (result == converter_type::ok || result == converter_type::noconv) {
		  const std::string s(&to[0], to_next);
		  return s;
	  }
	  return "";
  }

  void EnsureDirectoryExists(const std::string &path)
  {
	  char tmp[MAX_PATH];
	  strcpy(tmp, path.c_str());
	  for (char *p = strchr(tmp, '\\'); p; p = strchr(p + 1, '\\')) 
	  {
		  *p = 0;
		  ::CreateDirectoryA(tmp, nullptr);
		  *p = '\\';
	  }
	  ::CreateDirectoryA(tmp, nullptr);
  }

  void Init()
  {
    PWSTR path = nullptr;
    HRESULT r = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &path);
    std::string app_data = "c:\\";      // silly default
    if (r == S_OK) {
      app_data = wstr2str(std::wstring(path));
      CoTaskMemFree(path);
    }
    g_BaseDirectory = app_data + "\\" + g_AppName;
    EnsureDirectoryExists(g_BaseDirectory);
  }

  std::string GetDeviceStoragePath(unsigned device_serial)
  {
    if (g_BaseDirectory == "") Init();
    std::ostringstream p;
    p << g_BaseDirectory << "\\" << device_serial;
    std::string path = p.str();
    EnsureDirectoryExists(path);
    return path;
  }

  std::string GetKeyFile(unsigned device_serial)
  {
    std::ostringstream fn;
    fn << GetDeviceStoragePath(device_serial) << "\\" << g_KeyFileName;
    return fn.str();
  }

  const char* GetDirForFileType(AntfsFileSybType t)
  {
    for (int i = 0; i < g_NumAntDirectoryEntries; i++) {
      if (t == g_AntDirectory[i].m_Type)
        return g_AntDirectory[i].m_Name;
    }
    return "Unknown";
  }

};                                      // end anonymous namespace

namespace AntfsTool {

std::string GetFileStorageDirectory(unsigned device_serial, AntfsFileSybType t)
{
  if (g_BaseDirectory == "") Init();
  std::ostringstream p;
  p << g_BaseDirectory << "\\" << device_serial << "\\" << GetDirForFileType(t);
  std::string path = p.str();
  EnsureDirectoryExists(path);
  return path;
}

void PutKey (unsigned device_serial, const Buffer &key)
{
  if (! key.empty()) {
    std::ostringstream fn;
    std::ofstream o (GetKeyFile(device_serial).c_str(), std::ios::binary);
    if (o) {
      o.write (reinterpret_cast<const char*>(&key[0]), key.size());
    }
  }
}

Buffer GetKey(unsigned device_serial)
{
  std::ifstream i (GetKeyFile(device_serial).c_str(), std::ios::binary);
  if(i) {
    i.seekg (0, std::ios::end);
    std::streamsize size = i.tellg();
    i.seekg (0);

    Buffer key (static_cast<unsigned int>(size));
    i.read (reinterpret_cast<char*>(&key[0]), size);
      
    return key;
  }
  else {
    return Buffer();
  }
}

void RemoveKey(unsigned device_serial)
{
  auto f = GetKeyFile(device_serial);
  DeleteFileA(f.c_str());
}

}; // end namespace AntfsTool

#pragma warning (pop)
