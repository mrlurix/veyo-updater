// Veyo Updater - Native Win32, Single EXE, ~400KB, No Qt
// Build: cmake -S native -B build-native -G "Visual Studio 17 2022" -A x64 && cmake --build build-native --config Release
// Or MinGW: g++ -O2 -std=c++17 native/src/main.cpp -o VeyoUpdater.exe -municode -mwindows -lcomctl32 -ldwmapi -luxtheme

#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <shellapi.h>
#include "../resources/resource.h"
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cmath>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Windows 11 Fluent theme - follows system Light/Dark (like Settings / Store)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#define DWMSBT_MAINWINDOW 2   // Mica
#endif

static bool g_dark = true; // detected from registry, default dark like Win11

static bool DetectDarkMode() {
    HKEY h;
    DWORD v = 1, sz = sizeof(v);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &h) == ERROR_SUCCESS) {
        DWORD type = 0;
        if (RegQueryValueExW(h, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&v, &sz) != ERROR_SUCCESS) v = 1;
        RegCloseKey(h);
    }
    return v == 0;
}
// Theme-aware colors (set in ApplyTheme)
static COLORREF CLR_BG, CLR_CARD, CLR_BORDER, CLR_TEXT, CLR_SUB, CLR_ACCENT, CLR_ACCENT_HOVER, CLR_ACCENT_PRESS;
static COLORREF CLR_BTN, CLR_BTN_HOVER, CLR_BTN_PRESS, CLR_BTN_BORDER;
static void ApplyThemeColors() {
    if (g_dark) {
        CLR_BG      = RGB(32, 32, 32);    // Settings dark page bg
        CLR_CARD    = RGB(44, 44, 44);    // card
        CLR_BORDER  = RGB(28, 28, 28);
        CLR_TEXT    = RGB(255, 255, 255);
        CLR_SUB     = RGB(200, 200, 200);
        CLR_BTN     = RGB(45, 45, 45);
        CLR_BTN_HOVER = RGB(55, 55, 55);
        CLR_BTN_PRESS = RGB(35, 35, 35);
        CLR_BTN_BORDER = RGB(70, 70, 70);
    } else {
        CLR_BG      = RGB(243, 243, 243); // Settings light page bg
        CLR_CARD    = RGB(255, 255, 255);
        CLR_BORDER  = RGB(225, 225, 225);
        CLR_TEXT    = RGB(27, 27, 27);
        CLR_SUB     = RGB(96, 94, 92);
        CLR_BTN     = RGB(255, 255, 255);
        CLR_BTN_HOVER = RGB(247, 247, 247);
        CLR_BTN_PRESS = RGB(240, 240, 240);
        CLR_BTN_BORDER = RGB(200, 200, 200);
    }
    CLR_ACCENT = RGB(232, 17, 35); // Veyo brand red (matches logo)
    // hover = slightly darker, press = darker
    CLR_ACCENT_HOVER = RGB(
        (BYTE)(GetRValue(CLR_ACCENT) * 0.9),
        (BYTE)(GetGValue(CLR_ACCENT) * 0.9),
        (BYTE)(GetBValue(CLR_ACCENT) * 0.9));
    CLR_ACCENT_PRESS = RGB(
        (BYTE)(GetRValue(CLR_ACCENT) * 0.8),
        (BYTE)(GetGValue(CLR_ACCENT) * 0.8),
        (BYTE)(GetBValue(CLR_ACCENT) * 0.8));
}

struct Package {
    std::wstring name;
    std::wstring id;
    std::wstring version;
    std::wstring available;
    std::wstring source;
    bool checked = true;
};

HWND hMain, hList, hRefresh, hUpgradeAll, hUpgradeSel, hStatus, hProgress, hCount, hSettings;
HICON hAppIcon = nullptr;
static void FitLastColumn();
struct JobItem { std::wstring id, name; };
struct JobInfo { std::wstring name; int idx = 1, total = 1; };
static std::wstring g_jobName;
static int g_jobIdx = 0, g_jobTotal = 0, g_lastPct = -1;
static std::wstring g_stage = L"Starting...";
HFONT hFontNormal, hFontBold, hFontMono, hFontTitle, hFontSmall;
static std::wstring g_wingetVer;
HBRUSH hBrBg, hBrCard, hBrCard2;
std::vector<Package> g_packages;
bool g_loading = false;
int g_sortCol = -1; bool g_sortAsc = true;
static int g_hotRow = -1; // row with mouse over its Update button
static int g_pendingRows = 0; // cascade insert progress
static int g_spinAngle = 0; // loading spinner angle
static HIMAGELIST g_hRowImg = nullptr; // forces tall Store-like rows
struct BtnAnim { HWND hw = nullptr; float v = 0, target = 0; }; // v: 0 normal, 1 hover, 2 pressed
static BtnAnim g_btns[4];
static int g_rowBtnRow = -1; // list row whose Update button is fading
static float g_rowBtnV = 0, g_rowBtnTarget = 0;

// Helpers
std::wstring s2ws(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back()==0) w.pop_back(); return w;
}
std::string ws2s(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr,nullptr);
    std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,&s[0],n,nullptr,nullptr);
    if (!s.empty() && s.back()==0) s.pop_back(); return s;
}
void Trim(std::wstring &s){ size_t a=0; while(a<s.size() && iswspace(s[a])) a++; size_t b=s.size(); while(b>a && iswspace(s[b-1])) b--; s=s.substr(a,b-a); }

// Parser -   Qt   wstring (fixed-width   )
std::vector<Package> ParseWingetOutput(const std::wstring& raw) {
    std::vector<Package> res;
    std::wstring clean = raw;
    //  BOM
    clean.erase(std::remove(clean.begin(), clean.end(), 0xFEFF), clean.end());
    clean.erase(std::remove(clean.begin(), clean.end(), 0xFFFE), clean.end());
    // \r\n -> \n
    std::wstring tmp; tmp.reserve(clean.size());
    for (size_t i=0;i<clean.size();++i){ if(clean[i]==L'\r'){ if(i+1<clean.size() && clean[i+1]==L'\n') continue; tmp.push_back(L'\n'); } else tmp.push_back(clean[i]); }
    clean = tmp;
    if (clean.find(L"No available upgrade")!=std::wstring::npos) return res;
    if (clean.find(L"No installed package")!=std::wstring::npos) return res;
    if (clean.find(L"No package found")!=std::wstring::npos) return res;
    // split lines
    std::vector<std::wstring> lines;
    std::wstringstream ss(clean); std::wstring line;
    while (std::getline(ss, line, L'\n')) lines.push_back(line);
    int headerIdx=-1, dashIdx=-1;
    for (int i=0;i<(int)lines.size();++i){
        std::wstring t=lines[i]; Trim(t);
        if (t.find(L"Name")!=std::wstring::npos && t.find(L"Id")!=std::wstring::npos && t.find(L"Version")!=std::wstring::npos){
            if (t.find(L"Available")!=std::wstring::npos || t.find(L"Source")!=std::wstring::npos){
                headerIdx=i;
                for(int k=i+1;k<std::min(i+4,(int)lines.size());++k) if(lines[k].find(L"---")!=std::wstring::npos){ dashIdx=k; break; }
                break;
            }
        }
    }
    if(headerIdx==-1||dashIdx==-1) return res;
    std::wstring header = lines[headerIdx];
    std::wstring dash = lines[dashIdx];
    //  dash     
    int dashBlocks=0; bool inDash=false;
    for(auto c:dash){ if(c==L'-'){ if(!inDash){dashBlocks++; inDash=true;}} else if(c!=L'\u2014') inDash=false; }
    //    => fixed-width   
    if(dashBlocks<2){
        int namePos = (int)header.find(L"Name");
        int idPos = (int)header.find(L"Id");
        int verPos = (int)header.find(L"Version");
        int availPos = (int)header.find(L"Available");
        int srcPos = (int)header.find(L"Source");
        if(idPos<0||verPos<0||availPos<0) return res;
        if(namePos<0) namePos=0;
        if(srcPos<0) srcPos=(int)header.size();
        auto slice=[&](const std::wstring& s,int a,int b)->std::wstring{
            if(a<0) a=0; if(b>(int)s.size()) b=(int)s.size(); if(a>=b) return L"";
            std::wstring r=s.substr(a,b-a); Trim(r); return r;
        };
        for(int i=dashIdx+1;i<(int)lines.size();++i){
            std::wstring l=lines[i];
            if(l.empty()) continue;
            std::wstring t=l; Trim(t);
            if(t.empty()) continue;
            if(t.rfind(L"---",0)==0) continue;
            if(t.find(L"upgrades available")!=std::wstring::npos) break;
            if(t.find(L"package(s) have version")!=std::wstring::npos) continue;
            if(t.size()<10) continue;
            std::wstring padded=l;
            if((int)padded.size() < srcPos+20) padded += std::wstring(srcPos+20 - padded.size(), L' ');
            Package p;
            p.name = slice(padded,namePos,idPos);
            p.id = slice(padded,idPos,verPos);
            p.version = slice(padded,verPos,availPos);
            p.available = slice(padded,availPos,srcPos);
            p.source = slice(padded,srcPos,(int)padded.size());
            if(p.source.empty()) p.source=L"winget";
            if(p.id.empty()||p.available.empty()) continue;
            if(p.id==L"Id") continue;
            res.push_back(p);
        }
        return res;
    }
    //   (winget ) -   dash
    struct Col{int s,e;}; std::vector<Col> cols;
    for(int i=0;i<(int)dash.size();){
        if(dash[i]==L'-'){ int a=i; while(i<(int)dash.size() && dash[i]==L'-') i++; cols.push_back({a,i}); }
        else i++;
    }
    if(cols.size()<4) return res;
    for(int i=dashIdx+1;i<(int)lines.size();++i){
        std::wstring l=lines[i];
        std::wstring t=l; Trim(t);
        if(t.empty()) continue;
        if(t.find(L"---")!=std::wstring::npos) continue;
        if(t.find(L"upgrades available")!=std::wstring::npos) break;
        std::wstring padded=l;
        if((int)padded.size() < (int)dash.size()) padded += std::wstring(dash.size()-padded.size(), L' ');
        auto get=[&](int idx)->std::wstring{
            if(idx>=(int)cols.size()) return L"";
            int a=cols[idx].s, b= idx+1<(int)cols.size()? cols[idx+1].s : (int)padded.size();
            if(a>=(int)padded.size()) return L"";
            if(b>(int)padded.size()) b=(int)padded.size();
            std::wstring r=padded.substr(a,b-a); Trim(r); return r;
        };
        Package p; p.name=get(0); p.id=get(1); p.version=get(2); p.available=get(3);
        p.source = cols.size()>=5? get(4):L"winget";
        if(p.source.empty()) p.source=L"winget";
        if(p.id.empty()||p.available.empty()) continue;
        if(p.id==L"Id") continue;
        res.push_back(p);
    }
    return res;
}

