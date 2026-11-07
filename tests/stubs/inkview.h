#pragma once

// Only what the sources under test call from the SDK's inkview.h, for host tests.

#include <cstddef>

// FLASHDIR comes from the .pro: a scratch folder for the tests' synthetic files, never a
// device's storage
#define CONFIGPATH FLASHDIR
#define AUDIOBOOKS_DEFAULT_PATH FLASHDIR "/Audiobooks"
#define DOWNLOADFOLDER FLASHDIR "/Downloads"

enum {
    NET_CONNECTED = 1,
    NET_BLUETOOTH = 2,
    NET_WIFI = 4,
    NET_CDMA3G = 8,
    NET_BTREADY = 16,
    NET_WIFIREADY = 32,
    NET_CDMA3GREADY = 64,
};
enum { OTHERTASKS = 0, EVT_STARTSCAN = 0, EVT_SCANSTOPPED = 1, OB_ADDTOLAST = 0, OB_WITHRETURN = 0, TASK_MAKEACTIVE = 0 };

struct iv_filetype {
    const char *extension;
    const char *program;
};

inline int QueryNetwork() { return NET_CONNECTED; }
inline const char *GetDeviceModel() { return "Test"; }
inline void adjustAreaDefault(unsigned char *, int, int, int) {}
inline int SendEventTo(int, int, int, int) { return 0; }
inline const iv_filetype *GetSupportedFileTypes() { return nullptr; }
inline int GetSupportedFileTypesLength() { return 0; }
inline const char *GetLangText(const char *key) { return key; }
inline void SetFileHandler(const char *, const char *) {}
inline int OpenBook(const char *, const char *, int) { return 0; }
inline int OpenTask(const char *, int, const char **, int) { return 0; }