//  winget   
std::wstring RunWinget(const std::wstring& args) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), NULL, TRUE};
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW; si.hStdOutput=hWrite; si.hStdError=hWrite; si.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"winget " + args;
    // CreateProcessW   buffer   
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    if(!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)){
        CloseHandle(hRead); CloseHandle(hWrite); return L"";
    }
    CloseHandle(hWrite);
    std::string out;
    char tmp2[4096]; DWORD n;
    while(ReadFile(hRead, tmp2, sizeof(tmp2), &n, nullptr) && n) out.append(tmp2,n);
    WaitForSingleObject(pi.hProcess, 60000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hRead);
    // UTF8 -> wstring (winget  UTF8  BOM)
    //  BOM utf8
    if(out.size()>=3 && (unsigned char)out[0]==0xEF && (unsigned char)out[1]==0xBB && (unsigned char)out[2]==0xBF) out=out.substr(3);
    int wlen = MultiByteToWideChar(CP_UTF8,0,out.c_str(),(int)out.size(),nullptr,0);
    std::wstring w(wlen,0); MultiByteToWideChar(CP_UTF8,0,out.c_str(),(int)out.size(),&w[0],wlen);
    return w;
}

// ListView helpers
void UpdateCount(){
    int total = (int)g_packages.size();
    int checked=0;
    for(int i=0;i<ListView_GetItemCount(hList);++i) if(ListView_GetCheckState(hList,i)) checked++;
    int visible=0;
    for(int i=0;i<ListView_GetItemCount(hList);++i){
        //  search:  hidden  visible
        //     /     LVN
    }
    wchar_t buf[128];
    if(total==0) buf[0] = 0; // empty, avoid "-" ghost
    else swprintf_s(buf, L"%d selected", checked);
    SetWindowTextW(hCount, buf);
    InvalidateRect(hCount, nullptr, TRUE);
}
void PopulateList(const std::vector<Package>& pkgs){
    ListView_DeleteAllItems(hList);
    g_packages = pkgs;
    g_hotRow = -1;
    g_rowBtnRow = -1; g_rowBtnV = 0; g_rowBtnTarget = 0;
    g_pendingRows = 0;
    FitLastColumn();
    UpdateCount();
    KillTimer(hMain, 3);
    if (!pkgs.empty()) SetTimer(hMain, 3, 18, nullptr); // cascade rows in (Store-like entrance)
}
void SetLoading(bool b){
    g_loading=b;
    ShowWindow(hProgress, b?SW_SHOW:SW_HIDE);
    EnableWindow(hRefresh,!b);
    EnableWindow(hUpgradeAll,!b && !g_packages.empty());
    EnableWindow(hUpgradeSel,!b && !g_packages.empty());
    if(b) SetWindowTextW(hStatus,L"Checking for updates...");
    else {
        if(g_packages.empty()) SetWindowTextW(hStatus,L"All apps are up to date");
        else { wchar_t t[64]; swprintf_s(t,L"%d updates available",(int)g_packages.size()); SetWindowTextW(hStatus,t); }
    }
    if(b){ SendMessage(hProgress,PBM_SETMARQUEE,1,30); SetTimer(hMain,2,30,nullptr); KillTimer(hMain,5); }
    else { SendMessage(hProgress,PBM_SETMARQUEE,0,0); KillTimer(hMain,2); }
}

// Threads
void DoCheck(){
    PostMessage(hMain, WM_APP+3, 0, 0); // loading UI on UI thread (never touch HWND from worker)
    std::wstring ver = RunWinget(L"--version");
    Trim(ver);
    std::wstring out = RunWinget(L"upgrade --accept-source-agreements");
    auto pkgs = ParseWingetOutput(out);
    // UI thread
    PostMessage(hMain, WM_APP+1, (WPARAM)new std::vector<Package>(pkgs), 0);
    if (ver.empty())
        PostMessage(hMain, WM_APP+7, 0, (LPARAM)new std::wstring(L"winget not found — install App Installer from the Microsoft Store."));
}
// ---- Elevated upgrade with live progress ----
// winget needs admin for most upgrades: launch elevated via ShellExecuteEx "runas"
// writing to a temp log, then tail that log for live stage/percent updates.
// Unpredictable per-run log name: a fixed %TEMP% name would let another local
// user pre-plant a symlink and redirect the ELEVATED winget output anywhere.
static std::wstring LogPathFor() {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    ULONGLONG r = ((ULONGLONG)GetCurrentProcessId() << 32) ^ GetTickCount64() ^ ((ULONGLONG)rand() << 48);
    static volatile LONG ctr = 0;
    r ^= (ULONGLONG)InterlockedIncrement(&ctr) * 0x9E3779B97F4A7C15ULL;
    wchar_t rnd[32];
    swprintf_s(rnd, L"%016llX", r);
    return std::wstring(tmp) + L"veyo-updater-" + rnd + L".log";
}

// Strict allowlist for winget package ids: the id reaches an elevated
// `cmd /c winget ...` line, so anything outside [A-Za-z0-9._-] is refused.
static bool IsValidPackageId(const std::wstring& id) {
    if (id.empty() || id.size() > 128) return false;
    for (auto c : id) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'.' || c == L'_' || c == L'-')) return false;
    }
    return true;
}

static bool LaunchElevated(const std::wstring& wingetArgs, const std::wstring& logPath, HANDLE& hProc) {
    hProc = nullptr;
    std::wstring params = L"/c winget " + wingetArgs + L" > \"" + logPath + L"\" 2>&1";
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.hwnd = hMain;
    sei.lpVerb = L"runas"; // UAC prompt: always run updates as admin
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return false;
    hProc = sei.hProcess;
    return hProc != nullptr;
}

static std::wstring Utf8ToW(const char* s, size_t n) {
    if (!s || !n) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, (int)n, nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring w(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, (int)n, &w[0], wlen);
    return w;
}

// Tail the elevated winget log, posting each new line to the UI thread (WM_APP+4).
// True when winget exited 0 or reported success / nothing to do.
static bool TailLogFile(HANDLE hProc, const std::wstring& logPath) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 100 && hFile == INVALID_HANDLE_VALUE; ++i) {
        Sleep(100);
        hFile = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    ULONGLONG off = 0;
    bool firstRead = true;
    std::string pending;
    bool sawSuccess = false, sawNoUpgrade = false;
    auto pump = [&]() {
        if (hFile == INVALID_HANDLE_VALUE) return;
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart < 0) return;
        if ((ULONGLONG)sz.QuadPart <= off) return;
        DWORD toRead = (DWORD)((ULONGLONG)sz.QuadPart - off);
        std::string buf(toRead, '\0');
        OVERLAPPED ov{}; ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32);
        DWORD rd = 0;
        if (!ReadFile(hFile, &buf[0], toRead, &rd, &ov) || !rd) return;
        size_t start = 0;
        if (firstRead && rd >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) start = 3;
        firstRead = false;
        off += rd;
        pending.append(buf.data() + start, rd - start);
        size_t p;
        while ((p = pending.find('\n')) != std::string::npos) {
            std::string chunk = pending.substr(0, p);
            pending.erase(0, p + 1);
            // winget rewrites progress with \r: use last segment
            size_t seg = chunk.rfind('\r');
            std::string segStr = (seg == std::string::npos) ? chunk : chunk.substr(seg + 1);
            size_t ns = segStr.find_first_not_of(" \t\r");
            if (ns == std::string::npos) continue;
            segStr = segStr.substr(ns);
            while (!segStr.empty() && (segStr.back() == '\r' || segStr.back() == ' ' || segStr.back() == '\t')) segStr.pop_back();
            if (segStr.empty()) continue;
            std::wstring w = Utf8ToW(segStr.data(), segStr.size());
            Trim(w);
            if (w.empty()) continue;
            std::wstring low = w;
            std::transform(low.begin(), low.end(), low.begin(), ::towlower);
            if (low.find(L"successfully installed") != std::wstring::npos) sawSuccess = true;
            if (low.find(L"no available upgrade") != std::wstring::npos) sawNoUpgrade = true;
            PostMessage(hMain, WM_APP + 4, 0, (LPARAM)new std::wstring(w));
        }
    };
    bool exited = false;
    while (!exited) {
        if (WaitForSingleObject(hProc, 250) == WAIT_OBJECT_0) exited = true;
        pump();
    }
    pump();
    if (!pending.empty()) {
        std::wstring w = Utf8ToW(pending.data(), pending.size());
        Trim(w);
        if (!w.empty()) PostMessage(hMain, WM_APP + 4, 0, (LPARAM)new std::wstring(w));
    }
    DWORD code = 1;
    GetExitCodeProcess(hProc, &code);
    CloseHandle(hProc);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    return code == 0 || sawSuccess || sawNoUpgrade;
}

static bool DoOneUpgrade(const std::wstring& id, const std::wstring& name, std::wstring& err) {
    if (id != L"ALL" && !IsValidPackageId(id)) {
        err = L"Refused to upgrade \"" + name + L"\": unexpected package id.";
        return false;
    }
    std::wstring log = LogPathFor();
    DeleteFileW(log.c_str());
    std::wstring args;
    if (id == L"ALL") args = L"upgrade --all --silent --accept-package-agreements --accept-source-agreements --disable-interactivity";
    else args = L"upgrade --id \"" + id + L"\" -e --silent --accept-package-agreements --accept-source-agreements --disable-interactivity";
    HANDLE hProc = nullptr;
    if (!LaunchElevated(args, log, hProc)) {
        DWORD e = GetLastError();
        err = (e == ERROR_CANCELLED)
            ? L"Cancelled: administrator approval is required to update \"" + name + L"\"."
            : L"Could not start elevated installer for \"" + name + L"\".";
        return false;
    }
    bool ok = TailLogFile(hProc, log);
    if (ok) DeleteFileW(log.c_str());
    else err = L"Update of \"" + name + L"\" failed. Log: " + log;
    return ok;
}

static void UpgradeSelectedThread(std::vector<JobItem> items) {
    int okN = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        JobInfo* j = new JobInfo(); j->name = items[i].name; j->idx = (int)i + 1; j->total = (int)items.size();
        PostMessage(hMain, WM_APP + 6, 0, (LPARAM)j);
        std::wstring err;
        if (DoOneUpgrade(items[i].id, items[i].name, err)) ++okN;
        else PostMessage(hMain, WM_APP + 4, 0, (LPARAM)new std::wstring(err));
        Sleep(400);
    }
    wchar_t sum[256];
    swprintf_s(sum, L"Updated %d of %d apps.", okN, (int)items.size());
    PostMessage(hMain, WM_APP + 2, (okN == (int)items.size()) ? 1 : 0, (LPARAM)new std::wstring(sum));
}

static void UpgradeAllThread() {
    JobInfo* j = new JobInfo(); j->name = L"all apps"; j->idx = 1; j->total = 1;
    PostMessage(hMain, WM_APP + 6, 0, (LPARAM)j);
    std::wstring err;
    bool ok = DoOneUpgrade(L"ALL", L"all apps", err);
    PostMessage(hMain, WM_APP + 2, ok ? 1 : 0,
        (LPARAM)new std::wstring(ok ? L"All apps updated." : (L"Update finished with errors. " + err)));
}

// Parse one elevated-winget log line into stage text + percent (UI thread only).
static void OnProgressLine(const std::wstring& raw) {
    std::wstring line = raw;
    Trim(line);
    if (line.empty()) return;
    std::wstring low = line;
    std::transform(low.begin(), low.end(), low.begin(), ::towlower);
    // current app detection for --all upgrades
    for (auto& p : g_packages) {
        if (p.id.empty()) continue;
        std::wstring lid = p.id;
        std::transform(lid.begin(), lid.end(), lid.begin(), ::towlower);
        if (low.find(lid) != std::wstring::npos) { g_jobName = p.name; break; }
    }
    bool done = low.find(L"successfully installed") != std::wstring::npos;
    int pct = -1;
    try {
        static const std::wregex prog(LR"((\d+(?:\.\d+)?)\s*(KB|MB|GB)\s*/\s*(\d+(?:\.\d+)?)\s*(KB|MB|GB))", std::regex_constants::icase);
        std::wsmatch m;
        if (std::regex_search(line, m, prog)) {
            double a = std::stod(m[1].str()), b = std::stod(m[3].str());
            std::wstring u1 = m[2].str(), u2 = m[4].str();
            std::transform(u1.begin(), u1.end(), u1.begin(), ::towupper);
            std::transform(u2.begin(), u2.end(), u2.begin(), ::towupper);
            auto mult = [](const std::wstring& u) -> double {
                if (u == L"GB") return 1024.0 * 1024.0;
                if (u == L"MB") return 1024.0;
                return 1.0;
            };
            double kb1 = a * mult(u1), kb2 = b * mult(u2);
            if (kb2 > 0) pct = (int)(kb1 / kb2 * 100.0 + 0.5);
            if (pct < 0) pct = 0; if (pct > 100) pct = 100;
        }
    } catch (...) {}
    if (done) { pct = 100; g_stage = L"Done"; }
    else if (pct >= 0) g_stage = L"Downloading...";
    else if (low.find(L"downloading") != std::wstring::npos) g_stage = L"Downloading...";
    else if (low.find(L"installing") != std::wstring::npos) g_stage = L"Installing...";
    else if (low.find(L"found") != std::wstring::npos && low.find(L"package") != std::wstring::npos) g_stage = L"Preparing...";
    if (pct >= 0) {
        g_lastPct = pct;
        SendMessage(hProgress, PBM_SETMARQUEE, FALSE, 0);
        SendMessage(hProgress, PBM_SETRANGE32, 0, 100);
        int cur = (int)SendMessage(hProgress, PBM_GETPOS, 0, 0);
        if (pct >= cur) SendMessage(hProgress, PBM_DELTAPOS, pct - cur, 0); // smooth animated step
        else SendMessage(hProgress, PBM_SETPOS, pct, 0);
    }
    wchar_t st[512];
    swprintf_s(st, L"Updating %s - %s", g_jobName.c_str(), g_stage.c_str());
    SetWindowTextW(hStatus, st);
    wchar_t ct[128];
    if (g_lastPct >= 0) swprintf_s(ct, L"%d%% (%d/%d)", g_lastPct, g_jobIdx, g_jobTotal);
    else swprintf_s(ct, L"(%d/%d)", g_jobIdx, g_jobTotal);
    SetWindowTextW(hCount, ct);
}

// ListView subclass: its header notifies the ListView (not us), so paint dark header here
static void StartAnimTimer() { if (hMain) SetTimer(hMain, 4, 20, nullptr); } // 50fps fade tick

static COLORREF BlendC(COLORREF a, COLORREF b, float t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    return RGB((BYTE)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (BYTE)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (BYTE)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

// Command-button hover/press fade driver
static LRESULT CALLBACK BtnSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR) {
    int idx = (int)id - 10;
    if (idx >= 0 && idx < 4) {
        if (m == WM_MOUSEMOVE) {
            if (IsWindowEnabled(h) && g_btns[idx].target < 1.0f) { g_btns[idx].target = 1.0f; StartAnimTimer(); }
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        } else if (m == WM_MOUSELEAVE) {
            if (g_btns[idx].target != 0.0f) { g_btns[idx].target = 0.0f; StartAnimTimer(); }
        } else if (m == WM_LBUTTONDOWN) {
            if (IsWindowEnabled(h)) { g_btns[idx].target = 2.0f; StartAnimTimer(); }
        } else if (m == WM_LBUTTONUP) {
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            RECT rc; GetClientRect(h, &rc);
            g_btns[idx].target = (IsWindowEnabled(h) && PtInRect(&rc, pt)) ? 1.0f : 0.0f;
            StartAnimTimer();
        }
    }
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK ListSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_NOTIFY) {
        LPNMHDR nm = (LPNMHDR)l;
        if (nm->hwndFrom == ListView_GetHeader(h) && nm->code == NM_CUSTOMDRAW) {
            LPNMCUSTOMDRAW cd = (LPNMCUSTOMDRAW)l;
            if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
                HDC hdc = cd->hdc; RECT rc = cd->rc;
                HBRUSH br = CreateSolidBrush(g_dark ? RGB(44,44,44) : RGB(250,250,250));
                FillRect(hdc,&rc,br); DeleteObject(br);
                HPEN pen = CreatePen(PS_SOLID,1,g_dark ? RGB(30,30,30) : RGB(225,225,225));
                HPEN op = (HPEN)SelectObject(hdc,pen);
                MoveToEx(hdc,rc.left,rc.bottom-1,nullptr); LineTo(hdc,rc.right,rc.bottom-1);
                MoveToEx(hdc,rc.right-1,rc.top+5,nullptr); LineTo(hdc,rc.right-1,rc.bottom-5);
                SelectObject(hdc,op); DeleteObject(pen);
                wchar_t txt[128]={0};
                HDITEMW hi{}; hi.mask=HDI_TEXT; hi.pszText=txt; hi.cchTextMax=128;
                Header_GetItem((HWND)nm->hwndFrom,(int)cd->dwItemSpec,&hi);
                std::wstring label = txt;
                if((int)cd->dwItemSpec==g_sortCol) label += g_sortAsc ? L"  \u25B2" : L"  \u25BC";
                SetBkMode(hdc,TRANSPARENT);
                SetTextColor(hdc,g_dark ? RGB(255,255,255) : RGB(60,60,60));
                SelectObject(hdc,hFontNormal);
                RECT tr=rc; tr.left+=10; tr.right-=4;
                DrawTextW(hdc,label.c_str(),-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                return CDRF_SKIPDEFAULT;
            }
            return CDRF_DODEFAULT;
        }
    }
    if (m == WM_MOUSEMOVE) {
        LVHITTESTINFO ht{}; ht.pt.x = GET_X_LPARAM(l); ht.pt.y = GET_Y_LPARAM(l);
        int r = ListView_HitTest(h, &ht);
        int newHot = -1;
        if (r != -1 && (ht.flags & LVHT_ONITEM)) {
            LVHITTESTINFO sh = ht;
            ListView_SubItemHitTest(h, &sh);
            if (sh.iSubItem == 2) newHot = r;
        }
        if (newHot != g_hotRow) {
            int old = g_hotRow; g_hotRow = newHot;
            if (old != -1 && old != g_rowBtnRow) ListView_RedrawItems(h, old, old);
            if (newHot != -1) {
                if (g_rowBtnRow != -1 && g_rowBtnRow != newHot) ListView_RedrawItems(h, g_rowBtnRow, g_rowBtnRow);
                g_rowBtnRow = newHot; g_rowBtnV = 0.0f; g_rowBtnTarget = 1.0f;
                ListView_RedrawItems(h, newHot, newHot);
                StartAnimTimer();
            } else if (g_rowBtnRow != -1) {
                g_rowBtnTarget = 0.0f; // fade out
                StartAnimTimer();
            }
        }
        SetCursor(LoadCursorW(nullptr, newHot != -1 ? IDC_HAND : IDC_ARROW));
        if (newHot != -1) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return DefSubclassProc(h, m, w, l);
    }
    if (m == WM_MOUSELEAVE) {
        g_hotRow = -1;
        if (g_rowBtnRow != -1) { g_rowBtnTarget = 0.0f; StartAnimTimer(); }
        return DefSubclassProc(h, m, w, l);
    }
    return DefSubclassProc(h, m, w, l);
}

// Header subclass: overpaint the empty corner above the scrollbar (stays light otherwise)
static LRESULT CALLBACK HeaderBgSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_PAINT) {
        LRESULT r = DefSubclassProc(h, m, w, l);
        HDC hdc = GetDC(h);
        RECT crc; GetClientRect(h, &crc);
        int n = Header_GetItemCount(h);
        if (n > 0) {
            RECT lr; Header_GetItemRect(h, n - 1, &lr);
            if (lr.right < crc.right) {
                RECT corner{lr.right, 0, crc.right, crc.bottom};
                HBRUSH br = CreateSolidBrush(g_dark ? RGB(44,44,44) : RGB(250,250,250));
                FillRect(hdc, &corner, br); DeleteObject(br);
                HPEN pen = CreatePen(PS_SOLID, 1, g_dark ? RGB(30,30,30) : RGB(225,225,225));
                HPEN op = (HPEN)SelectObject(hdc, pen);
                MoveToEx(hdc, corner.left, corner.bottom - 1, nullptr);
                LineTo(hdc, corner.right, corner.bottom - 1);
                SelectObject(hdc, op); DeleteObject(pen);
            }
        }
        ReleaseDC(h, hdc);
        return r;
    }
    return DefSubclassProc(h, m, w, l);
}

static void FitLastColumn() {
    if (!hList) return;
    HWND hHdr = ListView_GetHeader(hList);
    if (!hHdr || Header_GetItemCount(hHdr) < 3) return;
    const int chkW = 40, btnW = 128;
    ListView_SetColumnWidth(hList, 0, chkW);
    ListView_SetColumnWidth(hList, 2, btnW);
    // loop until stable: column widths toggle scrollbars which change client width
    for (int k = 0; k < 3; ++k) {
        UpdateWindow(hList);
        RECT lrc; GetClientRect(hList, &lrc);
        int sbW = (GetWindowLongW(hList, GWL_STYLE) & WS_VSCROLL) ? GetSystemMetrics(SM_CXVSCROLL) : 0;
        int appW = (lrc.right - lrc.left) - chkW - btnW - sbW - 4;
        if (appW < 200) appW = 200;
        if (ListView_GetColumnWidth(hList, 1) == appW) break;
        ListView_SetColumnWidth(hList, 1, appW);
    }
}

// ---- Portable settings (VeyoUpdater.ini next to the exe) ----
struct AppSettings {
    bool autoUpdate = false;  // start updating automatically after countdown
    int countdownSec = 60;    // download timer, seconds
    bool autoCheck = false;   // check for updates periodically
    int checkMinutes = 60;    // auto-check interval, minutes
    int finishAction = 0;     // 0 nothing, 1 close app, 2 shut down PC
};
static AppSettings g_cfg;
static int g_countdown = 0;
static bool g_justUpgraded = false;

static std::wstring IniPath() {
    wchar_t p[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s;
    if (n == 0 || n >= MAX_PATH) {
        wchar_t t[MAX_PATH]; GetTempPathW(MAX_PATH, t);
        s = t; // absurdly long exe path fallback: per-user temp dir
    } else {
        s = std::wstring(p, n);
    }
    size_t d = s.find_last_of(L"\\/");
    return (d == std::wstring::npos ? s : s.substr(0, d + 1)) + L"VeyoUpdater.ini";
}
static void LoadSettings() {
    std::wstring ini = IniPath();
    g_cfg.autoUpdate = GetPrivateProfileIntW(L"Timers", L"AutoUpdate", 0, ini.c_str()) != 0;
    g_cfg.countdownSec = (int)GetPrivateProfileIntW(L"Timers", L"CountdownSec", 60, ini.c_str());
    g_cfg.autoCheck = GetPrivateProfileIntW(L"Timers", L"AutoCheck", 0, ini.c_str()) != 0;
    g_cfg.checkMinutes = (int)GetPrivateProfileIntW(L"Timers", L"CheckMinutes", 60, ini.c_str());
    g_cfg.finishAction = (int)GetPrivateProfileIntW(L"General", L"FinishAction", 0, ini.c_str());
    if (g_cfg.countdownSec < 5) g_cfg.countdownSec = 5; if (g_cfg.countdownSec > 3600) g_cfg.countdownSec = 3600;
    if (g_cfg.checkMinutes < 1) g_cfg.checkMinutes = 1; if (g_cfg.checkMinutes > 1440) g_cfg.checkMinutes = 1440;
    if (g_cfg.finishAction < 0 || g_cfg.finishAction > 2) g_cfg.finishAction = 0;
}
static void SaveSettings() {
    std::wstring ini = IniPath();
    wchar_t b[32];
    WritePrivateProfileStringW(L"Timers", L"AutoUpdate", g_cfg.autoUpdate ? L"1" : L"0", ini.c_str());
    swprintf_s(b, L"%d", g_cfg.countdownSec);
    WritePrivateProfileStringW(L"Timers", L"CountdownSec", b, ini.c_str());
    WritePrivateProfileStringW(L"Timers", L"AutoCheck", g_cfg.autoCheck ? L"1" : L"0", ini.c_str());
    swprintf_s(b, L"%d", g_cfg.checkMinutes);
    WritePrivateProfileStringW(L"Timers", L"CheckMinutes", b, ini.c_str());
    swprintf_s(b, L"%d", g_cfg.finishAction);
    WritePrivateProfileStringW(L"General", L"FinishAction", b, ini.c_str());
}
static void ApplyCheckTimer() {
    if (!hMain) return;
    KillTimer(hMain, 6);
    if (g_cfg.autoCheck && g_cfg.checkMinutes > 0)
        SetTimer(hMain, 6, (UINT)g_cfg.checkMinutes * 60000, nullptr);
}

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT m, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (m) {
    case WM_INITDIALOG: {
        CheckDlgButton(hDlg, IDC_CHK_AUTOUPDATE, g_cfg.autoUpdate ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg, IDC_EDT_COUNTDOWN, (UINT)g_cfg.countdownSec, FALSE);
        CheckDlgButton(hDlg, IDC_CHK_AUTOCHECK, g_cfg.autoCheck ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg, IDC_EDT_INTERVAL, (UINT)g_cfg.checkMinutes, FALSE);
        SendDlgItemMessageW(hDlg, IDC_CMB_FINISH, CB_ADDSTRING, 0, (LPARAM)L"Do nothing");
        SendDlgItemMessageW(hDlg, IDC_CMB_FINISH, CB_ADDSTRING, 0, (LPARAM)L"Close the app");
        SendDlgItemMessageW(hDlg, IDC_CMB_FINISH, CB_ADDSTRING, 0, (LPARAM)L"Shut down the computer");
        SendDlgItemMessageW(hDlg, IDC_CMB_FINISH, CB_SETCURSEL, (WPARAM)g_cfg.finishAction, 0);
        EnableWindow(GetDlgItem(hDlg, IDC_EDT_COUNTDOWN), g_cfg.autoUpdate);
        EnableWindow(GetDlgItem(hDlg, IDC_EDT_INTERVAL), g_cfg.autoCheck);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam), code = HIWORD(wParam);
        if ((id == IDC_CHK_AUTOUPDATE || id == IDC_CHK_AUTOCHECK) && code == BN_CLICKED) {
            bool on = IsDlgButtonChecked(hDlg, id) == BST_CHECKED;
            EnableWindow(GetDlgItem(hDlg, id == IDC_CHK_AUTOUPDATE ? IDC_EDT_COUNTDOWN : IDC_EDT_INTERVAL), on);
            return TRUE;
        }
        if (id == IDOK) {
            BOOL ok1 = FALSE, ok2 = FALSE;
            UINT cd = GetDlgItemInt(hDlg, IDC_EDT_COUNTDOWN, &ok1, FALSE);
            UINT iv = GetDlgItemInt(hDlg, IDC_EDT_INTERVAL, &ok2, FALSE);
            if (!ok1) cd = 60; if (!ok2) iv = 60;
            if (cd < 5) cd = 5; if (cd > 3600) cd = 3600;
            if (iv < 1) iv = 1; if (iv > 1440) iv = 1440;
            g_cfg.autoUpdate = IsDlgButtonChecked(hDlg, IDC_CHK_AUTOUPDATE) == BST_CHECKED;
            g_cfg.countdownSec = (int)cd;
            g_cfg.autoCheck = IsDlgButtonChecked(hDlg, IDC_CHK_AUTOCHECK) == BST_CHECKED;
            g_cfg.checkMinutes = (int)iv;
            int sel = (int)SendDlgItemMessageW(hDlg, IDC_CMB_FINISH, CB_GETCURSEL, 0, 0);
            g_cfg.finishAction = (sel < 0) ? 0 : (sel > 2 ? 2 : sel);
            SaveSettings();
            ApplyCheckTimer();
            KillTimer(hMain, 5);
            if (g_cfg.autoUpdate && !g_packages.empty() && !g_loading) {
                g_countdown = g_cfg.countdownSec;
                SetTimer(hMain, 5, 1000, nullptr);
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL) { EndDialog(hDlg, IDCANCEL); return TRUE; }
        break;
    }
    }
    return FALSE;
}

// WndProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
    case WM_CREATE:{
        // Windows 11 Fluent: follow system theme, Mica titlebar, round corners
        g_dark = DetectDarkMode();
        ApplyThemeColors();
        BOOL dark = g_dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        int corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        int backdrop = DWMSBT_MAINWINDOW; // Mica
        DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        // Fonts - Segoe UI Variable like Win11 Settings (Display Semibold for title)
        hFontNormal = CreateFontW(-15,0,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");
        if (!hFontNormal) hFontNormal = CreateFontW(-15,0,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        hFontBold = CreateFontW(-15,0,0,0,600,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");
        if (!hFontBold) hFontBold = CreateFontW(-15,0,0,0,600,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        hFontMono = CreateFontW(-13,0,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Cascadia Code");
        hFontTitle = CreateFontW(-26,0,0,0,600,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Display");
        if (!hFontTitle) hFontTitle = CreateFontW(-24,0,0,0,600,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        hFontSmall = CreateFontW(-16,0,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");
        if (!hFontSmall) hFontSmall = CreateFontW(-15,0,0,0,400,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        hBrBg = CreateSolidBrush(CLR_BG);
        hBrCard = CreateSolidBrush(CLR_CARD);
        hBrCard2 = CreateSolidBrush(CLR_CARD);
        hAppIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101)); // veyo.ico (must match app.rc)

        // Buttons - owner-draw Win11 style (4px radius, accent primary)
        hRefresh = CreateWindowW(L"BUTTON", L"Check for updates", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW, 0,0,0,0, hWnd, (HMENU)1002,nullptr,nullptr);
        hUpgradeSel = CreateWindowW(L"BUTTON", L"Update selected", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,0,0,hWnd,(HMENU)1003,nullptr,nullptr);
        hUpgradeAll = CreateWindowW(L"BUTTON", L"Update all", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,0,0,hWnd,(HMENU)1004,nullptr,nullptr);
        hSettings = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,0,0,hWnd,(HMENU)1005,nullptr,nullptr);
        g_btns[0].hw = hRefresh; g_btns[1].hw = hUpgradeSel; g_btns[2].hw = hUpgradeAll; g_btns[3].hw = hSettings;
        SetWindowSubclass(hRefresh, BtnSubclass, 10, 0);
        SetWindowSubclass(hUpgradeSel, BtnSubclass, 11, 0);
        SetWindowSubclass(hUpgradeAll, BtnSubclass, 12, 0);
        SetWindowSubclass(hSettings, BtnSubclass, 13, 0);

        // Progress - thin accent bar like Store (marquee)
        hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD|PBS_MARQUEE|PBS_SMOOTH,0,0,0,0,hWnd,nullptr,nullptr,nullptr);
        SendMessage(hProgress,PBM_SETMARQUEE,0,0);
        SendMessage(hProgress, PBM_SETBARCOLOR, 0, CLR_ACCENT);
        SendMessage(hProgress, PBM_SETBKCOLOR, 0, CLR_BG);
        SetWindowTheme(hProgress, L"Explorer", nullptr);

        // Status (InfoBar-like) + Count
        hStatus = CreateWindowW(L"STATIC", L"Ready", WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE,0,0,0,0,hWnd,nullptr,nullptr,nullptr);
        hCount = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_RIGHT|SS_CENTERIMAGE,0,0,0,0,hWnd,nullptr,nullptr,nullptr);
        SendMessage(hStatus,WM_SETFONT,(WPARAM)hFontNormal,TRUE);
        SendMessage(hCount,WM_SETFONT,(WPARAM)hFontNormal,TRUE);

        // ListView - File Explorer details style
        hList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,0,0,0,0,hWnd,(HMENU)1000,nullptr,nullptr);
        SendMessage(hList, WM_SETFONT,(WPARAM)hFontNormal,TRUE);
        // NOTE: no LVS_EX_DOUBLEBUFFER - its off-screen blit wipes custom-drawn cells
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES);
        ListView_SetBkColor(hList, CLR_CARD);
        ListView_SetTextBkColor(hList, CLR_CARD);
        ListView_SetTextColor(hList, CLR_TEXT);
        // DarkMode_Explorer = dark scrollbars like Win11 Explorer in dark mode
        SetWindowTheme(hList, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        SetWindowSubclass(hList, ListSubclass, 1, 0);
        SetWindowSubclass(ListView_GetHeader(hList), HeaderBgSubclass, 2, 0);
        // row height like Explorer (32px)
        ListView_SetItemCount(hList, 0);

        // Store-like rows: [check] App (icon + 2 lines) [Update button]
        g_hRowImg = ImageList_Create(1, 58, ILC_COLOR32, 0, 0);
        ListView_SetImageList(hList, g_hRowImg, LVSIL_SMALL);

        LVCOLUMNW col{};
        col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; col.fmt=LVCFMT_LEFT;
        col.pszText=(LPWSTR)L""; col.cx=40; ListView_InsertColumn(hList,0,&col);
        col.pszText=(LPWSTR)L"App"; col.cx=400; ListView_InsertColumn(hList,1,&col);
        col.pszText=(LPWSTR)L""; col.cx=128; ListView_InsertColumn(hList,2,&col);

        // Check winget version
        std::thread([]{
            auto v=RunWinget(L"--version");
            Trim(v);
            PostMessage(hMain, WM_APP+10, (WPARAM)new std::wstring(v),0);
        }).detach();

        // Auto check after 400ms
        SetTimer(hWnd,1,400,nullptr);
        // Explicit initial layout (AnimateWindow show path may not send WM_SIZE)
        RECT crc; GetClientRect(hWnd, &crc);
          SendMessage(hWnd, WM_SIZE, (WPARAM)SIZE_RESTORED, MAKELPARAM(crc.right - crc.left, crc.bottom - crc.top));
          LoadSettings();
          if (g_cfg.autoCheck) SetTimer(hWnd, 6, (UINT)g_cfg.checkMinutes * 60000, nullptr);
          return 0;
    }
    case WM_TIMER:
        if(wParam==1){ KillTimer(hWnd,1); if(!g_loading) std::thread(DoCheck).detach(); }
        else if(wParam==2){ // loading spinner sweep around logo
            g_spinAngle = (g_spinAngle + 12) % 360;
            RECT lr{24 - 4, 6, 24 + 48, 60};
            InvalidateRect(hWnd, &lr, FALSE);
        }
        else if(wParam==3){ // staggered row entrance (2 rows per tick)
            int n = (int)g_packages.size();
            for (int k = 0; k < 2 && g_pendingRows < n; ++k) {
                int i = g_pendingRows++;
                LVITEMW it{}; it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=ListView_GetItemCount(hList); it.lParam=i;
                it.pszText=(LPWSTR)L"";
                int idx=ListView_InsertItem(hList,&it);
                if (idx >= 0) {
                    ListView_SetItemText(hList,idx,1,(LPWSTR)g_packages[i].name.c_str());
                    ListView_SetItemText(hList,idx,2,(LPWSTR)L"");
                    ListView_SetCheckState(hList,idx,TRUE);
                }
            }
            UpdateCount();
            if (g_pendingRows >= n) { KillTimer(hWnd, 3); FitLastColumn(); }
        }
        else if(wParam==4){ // button fade animation tick (50fps)
            bool active = false;
            for (auto& ba : g_btns) {
                if (ba.v != ba.target) {
                    float d = ba.target - ba.v;
                    float step = (d > 0 ? 1 : -1) * 0.25f;
                    if (fabsf(d) <= 0.25f) ba.v = ba.target; else ba.v += step;
                    if (IsWindow(ba.hw)) InvalidateRect(ba.hw, nullptr, FALSE);
                    active = true;
                }
            }
            if (g_rowBtnRow != -1 && g_rowBtnV != g_rowBtnTarget) {
                float d = g_rowBtnTarget - g_rowBtnV;
                float step = (d > 0 ? 1 : -1) * 0.3f;
                if (fabsf(d) <= 0.3f) g_rowBtnV = g_rowBtnTarget; else g_rowBtnV += step;
                ListView_RedrawItems(hList, g_rowBtnRow, g_rowBtnRow);
                active = true;
                if (g_rowBtnV == 0.0f && g_rowBtnTarget == 0.0f) g_rowBtnRow = -1;
            }
            if (!active) KillTimer(hWnd, 4);
        }
        else if(wParam==5){ // auto-update countdown tick (1s)
            if (g_loading || g_packages.empty()) { KillTimer(hWnd, 5); }
            else if (--g_countdown <= 0) {
                KillTimer(hWnd, 5);
                std::thread(UpgradeAllThread).detach();
            } else {
                wchar_t t[128];
                swprintf_s(t, L"Auto-update in %ds...", g_countdown);
                SetWindowTextW(hStatus, t);
            }
        }
        else if(wParam==6){ // auto-check interval
            if(!g_loading) std::thread(DoCheck).detach();
        }
        return 0;
    case WM_GETMINMAXINFO:{
        LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
        mmi->ptMinTrackSize.x = 720;
        mmi->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_SIZE:{
        if (wParam == SIZE_MINIMIZED) return 0; // keep layout when minimized
        if (LOWORD(lParam) < 10 || HIWORD(lParam) < 10) return 0;
        int W=LOWORD(lParam), H=HIWORD(lParam);
        int pad = 24; // page margin
        int titleH = 62;    // compact single-line header (logo + title)
        int cmdH = 32;      // Win11 control height
        int cmdY = titleH + 2;
        int gap = 8;
        const int btnW1 = 150;  // Check for updates
        const int btnW2 = 135;  // Update selected
        const int btnW3 = 120;  // Update all (accent)
        const int btnW4 = 110;  // Settings (pinned right)
        int x = pad;
        MoveWindow(hRefresh, x, cmdY, btnW1, cmdH, TRUE);
        x += btnW1 + gap;
        MoveWindow(hUpgradeSel, x, cmdY, btnW2, cmdH, TRUE);
        x += btnW2 + gap;
        MoveWindow(hUpgradeAll, x, cmdY, btnW3, cmdH, TRUE);
        MoveWindow(hSettings, W - pad - btnW4, cmdY, btnW4, cmdH, TRUE);
        // thin progress under command bar
        MoveWindow(hProgress, pad, cmdY + cmdH + 6, W - pad*2, 4, TRUE);
        // Slim bottom status bar like real Windows apps (left status + right count)
        int statusH = 24;
        int statusY = H - 12 - statusH;
        int infoW = W - pad * 2;
        const int countW = 180;
        MoveWindow(hStatus, pad, statusY, infoW - countW - 8, statusH, TRUE);
        MoveWindow(hCount, pad + infoW - countW, statusY, countW, statusH, TRUE);
        // List fills everything between progress bar and status bar
        int listY = cmdY + cmdH + 6 + 4 + 8;
        MoveWindow(hList, pad, listY, W - pad*2, statusY - 6 - listY, TRUE);
        FitLastColumn();
        InvalidateRect(hWnd,nullptr,TRUE);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:{
        HDC hdc=(HDC)wParam; HWND hCtl=(HWND)lParam;
        if(hCtl==hStatus || hCtl==hCount){
            SetTextColor(hdc, CLR_TEXT);
            SetBkColor(hdc, CLR_BG); // plain text, no card
            return (LRESULT)hBrBg;
        }
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG);
        return (LRESULT)hBrBg;
    }
    case WM_NOTIFY:{
        LPNMHDR nm=(LPNMHDR)lParam;
        // Explorer-like rows: theme colors, selected = subtle accent tint
        if(nm->hwndFrom==hList && nm->code==NM_CUSTOMDRAW){
            LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;
            switch(cd->nmcd.dwDrawStage){
                case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:{
                    bool sel = (cd->nmcd.uItemState & CDIS_SELECTED);
                    bool focus = (GetFocus() == hList);
                    if(sel){
                        // Win11 selection: light blue tint (light) / gray tint (dark)
                        if (g_dark) { cd->clrText = RGB(255,255,255); cd->clrTextBk = RGB(60,60,60); }
                        else { cd->clrText = RGB(0,0,0); cd->clrTextBk = RGB(230,240,250); }
                    } else {
                        cd->clrText = CLR_TEXT;
                        cd->clrTextBk = CLR_CARD;
                    }
                    (void)focus;
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):{
                    LPNMLVCUSTOMDRAW scd = (LPNMLVCUSTOMDRAW)lParam;
                    int row = (int)scd->nmcd.dwItemSpec;
                    int col = scd->iSubItem;
                    int idx = (int)scd->nmcd.lItemlParam;
                    bool sel = (scd->nmcd.uItemState & CDIS_SELECTED);
                    COLORREF rowBg = sel ? (g_dark ? RGB(60,60,60) : RGB(230,240,250)) : CLR_CARD;
                    HDC hdc = scd->nmcd.hdc;
                    if (col == 1 && idx >= 0 && idx < (int)g_packages.size()) {
                        const Package& p = g_packages[idx];
                        // NOTE: nmcd.rc has zero height for subitems here - query it directly
                        RECT rc = scd->nmcd.rc;
                        if (rc.bottom <= rc.top)
                            ListView_GetSubItemRect(hList, row, col, LVIR_BOUNDS, &rc);
                        HBRUSH br = CreateSolidBrush(rowBg);
                        FillRect(hdc, &rc, br); DeleteObject(br);
                        int rh = rc.bottom - rc.top;
                        int iconS = rh - 20; if (iconS > 38) iconS = 38; if (iconS < 24) iconS = 24;
                        int ix = rc.left + 10, iy = rc.top + (rh - iconS) / 2;
                        HBRUSH ibr = CreateSolidBrush(g_dark ? RGB(70,70,70) : RGB(225,225,225));
                        HPEN ip = CreatePen(PS_SOLID, 1, g_dark ? RGB(100,100,100) : RGB(195,195,195));
                        HGDIOBJ oip = SelectObject(hdc, ip), oib = SelectObject(hdc, ibr);
                        RoundRect(hdc, ix, iy, ix + iconS, iy + iconS, 10, 10);
                        SelectObject(hdc, oip); SelectObject(hdc, oib);
                        DeleteObject(ip); DeleteObject(ibr);
                        wchar_t initial[2] = { p.name.empty() ? L'?' : (wchar_t)towupper(p.name[0]), 0 };
                        SetBkMode(hdc, TRANSPARENT);
                        SetTextColor(hdc, CLR_TEXT);
                        SelectObject(hdc, hFontBold);
                        RECT lr{ ix, iy, ix + iconS, iy + iconS };
                        DrawTextW(hdc, initial, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        int tx = ix + iconS + 12;
                        RECT r1{ tx, rc.top + 5, rc.right - 6, rc.top + rh / 2 + 3 };
                        RECT r2{ tx, rc.top + rh / 2, rc.right - 6, rc.bottom - 5 };
                        SelectObject(hdc, hFontBold);
                        SetTextColor(hdc, CLR_TEXT);
                        DrawTextW(hdc, p.name.c_str(), -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                        std::wstring info = p.id + L"  •  " + p.version + L" → " + p.available;
                        SelectObject(hdc, hFontNormal);
                        SetTextColor(hdc, CLR_SUB);
                        DrawTextW(hdc, info.c_str(), -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (col == 2) {
                        RECT rc = scd->nmcd.rc;
                        if (rc.bottom <= rc.top)
                            ListView_GetSubItemRect(hList, row, col, LVIR_BOUNDS, &rc);
                        HBRUSH br = CreateSolidBrush(rowBg);
                        FillRect(hdc, &rc, br); DeleteObject(br);
                        int rh = rc.bottom - rc.top;
                        int bh = 30; if (bh > rh - 10) bh = rh - 10;
                        RECT b{ rc.left + 8, rc.top + (rh - bh) / 2, rc.right - 10, rc.top + (rh - bh) / 2 + bh };
                        float rv = (row == g_rowBtnRow) ? g_rowBtnV : 0.0f;
                        COLORREF bg = BlendC(CLR_BTN, CLR_BTN_HOVER, rv);
                        COLORREF bd = BlendC(CLR_BTN_BORDER, RGB(130,130,130), rv);
                        HBRUSH bbr = CreateSolidBrush(bg);
                        HPEN bp = CreatePen(PS_SOLID, 1, bd);
                        HGDIOBJ obp = SelectObject(hdc, bp), obb = SelectObject(hdc, bbr);
                        RoundRect(hdc, b.left, b.top, b.right - 1, b.bottom - 1, 8, 8);
                        SelectObject(hdc, obp); SelectObject(hdc, obb);
                        DeleteObject(bp); DeleteObject(bbr);
                        SetBkMode(hdc, TRANSPARENT);
                        SetTextColor(hdc, CLR_TEXT);
                        SelectObject(hdc, hFontNormal);
                        DrawTextW(hdc, L"Update", -1, &b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        return CDRF_SKIPDEFAULT;
                    }
                    return CDRF_DODEFAULT;
                }
            }
            return CDRF_DODEFAULT;
        }
        if(nm->hwndFrom==hList && nm->code==NM_CLICK){
            LPNMITEMACTIVATE pa=(LPNMITEMACTIVATE)lParam;
            if(pa->iItem>=0 && pa->iSubItem==2 && !g_loading){
                LVITEMW vi{}; vi.mask=LVIF_PARAM; vi.iItem=pa->iItem;
                if(ListView_GetItem(hList,&vi) && vi.lParam>=0 && vi.lParam<(int)g_packages.size()){
                    const Package& p=g_packages[(int)vi.lParam];
                    wchar_t q[512];
                    swprintf_s(q, L"Update \"%s\"?\n\nAdministrator approval (UAC) will be requested.", p.name.c_str());
                    if(MessageBoxW(hMain,q,L"Confirm",MB_YESNO|MB_ICONQUESTION)==IDYES){
                        std::vector<JobItem> one; JobItem j; j.id=p.id; j.name=p.name; one.push_back(j);
                        std::thread(UpgradeSelectedThread, one).detach();
                    }
                }
            }
        }
        if(nm->hwndFrom==hList && nm->code==LVN_COLUMNCLICK){
            LPNMLISTVIEW p=(LPNMLISTVIEW)lParam;
            if(p->iSubItem!=1) return 0;
            if(g_sortCol==1) g_sortAsc=!g_sortAsc; else {g_sortCol=1; g_sortAsc=true;}
            // sort simple
            std::sort(g_packages.begin(), g_packages.end(), [](const Package& a, const Package& b){
                std::wstring av, bv;
                switch(g_sortCol){
                    case 0: av=a.name; bv=b.name; break;
                    case 1: av=a.id; bv=b.id; break;
                    case 2: av=a.version; bv=b.version; break;
                    case 3: av=a.available; bv=b.available; break;
                    default: av=a.name; bv=b.name;
                }
                std::transform(av.begin(),av.end(),av.begin(),::towlower);
                std::transform(bv.begin(),bv.end(),bv.begin(),::towlower);
                return g_sortAsc? av<bv : av>bv;
            });
            PopulateList(g_packages);
        }
        if(nm->hwndFrom==hList && nm->code==LVN_ITEMCHANGED){
            // update count on check
           UpdateCount();
        }
        return 0;
    }
    case WM_COMMAND:{
        int id=LOWORD(wParam), code=HIWORD(wParam);
        if(id==1005){ // settings
            DialogBoxW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, SettingsDlgProc);
        }
        if(id==1002){ // refresh
            if(!g_loading) std::thread(DoCheck).detach();
        }
        if(id==1004){ // upgrade all (always elevated)
            if(g_packages.empty()){ MessageBoxW(hWnd,L"No updates",L"Info",MB_OK); break; }
            wchar_t q[256];
            swprintf_s(q, L"Update all %d apps?\n\nAdministrator approval (UAC) will be requested.", (int)g_packages.size());
            if(MessageBoxW(hWnd,q,L"Confirm",MB_YESNO|MB_ICONQUESTION)==IDYES){
                std::thread(UpgradeAllThread).detach();
            }
        }
        if(id==1003){ // upgrade selected (always elevated)
            std::vector<JobItem> sel;
            for(int i=0;i<ListView_GetItemCount(hList);++i) if(ListView_GetCheckState(hList,i)){
                LVITEMW vi{}; vi.mask=LVIF_PARAM; vi.iItem=i;
                if(!ListView_GetItem(hList,&vi)) continue;
                int idx=(int)vi.lParam;
                if(idx<0||idx>=(int)g_packages.size()) continue;
                JobItem j; j.id=g_packages[idx].id; j.name=g_packages[idx].name; sel.push_back(j);
            }
            if(sel.empty()){ MessageBoxW(hWnd,L"No items selected",L"Info",MB_OK); break; }
            wchar_t q[256];
            swprintf_s(q, L"Update %d selected apps?\n\nAdministrator approval (UAC) will be requested.", (int)sel.size());
            if(MessageBoxW(hWnd,q,L"Confirm",MB_YESNO)==IDYES){
                if((int)sel.size()==(int)g_packages.size()){
                    std::thread(UpgradeAllThread).detach();
                } else {
                    std::thread(UpgradeSelectedThread, sel).detach();
                }
            }
        }
        if(code==0 && id==1000){
            // double click -> upgrade single
        }
        return 0;
    }
    case WM_APP+3:{ // loading started (posted by worker thread -> runs on UI thread)
        SetLoading(true);
        std::wstring *s = (std::wstring*)lParam;
        if (s) { SetWindowTextW(hStatus, s->c_str()); delete s; }
        return 0;
    }
    case WM_APP+6:{ // upgrade job started: show progress bar + job context
        JobInfo *j = (JobInfo*)lParam;
        g_jobName = j->name; g_jobIdx = j->idx; g_jobTotal = j->total;
        g_lastPct = -1; g_stage = L"Starting...";
        delete j;
        SetLoading(true);
        SendMessage(hProgress, PBM_SETMARQUEE, TRUE, 30);
        SendMessage(hProgress, PBM_SETPOS, 0, 0);
        wchar_t st[512];
        swprintf_s(st, L"Updating %s...", g_jobName.c_str());
        SetWindowTextW(hStatus, st);
        wchar_t ct[64];
        swprintf_s(ct, L"(%d/%d)", g_jobIdx, g_jobTotal);
        SetWindowTextW(hCount, ct);
        return 0;
    }
    case WM_APP+4:{ // live progress line from elevated winget log
        std::wstring *pl = (std::wstring*)lParam;
        OnProgressLine(*pl);
        delete pl;
        return 0;
    }
    case WM_APP+7:{ // fatal status (e.g. winget missing): overrides the status line
        std::wstring *s = (std::wstring*)lParam;
        SetLoading(false);
        SetWindowTextW(hStatus, s->c_str());
        delete s;
        return 0;
    }
    case WM_APP+1:{ // check finished
        auto *pkgs = (std::vector<Package>*)wParam;
        PopulateList(*pkgs);
        SetLoading(false);
        if(pkgs->empty()) SetWindowTextW(hStatus,L"All apps are up to date");
        delete pkgs;
        if (g_justUpgraded) { g_justUpgraded = false; }
        else if (g_cfg.autoUpdate && !g_packages.empty() && !g_loading) {
            g_countdown = g_cfg.countdownSec;
            SetTimer(hWnd, 5, 1000, nullptr);
            wchar_t t[128];
            swprintf_s(t, L"Auto-update in %ds...", g_countdown);
            SetWindowTextW(hStatus, t);
        }
        return 0;
    }
    case WM_APP+2:{ // upgrade finished
        bool ok = wParam!=0;
        auto *out = (std::wstring*)lParam;
        SetLoading(false);
        if(ok && g_cfg.finishAction == 2) {
            MessageBoxW(hWnd, L"All updates finished.\n\nThe computer will shut down in 60 seconds.\nRun \"shutdown /a\" to abort.", L"Veyo Updater", MB_OK | MB_ICONINFORMATION);
            ShellExecuteW(hWnd, L"open", L"shutdown.exe", L"/s /t 60 /c \"Veyo Updater finished all updates\"", nullptr, SW_HIDE);
        } else if(ok && g_cfg.finishAction == 1) {
            MessageBoxW(hWnd, L"All updates finished. The app will now close.", L"Veyo Updater", MB_OK | MB_ICONINFORMATION);
            PostMessage(hWnd, WM_CLOSE, 0, 0);
        } else if(ok) {
            SetWindowTextW(hStatus,L"Update finished. Refreshing...");
            MessageBoxW(hWnd, L"Update completed. Refreshing list.", L"Success", MB_OK);
            g_justUpgraded = true;
            std::thread(DoCheck).detach();
        } else {
            MessageBoxW(hWnd, out->c_str(), L"Result", MB_OK);
        }
        delete out;
        return 0;
    }
    case WM_APP+10:{
        auto *v=(std::wstring*)wParam;
        g_wingetVer = *v;
        if (g_wingetVer.size() > 24) g_wingetVer.resize(24);
        InvalidateRect(hWnd, nullptr, TRUE);
        delete v; return 0;
    }
    case WM_DRAWITEM:{
        LPDRAWITEMSTRUCT ds=(LPDRAWITEMSTRUCT)lParam;
        HWND hw = (HWND)ds->hwndItem;
        if(hw==hUpgradeAll || hw==hRefresh || hw==hUpgradeSel || hw==hSettings){
            bool isPrimary = (hw==hUpgradeAll);
            bool pressed = (ds->itemState & ODS_SELECTED);
            bool disabled = (ds->itemState & ODS_DISABLED);
            // animated hover/press value: 0 normal -> 1 hover -> 2 pressed
            float av = 0;
            for (auto& ba : g_btns) if (ba.hw == hw) { av = ba.v; break; }
            if (pressed) av = 2.0f; // keyboard press: snap
            COLORREF bg, border, txtClr;
            if (disabled) {
                bg = g_dark ? RGB(45,45,45) : RGB(245,245,245);
                border = g_dark ? RGB(50,50,50) : RGB(220,220,220);
                txtClr = RGB(150,150,150);
            } else if(isPrimary){
                bg = (av <= 1.0f) ? BlendC(CLR_ACCENT, CLR_ACCENT_HOVER, av)
                                  : BlendC(CLR_ACCENT_HOVER, CLR_ACCENT_PRESS, av - 1.0f);
                border = bg;
                txtClr = RGB(255,255,255);
            } else {
                bg = (av <= 1.0f) ? BlendC(CLR_BTN, CLR_BTN_HOVER, av)
                                  : BlendC(CLR_BTN_HOVER, CLR_BTN_PRESS, av - 1.0f);
                border = BlendC(CLR_BTN_BORDER, (g_dark ? RGB(120,120,120) : RGB(140,140,140)), av > 1.0f ? 1.0f : av);
                txtClr = CLR_TEXT;
            }
            HBRUSH br = CreateSolidBrush(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldPen = SelectObject(ds->hDC, pen);
            HGDIOBJ oldBr = SelectObject(ds->hDC, br);
            // Win11 radius = 4px
            RoundRect(ds->hDC, ds->rcItem.left, ds->rcItem.top, ds->rcItem.right-1, ds->rcItem.bottom-1, 8, 8);
            SelectObject(ds->hDC, oldPen);
            SelectObject(ds->hDC, oldBr);
            DeleteObject(pen);
            DeleteObject(br);
            SetBkMode(ds->hDC, TRANSPARENT);
            SetTextColor(ds->hDC, txtClr);
            SelectObject(ds->hDC, isPrimary ? hFontBold : hFontNormal);
            wchar_t txt[64]; GetWindowTextW(hw,txt,64);
            // focus rect (dotted) like Win32
            UINT fmt = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
            DrawTextW(ds->hDC, txt, -1, &ds->rcItem, fmt);
            if (ds->itemState & ODS_FOCUS) {
                RECT fr = ds->rcItem; InflateRect(&fr, -4, -4);
                DrawFocusRect(ds->hDC, &fr);
            }
            return TRUE;
        }
        return 0;
    }
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hWnd,&ps);
        RECT rc; GetClientRect(hWnd,&rc);
        SetBkMode(hdc, TRANSPARENT);
        // Page background (Mica shows in titlebar, client uses Settings bg)
        HBRUSH bg=CreateSolidBrush(CLR_BG);
        FillRect(hdc,&rc,bg); DeleteObject(bg);
        int pad = 24;
        // App logo (veyo.ico) left of title
        if (hAppIcon) {
            // Round (circular) logo like an avatar, with a subtle ring around it
            int sz = 42;
            HRGN rg = CreateEllipticRgn(pad, 10, pad + sz, 10 + sz);
            SelectClipRgn(hdc, rg);
            DrawIconEx(hdc, pad, 10, hAppIcon, sz, sz, 0, nullptr, DI_NORMAL);
            SelectClipRgn(hdc, nullptr);
            DeleteObject(rg);
            HPEN ring = CreatePen(PS_SOLID, 2, RGB(232, 17, 35)); // brand-red ring
            HGDIOBJ obr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            HGDIOBJ opr = SelectObject(hdc, ring);
            Ellipse(hdc, pad + 1, 10 + 1, pad + sz - 1, 10 + sz - 1);
            SelectObject(hdc, obr); SelectObject(hdc, opr);
            DeleteObject(ring);
            if (g_loading) { // sweeping red arc while busy
                HPEN sp = CreatePen(PS_SOLID, 3, RGB(232, 17, 35));
                HGDIOBJ osp = SelectObject(hdc, sp);
                double a0 = g_spinAngle * 3.14159265 / 180.0;
                double a1 = (g_spinAngle + 100) * 3.14159265 / 180.0;
                int cx = pad + sz / 2, cy = 10 + sz / 2, r = sz / 2 + 5;
                Arc(hdc, cx - r, cy - r, cx + r, cy + r,
                    (int)(cx + r * cos(a0)), (int)(cy - r * sin(a0)),
                    (int)(cx + r * cos(a1)), (int)(cy - r * sin(a1)));
                SelectObject(hdc, osp);
                DeleteObject(sp);
            }
        }
        // Compact single-line header: logo + title, version top-right
        RECT tR{pad + 52, 8, rc.right - pad - 180, 52};
        SelectObject(hdc,hFontTitle);
        SetTextColor(hdc, CLR_TEXT);
        DrawTextW(hdc,L"Veyo Updater",-1,&tR,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        std::wstring verTxt = L"v1.1";
        if (!g_wingetVer.empty()) verTxt += L"  •  winget " + g_wingetVer;
        RECT vR{rc.right - pad - 220, 8, rc.right - pad, 52};
        SelectObject(hdc,hFontSmall);
        SetTextColor(hdc, CLR_SUB);
        DrawTextW(hdc,verTxt.c_str(),-1,&vR,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        // No status card: status/count are plain text on background (no box at all)
        EndPaint(hWnd,&ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE:
        AnimateWindow(hWnd, 150, AW_BLEND | AW_HIDE); // fade-out on exit
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd,msg,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow){
    InitCommonControls();
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=hInst; wc.lpszClassName=L"VeyoUpdaterLight";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101)); // veyo.ico (must match app.rc)
    if(!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);

    // DPI aware via manifest (app.manifest)
    hMain = CreateWindowW(L"VeyoUpdaterLight", L"Veyo Updater",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT,CW_USEDEFAULT, 1040,680, nullptr,nullptr,hInst,nullptr);
       (void)nCmdShow;
    AnimateWindow(hMain, 220, AW_BLEND); // fade-in on launch
   UpdateWindow(hMain);

    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)){
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
