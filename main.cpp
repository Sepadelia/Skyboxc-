// Skybox — Créateur / Lanceur
// cl /EHsc /O2 /utf-8 main.cpp Skybox.res user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib ole32.lib /link /SUBSYSTEM:WINDOWS /OUT:Skybox.exe
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#pragma comment(lib,"uxtheme.lib")
#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"advapi32.lib")

#define WM_TRAY (WM_APP+10)
#define IDI_TRAY 1001

// ── Constants (mutable pour paramètres) ────────────────────────────────────────
static std::wstring CACHE_DIR = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\GarrysMod\\garrysmod\\cache";
static std::wstring SAVE_DIR  = L"C:\\skybox";
static const char*   FACES[6]    = {"ft","bk","lf","rt","up","dn"};
static const wchar_t* FLABELS[6] = {L"ft:",L"bk:",L"lf:",L"rt:",L"up:",L"dn:"};

// ── IDs ────────────────────────────────────────────────────────────────────────
enum {
    // Mode tabs
    IDC_TAB_CREATE=1, IDC_TAB_LAUNCH=2, IDC_TAB_PURGE=3,
    // Créateur
    IDC_NAME=10, IDC_RADIO_MAN=11, IDC_RADIO_DIR=12,
    IDC_DIR_EDIT=13, IDC_DIR_BROWSE=14,
    IDC_EDIT0=30,IDC_EDIT1,IDC_EDIT2,IDC_EDIT3,IDC_EDIT4,IDC_EDIT5,
    IDC_BROWSE0=40,IDC_BROWSE1,IDC_BROWSE2,IDC_BROWSE3,IDC_BROWSE4,IDC_BROWSE5,
    IDC_SAVE_BTN=50,
    IDC_GROUP_LIST=70, IDC_RENAME_EDIT=71,
    IDC_SETTINGS=90, IDC_OPEN_REPO=91,
    // Lanceur
    IDC_LISTBOX=60, IDC_REFRESH=61,
    IDC_WATCH=62, IDC_LOCK=63, IDC_PERM=64,
    // Purge
    IDC_PURGE_LIST=110, IDC_PURGE_REFRESH=111,
    IDC_PURGE_SKY=112, IDC_PURGE_PATH=113, IDC_PURGE_BROWSE=114,
    IDC_PURGE_APPLY=115,
    // Common
    IDC_STATUS=80,
};
#define WM_SETSTATUS (WM_APP+1)
#define WM_UPDLOCK   (WM_APP+2)

#define WM_SETREADY      (WM_APP+5)
#define WM_FIRST_LAUNCH  (WM_APP+6)

static bool g_start_hidden=false; // lancé depuis le démarrage Windows

// ── Preview globals ────────────────────────────────────────────────────────────
static HWND g_preview=nullptr;
static std::vector<uint8_t> g_prev_bgra[6];
static int g_prev_w[6]={}, g_prev_h[6]={};

// ── Globals ────────────────────────────────────────────────────────────────────
static HWND g_hwnd=nullptr, g_status=nullptr;
// Créateur controls
static HWND g_name=nullptr, g_name_lbl=nullptr, g_rad_man=nullptr, g_rad_dir=nullptr;
static HWND g_dir_edit=nullptr, g_dir_browse=nullptr, g_dir_lbl=nullptr;
static HWND g_edits[6]={}, g_browses[6]={}, g_man_labels[6]={};
static HWND g_group_list=nullptr, g_rename_edit=nullptr, g_rename_lbl=nullptr;
static HWND g_save_btn=nullptr, g_btn_repo_create=nullptr;
struct SkyGroup{std::wstring base;std::wstring paths[6];std::wstring savename;};
static std::vector<SkyGroup> g_groups;
static bool g_updating_rename=false;
// Lanceur controls
static HWND g_list=nullptr, g_refresh=nullptr, g_btn_repo=nullptr;
static HWND g_btn_watch=nullptr, g_btn_lock=nullptr, g_btn_perm=nullptr;

static HWND g_ready_lbl=nullptr;
static COLORREF g_ready_col=RGB(60,60,60);
static HBRUSH g_ready_br=nullptr;
// Purge controls
static HWND g_purge_list=nullptr, g_purge_refresh=nullptr;
static HWND g_purge_sky=nullptr, g_purge_path=nullptr, g_purge_browse=nullptr;
static HWND g_purge_apply=nullptr, g_purge_btn_repo=nullptr;
static HWND g_purge_lbl_sav=nullptr, g_purge_lbl_sky=nullptr, g_purge_lbl_dir=nullptr;

static bool g_mode_create=false;
static bool g_mode_purge=false;
static bool g_folder_mode=false;

// ── Dark theme ────────────────────────────────────────────────────────────────
#define DK_BG     RGB(18,18,20)
#define DK_PANEL  RGB(30,30,33)
#define DK_CTRL   RGB(42,42,48)
#define DK_HOVER  RGB(55,55,62)
#define DK_ACCENT RGB(0,120,212)
#define DK_ACCHV  RGB(25,140,232)
#define DK_TEXT   RGB(220,220,224)
static HBRUSH g_br_bg=nullptr,g_br_panel=nullptr,g_br_ctrl=nullptr;
static HWND g_tab_launch=nullptr,g_tab_purge=nullptr,g_tab_create=nullptr,g_btn_settings=nullptr;
static HWND g_hover_tab=nullptr;
static int  g_hanim=0;
static bool g_hanim_in=false,g_tmouse=false;
static HICON g_ico_folder=nullptr,g_ico_gear=nullptr;

static std::atomic<bool> g_watching{false};
static std::thread        g_wthread;
static HANDLE             g_dir_handle=INVALID_HANDLE_VALUE;
static HANDLE             g_lock_handle=INVALID_HANDLE_VALUE;
static bool               g_locked=false, g_permanent=false, g_perm_pending=false;
static std::wstring       g_active_save; // dossier de sauvegarde actif
static COLORREF g_status_col=RGB(0,140,0);

// ── CRC32 ──────────────────────────────────────────────────────────────────────
static uint32_t g_crc[256];
static void crc_init(){static bool ok=false;if(ok)return;ok=true;for(uint32_t i=0;i<256;i++){uint32_t c=i;for(int j=0;j<8;j++)c=(c&1)?(0xEDB88320^(c>>1)):(c>>1);g_crc[i]=c;}}
static uint32_t crc32b(const uint8_t*d,size_t n){crc_init();uint32_t c=0xFFFFFFFF;for(size_t i=0;i<n;i++)c=g_crc[(c^d[i])&0xFF]^(c>>8);return c^0xFFFFFFFF;}

// ── Binary helpers ─────────────────────────────────────────────────────────────
static uint16_t r16(const uint8_t*p){return p[0]|uint16_t(p[1])<<8;}
static uint32_t r32(const uint8_t*p){return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static void w16(std::vector<uint8_t>&b,uint16_t v){b.push_back(v&0xFF);b.push_back(v>>8);}
static void w32(std::vector<uint8_t>&b,uint32_t v){b.push_back(v&0xFF);b.push_back((v>>8)&0xFF);b.push_back((v>>16)&0xFF);b.push_back(v>>24);}
static void w32at(std::vector<uint8_t>&b,size_t p,uint32_t v){b[p]=v&0xFF;b[p+1]=(v>>8)&0xFF;b[p+2]=(v>>16)&0xFF;b[p+3]=v>>24;}

// ── ZIP ────────────────────────────────────────────────────────────────────────
struct ZipEnt{std::string name;std::vector<uint8_t>data;uint32_t crc=0,off=0;};
static std::vector<uint8_t> zip_write(std::vector<ZipEnt>&es){
    std::vector<uint8_t>buf;
    for(auto&e:es){
        e.off=(uint32_t)buf.size();
        w32(buf,0x04034b50);w16(buf,20);w16(buf,0);w16(buf,0);w16(buf,0);w16(buf,0);
        w32(buf,e.crc);w32(buf,(uint32_t)e.data.size());w32(buf,(uint32_t)e.data.size());
        w16(buf,(uint16_t)e.name.size());w16(buf,0);
        for(char c:e.name)buf.push_back((uint8_t)c);
        buf.insert(buf.end(),e.data.begin(),e.data.end());
    }
    uint32_t cdo=(uint32_t)buf.size();
    for(auto&e:es){
        w32(buf,0x02014b50);w16(buf,20);w16(buf,20);w16(buf,0);w16(buf,0);w16(buf,0);w16(buf,0);
        w32(buf,e.crc);w32(buf,(uint32_t)e.data.size());w32(buf,(uint32_t)e.data.size());
        w16(buf,(uint16_t)e.name.size());w16(buf,0);w16(buf,0);w16(buf,0);w16(buf,0);w32(buf,0);w32(buf,e.off);
        for(char c:e.name)buf.push_back((uint8_t)c);
    }
    uint32_t cds=(uint32_t)buf.size()-cdo;
    w32(buf,0x06054b50);w16(buf,0);w16(buf,0);
    w16(buf,(uint16_t)es.size());w16(buf,(uint16_t)es.size());
    w32(buf,cds);w32(buf,cdo);w16(buf,0);
    return buf;
}
static bool zip_read(const uint8_t*d,size_t sz,std::vector<ZipEnt>&out){
    if(sz<22)return false;
    int32_t ep=-1;
    for(int32_t i=(int32_t)sz-22;i>=0;i--)if(r32(d+i)==0x06054b50){ep=i;break;}
    if(ep<0)return false;
    uint16_t n=r16(d+ep+10);uint32_t co=r32(d+ep+16);
    const uint8_t*p=d+co;
    for(uint16_t i=0;i<n;i++){
        if(p+46>d+sz||r32(p)!=0x02014b50)break;
        uint16_t nl=r16(p+28),xl=r16(p+30),cl=r16(p+32);uint32_t lo=r32(p+42);
        std::string nm((const char*)p+46,nl);
        const uint8_t*lp=d+lo;
        if(lp+30>d+sz){p+=46+nl+xl+cl;continue;}
        uint16_t lnl=r16(lp+26),lxl=r16(lp+28);
        uint32_t csz=r32(lp+18),crcv=r32(lp+14);
        const uint8_t*dp=lp+30+lnl+lxl;
        if(dp+csz>d+sz){p+=46+nl+xl+cl;continue;}
        ZipEnt e;e.name=nm;e.crc=crcv;e.data=std::vector<uint8_t>(dp,dp+csz);
        out.push_back(e);p+=46+nl+xl+cl;
    }
    return true;
}

// ── BSP ────────────────────────────────────────────────────────────────────────
static std::string bsp_sky(const std::vector<uint8_t>&b){
    if(b.size()<16)return{};
    uint32_t o=r32(b.data()+8),l=r32(b.data()+12);
    if(o+l>b.size())return{};
    std::string en((const char*)b.data()+o,l);
    size_t p=en.find("\"skyname\"");if(p==en.npos)return{};
    p=en.find('"',p+9);if(p==en.npos)return{};
    size_t e=en.find('"',p+1);if(e==en.npos)return{};
    return en.substr(p+1,e-p-1);
}
static std::vector<uint8_t> bsp_apply(const std::vector<uint8_t>&bsp,const std::string&sky,const std::map<std::string,std::vector<uint8_t>>&vtfs){
    uint32_t ho=8+40*16;
    if(bsp.size()<ho+8)return{};
    uint32_t po=r32(bsp.data()+ho),pl=r32(bsp.data()+ho+4);
    if(po>bsp.size()||po+pl>bsp.size())return{};
    std::set<std::string>rep;
    for(auto&kv:vtfs){rep.insert("materials/skybox/"+sky+kv.first+".vtf");rep.insert("materials/skybox/"+sky+kv.first+".vmt");}
    std::vector<ZipEnt>es;
    if(pl>0)zip_read(bsp.data()+po,pl,es);
    es.erase(std::remove_if(es.begin(),es.end(),[&](const ZipEnt&e){return rep.count(e.name)>0;}),es.end());
    for(auto&kv:vtfs){
        ZipEnt v;v.name="materials/skybox/"+sky+kv.first+".vtf";v.data=kv.second;v.crc=crc32b(v.data.data(),v.data.size());es.push_back(v);
        std::string t="\"UnlitGeneric\"\n{\n\t\"$basetexture\" \"skybox/"+sky+kv.first+"\"\n\t\"$nofog\" \"1\"\n\t\"$ignorez\" \"1\"\n}\n";
        ZipEnt m;m.name="materials/skybox/"+sky+kv.first+".vmt";m.data=std::vector<uint8_t>(t.begin(),t.end());m.crc=crc32b(m.data.data(),m.data.size());es.push_back(m);
    }
    auto pak=zip_write(es);
    std::vector<uint8_t>out=bsp;
    uint32_t no=(uint32_t)out.size();
    w32at(out,ho,no);w32at(out,ho+4,(uint32_t)pak.size());
    out.insert(out.end(),pak.begin(),pak.end());
    return out;
}

// ── File I/O ───────────────────────────────────────────────────────────────────
static std::vector<uint8_t> fread_all(const std::wstring&p){
    HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if(h==INVALID_HANDLE_VALUE)return{};
    LARGE_INTEGER sz;GetFileSizeEx(h,&sz);
    std::vector<uint8_t>buf((size_t)sz.QuadPart);
    DWORD rd;ReadFile(h,buf.data(),(DWORD)buf.size(),&rd,nullptr);
    CloseHandle(h);return buf;
}
static bool fwrite_all(const std::wstring&p,const std::vector<uint8_t>&d){
    SetFileAttributesW(p.c_str(),FILE_ATTRIBUTE_NORMAL);
    HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD wr;WriteFile(h,d.data(),(DWORD)d.size(),&wr,nullptr);
    CloseHandle(h);return wr==(DWORD)d.size();
}

// ── Status ─────────────────────────────────────────────────────────────────────
static void ui_status(const std::wstring&msg,COLORREF col=RGB(0,140,0)){
    size_t n=msg.size()+1;wchar_t*s=new wchar_t[n];wcscpy_s(s,n,msg.c_str());
    PostMessageW(g_hwnd,WM_SETSTATUS,col,(LPARAM)s);
}

// ── icacls silent ──────────────────────────────────────────────────────────────
static void icacls(const std::wstring&args){
    STARTUPINFOW si={sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION pi;
    std::wstring cmd=L"icacls "+args;
    if(CreateProcessW(nullptr,(LPWSTR)cmd.c_str(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
        WaitForSingleObject(pi.hProcess,5000);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
    }
}

// ── Lock ───────────────────────────────────────────────────────────────────────
static void lock_bsp(const std::wstring&p){
    if(g_lock_handle!=INVALID_HANDLE_VALUE){CloseHandle(g_lock_handle);g_lock_handle=INVALID_HANDLE_VALUE;}
    HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(h!=INVALID_HANDLE_VALUE){g_lock_handle=h;g_locked=true;}
}
static void unlock_bsp(){
    if(g_lock_handle!=INVALID_HANDLE_VALUE){CloseHandle(g_lock_handle);g_lock_handle=INVALID_HANDLE_VALUE;}
    g_locked=false;
}

// ── Find BSP in cache ──────────────────────────────────────────────────────────
static std::wstring find_cache_bsp(){
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW((std::wstring(CACHE_DIR)+L"\\*.bsp").c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE)return{};
    std::wstring r=std::wstring(CACHE_DIR)+L"\\"+fd.cFileName;
    FindClose(h);return r;
}

// ── Detect face from filename ──────────────────────────────────────────────────
static int detect_face(const std::wstring&fname){
    size_t dot=fname.rfind(L'.');
    std::wstring stem=(dot!=std::wstring::npos)?fname.substr(0,dot):fname;
    std::transform(stem.begin(),stem.end(),stem.begin(),::towlower);
    // Check suffix (e.g. sky_invest01ft → ends with "ft")
    const wchar_t*codes[6]={L"ft",L"bk",L"lf",L"rt",L"up",L"dn"};
    for(int i=0;i<6;i++){
        std::wstring c=codes[i];
        if(stem.size()>=c.size()&&stem.substr(stem.size()-c.size())==c)return i;
    }
    return -1;
}

// ── Scan folder — détecte tous les groupes skybox ─────────────────────────────
static void refresh_group_list(){
    SendMessageW(g_group_list,LB_RESETCONTENT,0,0);
    for(auto&g:g_groups){
        int cnt=0;for(int i=0;i<6;i++)if(!g.paths[i].empty())cnt++;
        std::wstring lbl=g.savename+L"  ("+std::to_wstring(cnt)+L"/6 faces)";
        SendMessageW(g_group_list,LB_ADDSTRING,0,(LPARAM)lbl.c_str());
    }
}
static void scan_folder(const std::wstring&dir){
    std::map<std::wstring,SkyGroup> gmap;
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW((dir+L"\\*.vtf").c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE){ui_status(L"Aucun VTF trouvé dans ce dossier",RGB(200,0,0));return;}
    do{
        std::wstring fname=fd.cFileName;
        int face=detect_face(fname);
        if(face<0)continue;
        size_t dot=fname.rfind(L'.');
        std::wstring stem=(dot!=std::wstring::npos)?fname.substr(0,dot):fname;
        // base = stem minus last 2 chars (face code), trim trailing separators
        std::wstring base=stem.size()>=2?stem.substr(0,stem.size()-2):stem;
        while(!base.empty()&&(base.back()==L'.'||base.back()==L'_'||base.back()==L'-'))base.pop_back();
        std::wstring key=base;
        std::transform(key.begin(),key.end(),key.begin(),::towlower);
        if(gmap.find(key)==gmap.end()){gmap[key].base=base;gmap[key].savename=base;}
        gmap[key].paths[face]=dir+L"\\"+fname;
    }while(FindNextFileW(h,&fd));
    FindClose(h);
    g_groups.clear();
    for(auto&kv:gmap)g_groups.push_back(kv.second);
    refresh_group_list();
    if(!g_groups.empty()){
        SendMessageW(g_group_list,LB_SETCURSEL,0,0);
        g_updating_rename=true;SetWindowTextW(g_rename_edit,g_groups[0].savename.c_str());g_updating_rename=false;
    }
    ui_status(std::to_wstring(g_groups.size())+L" skybox détectée(s) — renomme si besoin puis Sauvegarder",RGB(0,140,0));
}

// ── Collect VTFs for apply ────────────────────────────────────────────────────
static std::map<std::string,std::vector<uint8_t>> collect_vtfs(){
    std::map<std::string,std::vector<uint8_t>>r;
    if(g_folder_mode){
        int idx=(int)SendMessageW(g_group_list,LB_GETCURSEL,0,0);
        if(idx>=0&&idx<(int)g_groups.size()){
            for(int i=0;i<6;i++){
                if(g_groups[idx].paths[i].empty())continue;
                auto d=fread_all(g_groups[idx].paths[i]);if(!d.empty())r[FACES[i]]=d;
            }
        }
    } else {
        for(int i=0;i<6;i++){
            wchar_t buf[MAX_PATH]={};GetWindowTextW(g_edits[i],buf,MAX_PATH);
            if(!buf[0])continue;
            auto d=fread_all(buf);if(!d.empty())r[FACES[i]]=d;
        }
    }
    return r;
}

// ── Refresh save list (folders in C:\skybox) ──────────────────────────────────
static void refresh_saves(){
    SendMessageW(g_list,LB_RESETCONTENT,0,0);
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW((std::wstring(SAVE_DIR)+L"\\*").c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE)return;
    do{
        if((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&fd.cFileName[0]!=L'.')
            SendMessageW(g_list,LB_ADDSTRING,0,(LPARAM)fd.cFileName);
    }while(FindNextFileW(h,&fd));
    FindClose(h);
    if(SendMessageW(g_list,LB_GETCOUNT,0,0)>0)SendMessageW(g_list,LB_SETCURSEL,0,0);
}

// ── Get selected save folder path ─────────────────────────────────────────────
static std::wstring selected_save(){
    int idx=(int)SendMessageW(g_list,LB_GETCURSEL,0,0);
    if(idx<0)return{};
    int len=(int)SendMessageW(g_list,LB_GETTEXTLEN,idx,0);
    std::wstring s(len,L'\0');
    SendMessageW(g_list,LB_GETTEXT,idx,(LPARAM)s.data());
    return std::wstring(SAVE_DIR)+L"\\"+s;
}

// ── Load VTFs from a save folder ──────────────────────────────────────────────
static std::map<std::string,std::vector<uint8_t>> vtfs_from_save(const std::wstring&dir){
    std::map<std::string,std::vector<uint8_t>>r;
    for(int i=0;i<6;i++){
        std::wstring fn(FACES[i],FACES[i]+2);
        auto d=fread_all(dir+L"\\"+fn+L".vtf");
        if(!d.empty())r[FACES[i]]=d;
    }
    return r;
}

// ── Core apply+lock ───────────────────────────────────────────────────────────
static void do_apply(const std::wstring&cache_path,const std::map<std::string,std::vector<uint8_t>>&vtfs){
    try{
        Sleep(50); // minimal — la notification LAST_WRITE arrive après fermeture du handle GMod
        auto bsp=fread_all(cache_path);
        if(bsp.size()<4||memcmp(bsp.data(),"VBSP",4))return;
        std::string sky=bsp_sky(bsp);if(sky.empty())return;
        auto patched=bsp_apply(bsp,sky,vtfs);
        if(patched.empty())return;
        bool written=false;
        for(int i=0;i<20&&!written;i++){written=fwrite_all(cache_path,patched);if(!written)Sleep(50);}
        if(!written){ui_status(L"Impossible d'écrire le BSP (GMod le tient?)",RGB(200,0,0));return;}
        // Sauvegarder une copie du BSP modifié pour restaurer au prochain lancement
        CopyFileW(cache_path.c_str(),(SAVE_DIR+L"\\__bsp_backup.bsp").c_str(),FALSE);
        lock_bsp(cache_path);
        PostMessageW(g_hwnd,WM_UPDLOCK,0,0);
        PostMessageW(g_hwnd,WM_SETREADY,1,0); // prêt à restart
        if(g_perm_pending){
            icacls(L"\""+cache_path+L"\" /deny Everyone:(W,D,DC,WD)");
            g_permanent=true;g_perm_pending=false;
            PostMessageW(g_hwnd,WM_APP+3,1,0);
            ui_status(L"✓ Skybox verrouillée — relance ou rejoins le serveur",RGB(0,140,0));
        }
    }catch(...){ui_status(L"Erreur lors de l'application",RGB(200,0,0));}
}

// ── GMod process watcher ───────────────────────────────────────────────────────
static DWORD find_gmod_pid(){
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snap==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W pe={sizeof(pe)};
    DWORD pid=0;
    if(Process32FirstW(snap,&pe))do{
        if(_wcsicmp(pe.szExeFile,L"hl2.exe")==0){pid=pe.th32ProcessID;break;}
    }while(Process32NextW(snap,&pe));
    CloseHandle(snap);
    return pid;
}

static void gmod_exit_watcher(){
    bool was_running=false;
    while(g_watching){
        bool running=(find_gmod_pid()!=0);
        if(was_running&&!running&&g_watching){
            // GMod vient de fermer → relâcher le verrou pour le prochain lancement
            unlock_bsp();
            std::wstring dest=CACHE_DIR+L"\\map_pack.bsp";
            icacls(L"\""+dest+L"\" /remove:d Everyone");
            SetFileAttributesW(dest.c_str(),FILE_ATTRIBUTE_NORMAL);
            g_perm_pending=true; // prochain do_apply re-lockera
        }
        was_running=running;
        Sleep(3000);
    }
}

// ── Watch thread ───────────────────────────────────────────────────────────────
static void watch_fn(){
    auto load_vtfs=[&](){return vtfs_from_save(g_active_save);};
    // Si pas de BSP dans le cache, restaurer le backup de la session précédente
    auto p=find_cache_bsp();
    if(p.empty()){
        std::wstring bkp=SAVE_DIR+L"\\__bsp_backup.bsp";
        std::wstring dest=CACHE_DIR+L"\\map_pack.bsp";
        if(GetFileAttributesW(bkp.c_str())!=INVALID_FILE_ATTRIBUTES){
            if(CopyFileW(bkp.c_str(),dest.c_str(),FALSE))p=dest;
        }
    }
    // Appliquer la skybox au BSP trouvé (existant ou restauré)
    if(!p.empty()){
        auto vtfs=load_vtfs();
        if(!vtfs.empty()){
            ui_status(L"BSP trouvé, application…",RGB(0,100,200));
            do_apply(p,vtfs);
            ui_status(L"✓ Skybox appliquée — relance ou rejoins le serveur",RGB(0,140,0));
        }
    }
    // Watch directory
    g_dir_handle=CreateFileW(CACHE_DIR.c_str(),FILE_LIST_DIRECTORY,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,
        OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(g_dir_handle==INVALID_HANDLE_VALUE)return;
    static uint8_t buf[4096];
    while(g_watching){
        DWORD br=0;
        if(!ReadDirectoryChangesW(g_dir_handle,buf,sizeof(buf),FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE,&br,nullptr,nullptr))
            break;
        if(!g_watching)break;
        DWORD off=0;
        while(true){
            auto*ni=(FILE_NOTIFY_INFORMATION*)(buf+off);
            std::wstring nm(ni->FileName,ni->FileNameLength/sizeof(wchar_t));
            if(nm.size()>4&&nm.rfind(L".bsp")==nm.size()-4){
                std::wstring full=std::wstring(CACHE_DIR)+L"\\"+nm;
                ui_status(L"BSP détecté: "+nm,RGB(0,100,200));
                auto vtfs=load_vtfs(); // lit la sauvegarde sélectionnée au moment de l'application
                if(!vtfs.empty())do_apply(full,vtfs);
                ui_status(L"✓ Skybox appliquée — relance ou rejoins le serveur",RGB(0,140,0));
            }
            if(!ni->NextEntryOffset)break;
            off+=ni->NextEntryOffset;
        }
    }
    CloseHandle(g_dir_handle);g_dir_handle=INVALID_HANDLE_VALUE;
}

// ── Show/hide mode panels ──────────────────────────────────────────────────────
static void show_create_controls(bool show){
    int sw=show?SW_SHOW:SW_HIDE;
    ShowWindow(g_rad_man,sw);ShowWindow(g_rad_dir,sw);
    ShowWindow(g_save_btn,sw);ShowWindow(g_btn_repo_create,sw);
    if(!show){
        // Tout cacher
        ShowWindow(g_name,SW_HIDE);ShowWindow(g_name_lbl,SW_HIDE);
        ShowWindow(g_dir_edit,SW_HIDE);ShowWindow(g_dir_browse,SW_HIDE);ShowWindow(g_dir_lbl,SW_HIDE);
        ShowWindow(g_group_list,SW_HIDE);ShowWindow(g_rename_edit,SW_HIDE);ShowWindow(g_rename_lbl,SW_HIDE);
        for(int i=0;i<6;i++){ShowWindow(g_man_labels[i],SW_HIDE);ShowWindow(g_edits[i],SW_HIDE);ShowWindow(g_browses[i],SW_HIDE);}
    } else {
        bool folder=g_folder_mode;
        ShowWindow(g_name,folder?SW_HIDE:SW_SHOW);ShowWindow(g_name_lbl,folder?SW_HIDE:SW_SHOW);
        ShowWindow(g_dir_edit,folder?SW_SHOW:SW_HIDE);ShowWindow(g_dir_browse,folder?SW_SHOW:SW_HIDE);ShowWindow(g_dir_lbl,folder?SW_SHOW:SW_HIDE);
        ShowWindow(g_group_list,folder?SW_SHOW:SW_HIDE);ShowWindow(g_rename_edit,folder?SW_SHOW:SW_HIDE);ShowWindow(g_rename_lbl,folder?SW_SHOW:SW_HIDE);
        for(int i=0;i<6;i++){
            ShowWindow(g_man_labels[i],folder?SW_HIDE:SW_SHOW);
            ShowWindow(g_edits[i],folder?SW_HIDE:SW_SHOW);
            ShowWindow(g_browses[i],folder?SW_HIDE:SW_SHOW);
        }
    }
}
static void show_launch_controls(bool show){
    int sw=show?SW_SHOW:SW_HIDE;
    ShowWindow(g_list,sw);ShowWindow(g_refresh,sw);ShowWindow(g_btn_repo,sw);
    ShowWindow(g_btn_watch,sw);ShowWindow(g_ready_lbl,sw);
}
static void show_purge_controls(bool show){
    int sw=show?SW_SHOW:SW_HIDE;
    ShowWindow(g_purge_lbl_sav,sw);ShowWindow(g_purge_refresh,sw);ShowWindow(g_purge_btn_repo,sw);ShowWindow(g_purge_list,sw);
    ShowWindow(g_purge_lbl_sky,sw);ShowWindow(g_purge_sky,sw);
    ShowWindow(g_purge_lbl_dir,sw);ShowWindow(g_purge_path,sw);ShowWindow(g_purge_browse,sw);
    ShowWindow(g_purge_apply,sw);
}
static void refresh_purge_saves(){
    SendMessageW(g_purge_list,LB_RESETCONTENT,0,0);
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW((std::wstring(SAVE_DIR)+L"\\*").c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE)return;
    do{
        if((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&fd.cFileName[0]!=L'.')
            SendMessageW(g_purge_list,LB_ADDSTRING,0,(LPARAM)fd.cFileName);
    }while(FindNextFileW(h,&fd));
    FindClose(h);
    if(SendMessageW(g_purge_list,LB_GETCOUNT,0,0)>0)SendMessageW(g_purge_list,LB_SETCURSEL,0,0);
}

// ── Browse folder (SHBrowseForFolder) ─────────────────────────────────────────
static std::wstring browse_folder(){
    wchar_t buf[MAX_PATH]={};
    BROWSEINFOW bi={};bi.hwndOwner=g_hwnd;bi.pszDisplayName=buf;
    bi.lpszTitle=L"Sélectionne le dossier contenant les VTF";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST il=SHBrowseForFolderW(&bi);
    if(!il)return{};
    wchar_t path[MAX_PATH]={};SHGetPathFromIDListW(il,path);
    CoTaskMemFree(il);return path;
}

// ── VTF decoder ───────────────────────────────────────────────────────────────
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
static int vtf_mip_bytes(uint32_t fmt,int w,int h){
    w=std::max(1,w);h=std::max(1,h);
    if(fmt==13||fmt==20){int bw=(w+3)/4,bh=(h+3)/4;return std::max(1,bw)*std::max(1,bh)*8;}
    if(fmt==14||fmt==15){int bw=(w+3)/4,bh=(h+3)/4;return std::max(1,bw)*std::max(1,bh)*16;}
    if(fmt==2||fmt==3||fmt==9||fmt==10)return w*h*3;
    return w*h*4;
}
static void vtf_dxt1(const uint8_t*src,int w,int h,uint8_t*dst){
    int bw=(w+3)/4,bh=(h+3)/4;
    for(int by=0;by<bh;by++)for(int bx=0;bx<bw;bx++){
        const uint8_t*b=src+(by*bw+bx)*8;
        uint16_t c0=b[0]|(b[1]<<8),c1=b[2]|(b[3]<<8);
        uint32_t m=b[4]|(b[5]<<8)|(uint32_t(b[6])<<16)|(uint32_t(b[7])<<24);
        uint8_t r[4],g[4],bl[4];
        r[0]=(c0>>11)*255/31;g[0]=((c0>>5)&0x3F)*255/63;bl[0]=(c0&0x1F)*255/31;
        r[1]=(c1>>11)*255/31;g[1]=((c1>>5)&0x3F)*255/63;bl[1]=(c1&0x1F)*255/31;
        if(c0>c1){r[2]=(2*r[0]+r[1])/3;g[2]=(2*g[0]+g[1])/3;bl[2]=(2*bl[0]+bl[1])/3;r[3]=(r[0]+2*r[1])/3;g[3]=(g[0]+2*g[1])/3;bl[3]=(bl[0]+2*bl[1])/3;}
        else{r[2]=(r[0]+r[1])/2;g[2]=(g[0]+g[1])/2;bl[2]=(bl[0]+bl[1])/2;r[3]=g[3]=bl[3]=0;}
        for(int y=0;y<4;y++)for(int x=0;x<4;x++){
            int px=bx*4+x,py=by*4+y;if(px>=w||py>=h)continue;
            int i=(m>>(2*(y*4+x)))&3;uint8_t*p=dst+(py*w+px)*4;
            p[0]=bl[i];p[1]=g[i];p[2]=r[i];p[3]=255;
        }
    }
}
static void vtf_dxt5(const uint8_t*src,int w,int h,uint8_t*dst){
    int bw=(w+3)/4,bh=(h+3)/4;
    for(int by=0;by<bh;by++)for(int bx=0;bx<bw;bx++){
        const uint8_t*b=src+(by*bw+bx)*16;
        uint8_t a0=b[0],a1=b[1],alut[8];alut[0]=a0;alut[1]=a1;
        if(a0>a1){for(int i=2;i<8;i++)alut[i]=(uint8_t)(((8-i)*a0+(i-1)*a1)/7);}
        else{for(int i=2;i<6;i++)alut[i]=(uint8_t)(((6-i)*a0+(i-1)*a1)/5);alut[6]=0;alut[7]=255;}
        uint64_t am=uint64_t(b[2])|(uint64_t(b[3])<<8)|(uint64_t(b[4])<<16)|(uint64_t(b[5])<<24)|(uint64_t(b[6])<<32)|(uint64_t(b[7])<<40);
        const uint8_t*cb=b+8;
        uint16_t c0=cb[0]|(cb[1]<<8),c1=cb[2]|(cb[3]<<8);
        uint32_t cm=cb[4]|(cb[5]<<8)|(uint32_t(cb[6])<<16)|(uint32_t(cb[7])<<24);
        uint8_t r[4],g[4],bl[4];
        r[0]=(c0>>11)*255/31;g[0]=((c0>>5)&0x3F)*255/63;bl[0]=(c0&0x1F)*255/31;
        r[1]=(c1>>11)*255/31;g[1]=((c1>>5)&0x3F)*255/63;bl[1]=(c1&0x1F)*255/31;
        if(c0>c1){r[2]=(2*r[0]+r[1])/3;g[2]=(2*g[0]+g[1])/3;bl[2]=(2*bl[0]+bl[1])/3;r[3]=(r[0]+2*r[1])/3;g[3]=(g[0]+2*g[1])/3;bl[3]=(bl[0]+2*bl[1])/3;}
        else{r[2]=(r[0]+r[1])/2;g[2]=(g[0]+g[1])/2;bl[2]=(bl[0]+bl[1])/2;r[3]=g[3]=bl[3]=0;}
        for(int y=0;y<4;y++)for(int x=0;x<4;x++){
            int px=bx*4+x,py=by*4+y;if(px>=w||py>=h)continue;
            int ci=(cm>>(2*(y*4+x)))&3,ai=(int)((am>>(3*(y*4+x)))&7);
            uint8_t*p=dst+(py*w+px)*4;p[0]=bl[ci];p[1]=g[ci];p[2]=r[ci];p[3]=alut[ai];
        }
    }
}
static std::vector<uint8_t> vtf_decode(const std::vector<uint8_t>&vtf,int&outW,int&outH,int tgt=65536){
    const uint8_t*d=vtf.data();size_t sz=vtf.size();
    if(sz<80||memcmp(d,"VTF\0",4)!=0)return{};
    uint32_t hdrSz=*(uint32_t*)(d+12);uint16_t fw=*(uint16_t*)(d+16),fh=*(uint16_t*)(d+18);
    uint32_t fmt=*(uint32_t*)(d+52);uint8_t mips=d[56],lowFmt=d[57],lowW=d[61],lowH=d[62];
    if(fw==0||fh==0||mips==0||hdrSz>sz)return{};
    // find mip where both dims ≤ tgt
    int mipK=mips-1;
    for(int k=0;k<mips;k++){if(std::max(1,(int)fw>>k)<=tgt&&std::max(1,(int)fh>>k)<=tgt){mipK=k;break;}}
    outW=std::max(1,(int)fw>>mipK);outH=std::max(1,(int)fh>>mipK);
    // compute byte offset
    size_t off=hdrSz+(size_t)vtf_mip_bytes(lowFmt,lowW,lowH);
    for(int k=mips-1;k>mipK;k--)off+=(size_t)vtf_mip_bytes(fmt,std::max(1,(int)fw>>k),std::max(1,(int)fh>>k));
    if(off+(size_t)vtf_mip_bytes(fmt,outW,outH)>sz)return{};
    const uint8_t*src=d+off;
    std::vector<uint8_t>bgra(outW*outH*4,0xFF);
    if(fmt==13||fmt==20)vtf_dxt1(src,outW,outH,bgra.data());
    else if(fmt==15)vtf_dxt5(src,outW,outH,bgra.data());
    else if(fmt==14){vtf_dxt1(src,outW,outH,bgra.data());} // DXT3 — approx, skip alpha block
    else if(fmt==3||fmt==10){for(int i=0;i<outW*outH;i++){bgra[i*4]=src[i*3];bgra[i*4+1]=src[i*3+1];bgra[i*4+2]=src[i*3+2];bgra[i*4+3]=255;}}
    else if(fmt==2||fmt==9){for(int i=0;i<outW*outH;i++){bgra[i*4]=src[i*3+2];bgra[i*4+1]=src[i*3+1];bgra[i*4+2]=src[i*3];bgra[i*4+3]=255;}}
    else if(fmt==12||fmt==16){for(int i=0;i<outW*outH;i++){bgra[i*4]=src[i*4];bgra[i*4+1]=src[i*4+1];bgra[i*4+2]=src[i*4+2];bgra[i*4+3]=255;}}
    else if(fmt==0){for(int i=0;i<outW*outH;i++){bgra[i*4]=src[i*4+2];bgra[i*4+1]=src[i*4+1];bgra[i*4+2]=src[i*4];bgra[i*4+3]=src[i*4+3];}}
    return bgra;
}
static void update_preview(const std::wstring paths[6]){
    for(int i=0;i<6;i++){g_prev_bgra[i].clear();g_prev_w[i]=0;g_prev_h[i]=0;
        if(!paths[i].empty()){auto v=fread_all(paths[i]);if(!v.empty())g_prev_bgra[i]=vtf_decode(v,g_prev_w[i],g_prev_h[i]);}}
    if(g_preview)InvalidateRect(g_preview,nullptr,TRUE);
}
static void update_preview_save(const std::wstring&dir){
    std::wstring paths[6];
    for(int i=0;i<6;i++){std::wstring fn(FACES[i],FACES[i]+2);paths[i]=dir+L"\\"+fn+L".vtf";
        if(GetFileAttributesW(paths[i].c_str())==INVALID_FILE_ATTRIBUTES)paths[i]=L"";}
    update_preview(paths);
}

// ── Preview window WndProc (fenêtre séparée) ──────────────────────────────────
static LRESULT CALLBACK PreviewWndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_CLOSE){if(g_hwnd)SendMessageW(g_hwnd,WM_CLOSE,0,0);return 0;}
    if(msg==WM_SIZE){if(wp==SIZE_MINIMIZED&&g_hwnd)ShowWindow(g_hwnd,SW_MINIMIZE);
        else if(wp==SIZE_RESTORED&&g_hwnd)ShowWindow(g_hwnd,SW_SHOWNOACTIVATE);}

    if(msg==WM_PAINT){
        PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);
        RECT rc;GetClientRect(hw,&rc);
        HBRUSH bg=CreateSolidBrush(RGB(18,18,18));FillRect(hdc,&rc,bg);DeleteObject(bg);
        bool any=false;for(int i=0;i<6;i++)if(!g_prev_bgra[i].empty())any=true;
        if(!any){
            // Message centré
            HFONT hf=CreateFontW(20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
            HFONT old=(HFONT)SelectObject(hdc,hf);
            SetTextColor(hdc,RGB(90,90,90));SetBkMode(hdc,TRANSPARENT);
            const wchar_t*txt=L"Aucune skybox sélectionnée";int len=(int)wcslen(txt);
            SIZE sz;GetTextExtentPoint32W(hdc,txt,len,&sz);
            TextOutW(hdc,(rc.right-sz.cx)/2,(rc.bottom-sz.cy)/2,txt,len);
            SelectObject(hdc,old);DeleteObject(hf);
        } else {
            // Croix : ft=0,bk=1,lf=2,rt=3,up=4,dn=5
            // (col,row): ft(1,1) bk(3,1) lf(0,1) rt(2,1) up(1,0) dn(1,2)
            static const int COL[6]={1,3,0,2,1,1},ROW[6]={1,1,1,1,0,2};
            static const wchar_t*LBL[6]={L"ft",L"bk",L"lf",L"rt",L"up",L"dn"};
            const int FS=150;
            int crossW=4*FS,crossH=3*FS;
            int OX=(rc.right-crossW)/2,OY=(rc.bottom-crossH)/2;
            HFONT hf=CreateFontW(13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
            HFONT old=(HFONT)SelectObject(hdc,hf);
            for(int i=0;i<6;i++){
                int px=OX+COL[i]*FS,py=OY+ROW[i]*FS;
                RECT fr={px,py,px+FS,py+FS};
                HBRUSH br=CreateSolidBrush(RGB(40,40,40));FrameRect(hdc,&fr,br);DeleteObject(br);
                if(!g_prev_bgra[i].empty()&&g_prev_w[i]>0){
                    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                    bi.bmiHeader.biWidth=g_prev_w[i];bi.bmiHeader.biHeight=-g_prev_h[i];
                    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
                    SetStretchBltMode(hdc,HALFTONE);SetBrushOrgEx(hdc,0,0,nullptr);
                    StretchDIBits(hdc,px+1,py+1,FS-2,FS-2,0,0,g_prev_w[i],g_prev_h[i],
                        g_prev_bgra[i].data(),&bi,DIB_RGB_COLORS,SRCCOPY);
                    // Étiquette face
                    SetTextColor(hdc,RGB(180,180,180));SetBkMode(hdc,TRANSPARENT);
                    TextOutW(hdc,px+3,py+3,LBL[i],2);
                } else {
                    SetTextColor(hdc,RGB(55,55,55));SetBkMode(hdc,TRANSPARENT);
                    SIZE sz;GetTextExtentPoint32W(hdc,LBL[i],2,&sz);
                    TextOutW(hdc,px+(FS-sz.cx)/2,py+(FS-sz.cy)/2,LBL[i],2);
                }
            }
            SelectObject(hdc,old);DeleteObject(hf);
        }
        EndPaint(hw,&ps);return 0;
    }
    return DefWindowProcW(hw,msg,wp,lp);
}

// ── Window procedure ───────────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        g_hwnd=hw;
        CreateDirectoryW(SAVE_DIR.c_str(),nullptr);

        // ── Icône dossier chargée en premier ────────────────────────────────
        {SHSTOCKICONINFO sii={sizeof(sii)};
         if(SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER,SHGSI_ICON|SHGSI_SMALLICON,&sii)))g_ico_folder=sii.hIcon;}

        // ── Mode tab buttons ────────────────────────────────────────────────
        g_tab_launch  =CreateWindowW(L"BUTTON",L"SKYBOX ROCKFORD",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,5,  5,150,26,hw,(HMENU)IDC_TAB_LAUNCH,nullptr,nullptr);
        g_tab_purge   =CreateWindowW(L"BUTTON",L"SKYBOX PURGE",   WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,160,5,130,26,hw,(HMENU)IDC_TAB_PURGE, nullptr,nullptr);
        g_tab_create  =CreateWindowW(L"BUTTON",L"CRÉATEUR",       WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,295,5,100,26,hw,(HMENU)IDC_TAB_CREATE,nullptr,nullptr);
        g_btn_settings=CreateWindowW(L"BUTTON",L"Paramètres",     WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,500,5,145,26,hw,(HMENU)IDC_SETTINGS,nullptr,nullptr);

        int y=38;
        // ── Créateur: nom ───────────────────────────────────────────────────
        g_name_lbl=CreateWindowW(L"STATIC",L"Nom du fichier:",WS_CHILD|WS_VISIBLE,5,y,110,20,hw,nullptr,nullptr,nullptr);
        g_name=CreateWindowW(L"EDIT",L"map_pack",WS_CHILD|WS_VISIBLE|WS_BORDER,120,y,200,20,hw,(HMENU)IDC_NAME,nullptr,nullptr);
        y+=28;

        // ── Créateur: radio ─────────────────────────────────────────────────
        g_rad_man=CreateWindowW(L"BUTTON",L"Manuel",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|WS_GROUP,5,y,70,20,hw,(HMENU)IDC_RADIO_MAN,nullptr,nullptr);
        g_rad_dir=CreateWindowW(L"BUTTON",L"Dossier (plusieurs d'un coup)",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,80,y,210,20,hw,(HMENU)IDC_RADIO_DIR,nullptr,nullptr);
        SendMessageW(g_rad_man,BM_SETCHECK,BST_CHECKED,0);
        y+=28;

        // ── Dossier picker (hidden by default) ──────────────────────────────
        g_dir_lbl=CreateWindowW(L"STATIC",L"Dossier:",WS_CHILD,5,y,60,20,hw,nullptr,nullptr,nullptr);
        g_dir_edit=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,70,y,510,20,hw,(HMENU)IDC_DIR_EDIT,nullptr,nullptr);
        g_dir_browse=CreateWindowW(L"BUTTON",L"",WS_CHILD|BS_ICON,585,y,36,20,hw,(HMENU)IDC_DIR_BROWSE,nullptr,nullptr);
        if(g_ico_folder)SendMessageW(g_dir_browse,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_ico_folder);
        // group list: y+26 .. y+26+130=y+156; rename: y+162
        g_group_list=CreateWindowW(L"LISTBOX",nullptr,WS_CHILD|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,5,y+26,645,130,hw,(HMENU)IDC_GROUP_LIST,nullptr,nullptr);
        g_rename_lbl=CreateWindowW(L"STATIC",L"Renommer:",WS_CHILD,5,y+162,70,20,hw,nullptr,nullptr,nullptr);
        g_rename_edit=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,80,y+162,300,20,hw,(HMENU)IDC_RENAME_EDIT,nullptr,nullptr);

        // ── Manuel rows (visible by default) ────────────────────────────────
        for(int i=0;i<6;i++){
            g_man_labels[i]=CreateWindowW(L"STATIC",FLABELS[i],WS_CHILD|WS_VISIBLE,5,y+i*26,95,20,hw,nullptr,nullptr,nullptr);
            g_edits[i]=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,100,y+i*26,400,20,hw,(HMENU)(IDC_EDIT0+i),nullptr,nullptr);
            g_browses[i]=CreateWindowW(L"BUTTON",L"...",WS_CHILD|WS_VISIBLE,505,y+i*26,30,20,hw,(HMENU)(IDC_BROWSE0+i),nullptr,nullptr);
        }
        // Save button fixed at y+192 (below both modes)
        g_save_btn=CreateWindowW(L"BUTTON",L"SAUVEGARDER LES TEXTURES",WS_CHILD|WS_VISIBLE,5,y+192,220,26,hw,(HMENU)IDC_SAVE_BTN,nullptr,nullptr);
        g_btn_repo_create=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_ICON,230,y+192,36,26,hw,(HMENU)IDC_OPEN_REPO,nullptr,nullptr);
        if(g_ico_folder)SendMessageW(g_btn_repo_create,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_ico_folder);

        // ── Lanceur: listbox (hidden) ────────────────────────────────────────
        // Listbox width = 645 (x=5..650). RAFRAÎCHIR + 📂 au-dessus à droite.
        CreateWindowW(L"STATIC",L"Sauvegardes:",WS_CHILD,5,38,100,20,hw,nullptr,nullptr,nullptr);
        g_refresh=CreateWindowW(L"BUTTON",L"RAFRAÎCHIR",WS_CHILD,490,36,90,22,hw,(HMENU)IDC_REFRESH,nullptr,nullptr);
        g_btn_repo=CreateWindowW(L"BUTTON",L"",WS_CHILD|BS_ICON,585,36,40,22,hw,(HMENU)IDC_OPEN_REPO,nullptr,nullptr);
        if(g_ico_folder)SendMessageW(g_btn_repo,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_ico_folder);
        g_list=CreateWindowW(L"LISTBOX",nullptr,WS_CHILD|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,5,60,640,200,hw,(HMENU)IDC_LISTBOX,nullptr,nullptr);
        int by=270;
        g_btn_watch=CreateWindowW(L"BUTTON",L"APPLIQUER",WS_CHILD,5,by,110,30,hw,(HMENU)IDC_WATCH,nullptr,nullptr);
        // Indicateur restart — SS_CENTER|SS_CENTERIMAGE pour centrer le texte
        g_ready_lbl=CreateWindowW(L"STATIC",L"",WS_CHILD|SS_CENTER|SS_CENTERIMAGE,120,by,530,30,hw,nullptr,nullptr,nullptr);
        {HFONT hf=CreateFontW(17,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        SendMessageW(g_ready_lbl,WM_SETFONT,(WPARAM)hf,FALSE);}
        g_btn_lock=CreateWindowW(L"BUTTON",L"LOCK",WS_CHILD,5,by+50,80,26,hw,(HMENU)IDC_LOCK,nullptr,nullptr);   // caché
        g_btn_perm=CreateWindowW(L"BUTTON",L"PERMANENT",WS_CHILD,5,by+80,110,26,hw,(HMENU)IDC_PERM,nullptr,nullptr); // caché

        // ── Purge controls (cachés par défaut) ──────────────────────────────
        {// Chemin addon par défaut dérivé du CACHE_DIR
        std::wstring gmod_gg=CACHE_DIR;
        size_t pc=gmod_gg.rfind(L'\\');if(pc!=std::wstring::npos)gmod_gg=gmod_gg.substr(0,pc);
        std::wstring purge_def=gmod_gg+L"\\addons\\css-content-gmodcontent\\materials\\skybox";
        g_purge_lbl_sav=CreateWindowW(L"STATIC",L"Sauvegardes:",WS_CHILD,5,38,100,20,hw,nullptr,nullptr,nullptr);
        g_purge_refresh=CreateWindowW(L"BUTTON",L"RAFRAÎCHIR",WS_CHILD,490,36,90,22,hw,(HMENU)IDC_PURGE_REFRESH,nullptr,nullptr);
        g_purge_btn_repo=CreateWindowW(L"BUTTON",L"",WS_CHILD|BS_ICON,585,36,40,22,hw,(HMENU)IDC_OPEN_REPO,nullptr,nullptr);
        if(g_ico_folder)SendMessageW(g_purge_btn_repo,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_ico_folder);
        g_purge_list=CreateWindowW(L"LISTBOX",nullptr,WS_CHILD|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,5,60,640,155,hw,(HMENU)IDC_PURGE_LIST,nullptr,nullptr);
        g_purge_lbl_sky=CreateWindowW(L"STATIC",L"Nom skybox :",WS_CHILD,5,224,90,20,hw,nullptr,nullptr,nullptr);
        g_purge_sky=CreateWindowW(L"EDIT",L"grimmnight",WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,98,222,160,22,hw,(HMENU)IDC_PURGE_SKY,nullptr,nullptr);
        g_purge_lbl_dir=CreateWindowW(L"STATIC",L"Dossier :",WS_CHILD,5,252,70,20,hw,nullptr,nullptr,nullptr);
        g_purge_path=CreateWindowW(L"EDIT",purge_def.c_str(),WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,78,250,520,22,hw,(HMENU)IDC_PURGE_PATH,nullptr,nullptr);
        g_purge_browse=CreateWindowW(L"BUTTON",L"",WS_CHILD|BS_ICON,602,250,40,22,hw,(HMENU)IDC_PURGE_BROWSE,nullptr,nullptr);
        if(g_ico_folder)SendMessageW(g_purge_browse,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_ico_folder);
        g_purge_apply=CreateWindowW(L"BUTTON",L"APPLIQUER",WS_CHILD,5,282,110,28,hw,(HMENU)IDC_PURGE_APPLY,nullptr,nullptr);}

        // Lanceur par défaut
        show_create_controls(false);
        show_purge_controls(false);
        show_launch_controls(true);
        refresh_saves();
        PostMessageW(hw,WM_FIRST_LAUNCH,0,0);
        // Show créateur label for dir (hidden since Manuel mode)
        // Already handled: dir_edit, dir_browse, g_det[] are WS_CHILD (no WS_VISIBLE)

        // ── Status ───────────────────────────────────────────────────────────
        g_status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,5,y+224,645,20,hw,(HMENU)IDC_STATUS,nullptr,nullptr);

        // ── Dark theme init ───────────────────────────────────────────────────
        g_br_bg   =CreateSolidBrush(DK_BG);
        g_br_panel=CreateSolidBrush(DK_PANEL);
        g_br_ctrl =CreateSolidBrush(DK_CTRL);
        // Barre ready invisible par défaut (fond identique au BG)
        g_ready_col=DK_BG;
        g_ready_br =CreateSolidBrush(DK_BG);
        // Icône engrenage pour le bouton Paramètres
        if(!ExtractIconExW(L"C:\\Windows\\System32\\imageres.dll",109,nullptr,&g_ico_gear,1)||!g_ico_gear)
            ExtractIconExW(L"C:\\Windows\\System32\\shell32.dll",315,nullptr,&g_ico_gear,1);
        // Retirer les themes sur les boutons BS_ICON pour qu'ils dessinent l'icône correctement
        if(g_ico_folder){auto fi2=[](HWND b){if(b)SetWindowTheme(b,L"",L"");};
        fi2(g_dir_browse);fi2(g_btn_repo);fi2(g_btn_repo_create);fi2(g_purge_btn_repo);fi2(g_purge_browse);}
        // SetWindowTheme sur tous les boutons non-owner-draw
        auto tb=[](HWND b){if(b)SetWindowTheme(b,L"",L"");};
        tb(g_refresh);tb(g_btn_repo);tb(g_btn_watch);tb(g_btn_lock);tb(g_btn_perm);
        tb(g_purge_refresh);tb(g_purge_btn_repo);tb(g_purge_apply);tb(g_save_btn);tb(g_btn_repo_create);
        tb(g_rad_man);tb(g_rad_dir);
        // SetWindowTheme sur tous les edits pour forcer fond sombre
        auto te=[](HWND e){if(e)SetWindowTheme(e,L"",L"");};
        te(g_name);te(g_dir_edit);te(g_rename_edit);
        for(int i=0;i<6;i++)te(g_edits[i]);
        te(g_purge_sky);te(g_purge_path);
        // Dark scrollbar dans les listbox
        SetWindowTheme(g_list,L"DarkMode_Explorer",nullptr);
        SetWindowTheme(g_purge_list,L"DarkMode_Explorer",nullptr);
        SetWindowTheme(g_group_list,L"DarkMode_Explorer",nullptr);
        break;
    }

    case WM_COMMAND:{
        int id=LOWORD(wp);
        // Browse VTF (manuel mode)
        if(id>=IDC_BROWSE0&&id<IDC_BROWSE0+6){
            OPENFILENAMEW ofn={sizeof(ofn)};wchar_t file[MAX_PATH]={};
            ofn.hwndOwner=hw;ofn.lpstrFilter=L"VTF\0*.vtf\0Tous\0*.*\0";
            ofn.lpstrFile=file;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                SetWindowTextW(g_edits[id-IDC_BROWSE0],file);
                // Mettre à jour l'aperçu avec tous les fichiers sélectionnés
                std::wstring pp[6];
                for(int i=0;i<6;i++){wchar_t b[MAX_PATH]={};GetWindowTextW(g_edits[i],b,MAX_PATH);pp[i]=b;}
                std::thread([pp]{std::wstring tmp[6];for(int i=0;i<6;i++)tmp[i]=pp[i];update_preview(tmp);}).detach();
            }
            break;
        }
        switch(id){
        // ── Mode switch ────────────────────────────────────────────────────
        case IDC_OPEN_REPO:
            ShellExecuteW(nullptr,L"explore",SAVE_DIR.c_str(),nullptr,nullptr,SW_SHOW);
            break;
        case IDC_SETTINGS:{
            // Dialog inline : éditer CACHE_DIR et SAVE_DIR
            {RECT r;GetWindowRect(g_hwnd,&r);
            int cx=(r.left+r.right)/2-165,cy=(r.top+r.bottom)/2-55;
            HWND dlg=CreateWindowW(L"SI2_CFG",L"Paramètres",
                WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,cx,cy,330,155,hw,nullptr,GetModuleHandleW(nullptr),nullptr);
            ShowWindow(dlg,SW_SHOW);}
            break;
        }
        case IDC_TAB_CREATE:
            if(!g_mode_create){
                g_mode_create=true;g_mode_purge=false;
                show_launch_controls(false);
                show_purge_controls(false);
                show_create_controls(true);
                SetWindowTextW(g_status,L"");
                InvalidateRect(g_tab_launch,nullptr,TRUE);InvalidateRect(g_tab_purge,nullptr,TRUE);InvalidateRect(g_tab_create,nullptr,TRUE);
            }
            break;
        case IDC_TAB_LAUNCH:
            if(g_mode_create||g_mode_purge){
                g_mode_create=false;g_mode_purge=false;
                show_create_controls(false);
                show_purge_controls(false);
                show_launch_controls(true);
                refresh_saves();
                InvalidateRect(g_tab_launch,nullptr,TRUE);InvalidateRect(g_tab_purge,nullptr,TRUE);InvalidateRect(g_tab_create,nullptr,TRUE);
            }
            break;
        case IDC_TAB_PURGE:
            if(!g_mode_purge){
                g_mode_purge=true;g_mode_create=false;
                show_create_controls(false);
                show_launch_controls(false);
                show_purge_controls(true);
                refresh_purge_saves();
                SetWindowTextW(g_status,L"");
                InvalidateRect(g_tab_launch,nullptr,TRUE);InvalidateRect(g_tab_purge,nullptr,TRUE);InvalidateRect(g_tab_create,nullptr,TRUE);
            }
            break;

        // ── Radio Manuel/Dossier ───────────────────────────────────────────
        case IDC_RADIO_MAN:
            g_folder_mode=false;
            ShowWindow(g_name_lbl,SW_SHOW);ShowWindow(g_name,SW_SHOW);
            ShowWindow(g_dir_lbl,SW_HIDE);ShowWindow(g_dir_edit,SW_HIDE);ShowWindow(g_dir_browse,SW_HIDE);
            ShowWindow(g_group_list,SW_HIDE);ShowWindow(g_rename_edit,SW_HIDE);ShowWindow(g_rename_lbl,SW_HIDE);
            for(int i=0;i<6;i++){ShowWindow(g_man_labels[i],SW_SHOW);ShowWindow(g_edits[i],SW_SHOW);ShowWindow(g_browses[i],SW_SHOW);}
            break;
        case IDC_RADIO_DIR:
            g_folder_mode=true;
            ShowWindow(g_name_lbl,SW_HIDE);ShowWindow(g_name,SW_HIDE);
            ShowWindow(g_dir_lbl,SW_SHOW);ShowWindow(g_dir_edit,SW_SHOW);ShowWindow(g_dir_browse,SW_SHOW);
            ShowWindow(g_group_list,SW_SHOW);ShowWindow(g_rename_edit,SW_SHOW);ShowWindow(g_rename_lbl,SW_SHOW);
            for(int i=0;i<6;i++){ShowWindow(g_man_labels[i],SW_HIDE);ShowWindow(g_edits[i],SW_HIDE);ShowWindow(g_browses[i],SW_HIDE);}
            break;

        // ── Dossier browse ─────────────────────────────────────────────────
        case IDC_DIR_BROWSE:{
            auto dir=browse_folder();
            if(!dir.empty()){SetWindowTextW(g_dir_edit,dir.c_str());scan_folder(dir);}
            break;
        }

        // ── Sauvegarder les textures ───────────────────────────────────────
        case IDC_SAVE_BTN:{
            if(g_folder_mode){
                // Sauvegarde de TOUS les groupes détectés
                if(g_groups.empty()){ui_status(L"Sélectionne un dossier d'abord",RGB(200,0,0));break;}
                int total=0,groups_ok=0;
                for(auto&g:g_groups){
                    std::wstring nm=g.savename;
                    if(nm.empty())nm=g.base;
                    std::wstring savedir=std::wstring(SAVE_DIR)+L"\\"+nm;
                    CreateDirectoryW(savedir.c_str(),nullptr);
                    int saved=0;
                    for(int i=0;i<6;i++){
                        if(g.paths[i].empty())continue;
                        auto d=fread_all(g.paths[i]);
                        if(!d.empty()&&fwrite_all(savedir+L"\\"+std::wstring(FACES[i],FACES[i]+2)+L".vtf",d))saved++;
                    }
                    if(saved>0)groups_ok++;
                    total+=saved;
                }
                ui_status(std::to_wstring(groups_ok)+L" sauvegarde(s) créée(s) ("+std::to_wstring(total)+L" VTF)",RGB(0,140,0));
            } else {
                auto vtfs=collect_vtfs();
                if(vtfs.empty()){ui_status(L"Aucune texture VTF sélectionnée",RGB(200,0,0));break;}
                wchar_t namebuf[MAX_PATH]={};GetWindowTextW(g_name,namebuf,MAX_PATH);
                if(!namebuf[0]){ui_status(L"Entre un nom",RGB(200,0,0));break;}
                std::wstring savedir=std::wstring(SAVE_DIR)+L"\\"+namebuf;
                CreateDirectoryW(savedir.c_str(),nullptr);
                int saved=0;
                for(auto&kv:vtfs){
                    std::wstring fn(kv.first.begin(),kv.first.end());
                    if(fwrite_all(savedir+L"\\"+fn+L".vtf",kv.second))saved++;
                }
                ui_status(std::wstring(L"✓ ")+std::to_wstring(saved)+L" textures sauvegardées dans: "+namebuf,RGB(0,140,0));
            }
            break;
        }

        // ── Lanceur ────────────────────────────────────────────────────────
        // ── Group list sélection ───────────────────────────────────────────
        case IDC_GROUP_LIST:
            if(HIWORD(wp)==LBN_SELCHANGE){
                int idx=(int)SendMessageW(g_group_list,LB_GETCURSEL,0,0);
                if(idx>=0&&idx<(int)g_groups.size()){
                    g_updating_rename=true;
                    SetWindowTextW(g_rename_edit,g_groups[idx].savename.c_str());
                    g_updating_rename=false;
                    auto pp=g_groups[idx].paths; // copie pour le thread
                    std::thread([pp]{std::wstring tmp[6];for(int i=0;i<6;i++)tmp[i]=pp[i];update_preview(tmp);}).detach();
                }
            }
            break;
        // ── Rename edit ────────────────────────────────────────────────────
        case IDC_RENAME_EDIT:
            if(HIWORD(wp)==EN_CHANGE&&!g_updating_rename){
                int idx=(int)SendMessageW(g_group_list,LB_GETCURSEL,0,0);
                if(idx>=0&&idx<(int)g_groups.size()){
                    wchar_t buf[MAX_PATH]={};GetWindowTextW(g_rename_edit,buf,MAX_PATH);
                    g_groups[idx].savename=buf;
                    // Mettre à jour le label dans la listbox
                    int cnt=0;for(int i=0;i<6;i++)if(!g_groups[idx].paths[i].empty())cnt++;
                    std::wstring lbl=g_groups[idx].savename+L"  ("+std::to_wstring(cnt)+L"/6 faces)";
                    SendMessageW(g_group_list,LB_DELETESTRING,idx,0);
                    SendMessageW(g_group_list,LB_INSERTSTRING,idx,(LPARAM)lbl.c_str());
                    g_updating_rename=true;SendMessageW(g_group_list,LB_SETCURSEL,idx,0);g_updating_rename=false;
                }
            }
            break;
        case IDC_REFRESH: refresh_saves(); break;
        case IDC_PURGE_REFRESH: refresh_purge_saves(); break;
        case IDC_PURGE_BROWSE:{
            wchar_t buf[MAX_PATH]={};
            BROWSEINFOW bi={};bi.hwndOwner=hw;bi.pszDisplayName=buf;
            bi.lpszTitle=L"Sélectionne le dossier skybox de l'addon";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST il=SHBrowseForFolderW(&bi);
            if(il){wchar_t p[MAX_PATH]={};SHGetPathFromIDListW(il,p);CoTaskMemFree(il);SetWindowTextW(g_purge_path,p);}
            break;
        }
        case IDC_PURGE_APPLY:{
            // Récupérer la save sélectionnée
            int idx=(int)SendMessageW(g_purge_list,LB_GETCURSEL,0,0);
            if(idx<0){ui_status(L"Sélectionne une sauvegarde",RGB(200,0,0));break;}
            int len=(int)SendMessageW(g_purge_list,LB_GETTEXTLEN,idx,0);
            std::wstring sname(len,L'\0');
            SendMessageW(g_purge_list,LB_GETTEXT,idx,(LPARAM)sname.data());
            std::wstring savedir=std::wstring(SAVE_DIR)+L"\\"+sname;
            // Skyname
            wchar_t sky[256]={};GetWindowTextW(g_purge_sky,sky,256);
            if(!sky[0]){ui_status(L"Entre un nom de skybox (ex: grimmnight)",RGB(200,0,0));break;}
            // Dossier destination
            wchar_t tpath[MAX_PATH]={};GetWindowTextW(g_purge_path,tpath,MAX_PATH);
            if(!tpath[0]){ui_status(L"Entre le dossier de destination",RGB(200,0,0));break;}
            // Créer le dossier (et parents) si besoin
            SHCreateDirectoryExW(nullptr,tpath,nullptr);
            // Copier et renommer chaque face
            int copied=0;
            for(int i=0;i<6;i++){
                std::wstring src=savedir+L"\\"+std::wstring(FACES[i],FACES[i]+2)+L".vtf";
                std::wstring dst=std::wstring(tpath)+L"\\"+std::wstring(sky)+std::wstring(FACES[i],FACES[i]+2)+L".vtf";
                auto data=fread_all(src);
                if(!data.empty()&&fwrite_all(dst,data))copied++;
            }
            if(copied==0)ui_status(L"Aucun VTF trouvé dans la sauvegarde",RGB(200,0,0));
            else ui_status(L"✓ "+std::to_wstring(copied)+L"/6 textures appliquées — relance GMod",RGB(0,140,0));
            break;
        }
        case IDC_LISTBOX:
            if(HIWORD(wp)==LBN_SELCHANGE){
                auto sav=selected_save();
                if(!sav.empty()){
                    if(g_watching)g_active_save=sav;
                    std::thread([sav]{update_preview_save(sav);}).detach();
                }
            }
            break;
        case IDC_PURGE_LIST:
            if(HIWORD(wp)==LBN_SELCHANGE){
                int idx=(int)SendMessageW(g_purge_list,LB_GETCURSEL,0,0);
                if(idx>=0){
                    int len=(int)SendMessageW(g_purge_list,LB_GETTEXTLEN,idx,0);
                    std::wstring sname(len,L'\0');
                    SendMessageW(g_purge_list,LB_GETTEXT,idx,(LPARAM)sname.data());
                    std::wstring sav=std::wstring(SAVE_DIR)+L"\\"+sname;
                    std::thread([sav]{update_preview_save(sav);}).detach();
                }
            }
            break;

        case IDC_WATCH:
            if(!g_watching){
                // START : lance le watcher + permanent automatique
                auto sav=selected_save();
                if(sav.empty()){ui_status(L"Sélectionne une sauvegarde",RGB(200,0,0));break;}
                if(vtfs_from_save(sav).empty()){ui_status(L"Aucun VTF trouvé dans la sauvegarde",RGB(200,0,0));break;}
                // Sauvegarder la dernière skybox utilisée
                {int idx=(int)SendMessageW(g_list,LB_GETCURSEL,0,0);
                wchar_t sname[MAX_PATH]={};SendMessageW(g_list,LB_GETTEXT,idx,(LPARAM)sname);
                HKEY hk2;
                if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,nullptr,0,KEY_SET_VALUE,nullptr,&hk2,nullptr)==ERROR_SUCCESS){
                    RegSetValueExW(hk2,L"LastSkybox",0,REG_SZ,(BYTE*)sname,(DWORD)((wcslen(sname)+1)*sizeof(wchar_t)));
                    RegCloseKey(hk2);
                }}
                g_active_save=sav;
                // Nettoyer tout verrou résiduel (DENY peut rester si app fermée sans STOP)
                {std::wstring dest=CACHE_DIR+L"\\map_pack.bsp";
                unlock_bsp();
                icacls(L"\""+dest+L"\" /remove:d Everyone");
                SetFileAttributesW(dest.c_str(),FILE_ATTRIBUTE_NORMAL);}
                g_permanent=false;
                g_perm_pending=true;
                // "En attente" seulement si pas de BSP en cache — sinon le watcher va mettre vert immédiatement
                if(find_cache_bsp().empty())PostMessageW(hw,WM_SETREADY,0,0);
                g_watching=true;SetWindowTextW(g_btn_watch,L"ARRÊTER");
                if(g_wthread.joinable()){g_watching=false;if(g_dir_handle!=INVALID_HANDLE_VALUE)CancelIoEx(g_dir_handle,nullptr);g_wthread.join();g_watching=true;}
                g_wthread=std::thread(watch_fn);
                std::thread(gmod_exit_watcher).detach();
                ui_status(L"Actif — skybox sera appliquée automatiquement",RGB(0,100,200));
            } else {
                // STOP : arrête le watcher + supprime le verrou
                g_watching=false;
                if(g_dir_handle!=INVALID_HANDLE_VALUE)CancelIoEx(g_dir_handle,nullptr);
                std::wstring dest=CACHE_DIR+L"\\map_pack.bsp";
                unlock_bsp();
                icacls(L"\""+dest+L"\" /remove:d Everyone");
                SetFileAttributesW(dest.c_str(),FILE_ATTRIBUTE_NORMAL);
                g_permanent=false;g_perm_pending=false;
                PostMessageW(hw,WM_SETREADY,2,0); // arrêté
                SetWindowTextW(g_btn_watch,L"APPLIQUER");
                ui_status(L"Arrêté — verrou retiré",RGB(0,140,0));
            }
            break;

        case IDC_LOCK:
            if(!g_locked){
                auto p=find_cache_bsp();if(p.empty()){ui_status(L"BSP introuvable",RGB(200,0,0));break;}
                lock_bsp(p);SetWindowTextW(g_btn_lock,L"UNLOCK");
                ui_status(L"Fichier verrouillé",RGB(0,140,0));
            } else {
                unlock_bsp();SetWindowTextW(g_btn_lock,L"LOCK");
                ui_status(L"Fichier déverrouillé",RGB(0,140,0));
            }
            break;

        case IDC_PERM:{
            std::wstring dest=std::wstring(CACHE_DIR)+L"\\map_pack.bsp";
            if(!g_permanent){
                // Si la skybox a déjà été appliquée : poser juste le DENY
                // Sinon : appliquer d'abord
                auto sav=selected_save();
                if(sav.empty()){ui_status(L"Sélectionne une sauvegarde",RGB(200,0,0));break;}
                EnableWindow(g_btn_perm,FALSE);
                ui_status(L"Application du PERMANENT…",RGB(0,100,200));
                std::thread([=](){
                    unlock_bsp();
                    icacls(L"\""+dest+L"\" /remove:d Everyone");
                    SetFileAttributesW(dest.c_str(),FILE_ATTRIBUTE_NORMAL);
                    auto bsp=fread_all(dest);
                    if(bsp.size()>=4&&!memcmp(bsp.data(),"VBSP",4)){
                        // BSP déjà en cache — appliquer et locker maintenant
                        auto vtfs=vtfs_from_save(sav);
                        if(!vtfs.empty()){
                            std::string sky=bsp_sky(bsp);
                            if(!sky.empty()){
                                ui_status(L"Application en cours…",RGB(0,100,200));
                                auto patched=bsp_apply(bsp,sky,vtfs);
                                fwrite_all(dest,patched);
                            }
                        }
                        icacls(L"\""+dest+L"\" /deny Everyone:(W,D,DC,WD)");
                        g_permanent=true;g_perm_pending=false;
                        PostMessageW(g_hwnd,WM_APP+3,1,0);
                        ui_status(L"✓ Skybox verrouillée — relance ou rejoins le serveur",RGB(0,140,0));
                    } else {
                        // BSP pas encore là — activer le flag, sera appliqué au prochain chargement
                        g_perm_pending=true;
                        PostMessageW(g_hwnd,WM_APP+3,1,0);
                        ui_status(L"En attente — sera appliqué au prochain chargement de map",RGB(0,100,200));
                    }
                }).detach();
            } else {
                unlock_bsp();
                icacls(L"\""+dest+L"\" /remove:d Everyone");
                SetFileAttributesW(dest.c_str(),FILE_ATTRIBUTE_NORMAL);
                g_permanent=false;g_perm_pending=false;
                SetWindowTextW(g_btn_perm,L"PERMANENT");
                EnableWindow(g_btn_perm,TRUE);
                ui_status(L"DEPERM — change la sauvegarde et relance AUTO-WATCH",RGB(0,100,200));
            }
            break;
        }
        }
        break;
    }

    case WM_SETREADY:{
        // wp=0: en attente (orange), 1: prêt (vert), 2: arrêté (gris)
        static const COLORREF COLS[3]={RGB(170,90,0),RGB(0,140,0),DK_BG};
        static const wchar_t*TXTS[3]={L"⏳  En attente du BSP…",L"✅  Tu peux restart !",L""};
        g_ready_col=COLS[wp];
        if(g_ready_br){DeleteObject(g_ready_br);g_ready_br=nullptr;}
        g_ready_br=CreateSolidBrush(g_ready_col);
        SetWindowTextW(g_ready_lbl,TXTS[wp]);
        InvalidateRect(g_ready_lbl,nullptr,TRUE);
        break;
    }
    case WM_SETSTATUS:{
        g_status_col=(COLORREF)wp;
        wchar_t*s=(wchar_t*)lp;
        SetWindowTextW(g_status,s);
        InvalidateRect(g_status,nullptr,TRUE);
        delete[]s;
        break;
    }
    case WM_UPDLOCK:
        SetWindowTextW(g_btn_lock,g_locked?L"UNLOCK":L"LOCK");
        break;
    case WM_APP+3: // permanent done
        EnableWindow(g_btn_perm,TRUE);
        SetWindowTextW(g_btn_perm,wp?L"DEPERM":L"PERMANENT");
        break;
    case WM_ERASEBKGND:{
        RECT rc;GetClientRect(hw,&rc);
        FillRect((HDC)wp,&rc,g_br_bg?g_br_bg:GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    case WM_CTLCOLOREDIT:{
        HDC hdc=(HDC)wp;
        SetTextColor(hdc,DK_TEXT);SetBkColor(hdc,DK_CTRL);
        return(LRESULT)(g_br_ctrl?g_br_ctrl:GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_CTLCOLORLISTBOX:{
        HDC hdc=(HDC)wp;
        SetTextColor(hdc,DK_TEXT);SetBkColor(hdc,DK_PANEL);
        return(LRESULT)(g_br_panel?g_br_panel:GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_CTLCOLORBTN:{
        HDC hdc=(HDC)wp;HWND hb=(HWND)lp;
        if(hb!=g_tab_launch&&hb!=g_tab_purge&&hb!=g_tab_create&&hb!=g_btn_settings){
            SetTextColor(hdc,DK_TEXT);SetBkColor(hdc,DK_PANEL);
            return(LRESULT)(g_br_panel?g_br_panel:GetSysColorBrush(COLOR_BTNFACE));
        }
        return DefWindowProcW(hw,msg,wp,lp);
    }
    case WM_CTLCOLORSTATIC:{
        HDC hdc=(HDC)wp;
        if((HWND)lp==g_ready_lbl){
            SetTextColor(hdc,RGB(255,255,255));
            SetBkColor(hdc,g_ready_col);
            SetBkMode(hdc,OPAQUE);
            return(LRESULT)(g_ready_br?g_ready_br:GetSysColorBrush(COLOR_BTNFACE));
        }
        if((HWND)lp==g_status)SetTextColor(hdc,g_status_col);
        else SetTextColor(hdc,DK_TEXT);
        SetBkMode(hdc,TRANSPARENT);
        return(LRESULT)(g_br_bg?g_br_bg:GetSysColorBrush(COLOR_BTNFACE));
    }
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT*di=(DRAWITEMSTRUCT*)lp;
        UINT id=di->CtlID;
        if(id!=IDC_TAB_LAUNCH&&id!=IDC_TAB_PURGE&&id!=IDC_TAB_CREATE&&id!=IDC_SETTINGS)break;
        HDC dc=di->hDC;RECT rc=di->rcItem;
        bool isTab=(id!=IDC_SETTINGS);
        bool active=(id==IDC_TAB_LAUNCH&&!g_mode_create&&!g_mode_purge)||
                    (id==IDC_TAB_PURGE&&g_mode_purge)||
                    (id==IDC_TAB_CREATE&&g_mode_create);
        bool hov=(di->hwndItem==g_hover_tab);
        // Couleur de fond interpolée
        float t=(float)g_hanim/8.f;
        auto lerp=[](int a,int b,float f)->int{return a+(int)((b-a)*f);};
        auto lerpc=[&](COLORREF a,COLORREF b)->COLORREF{
            return RGB(lerp(GetRValue(a),GetRValue(b),t),
                       lerp(GetGValue(a),GetGValue(b),t),
                       lerp(GetBValue(a),GetBValue(b),t));};
        COLORREF bgBase=active?DK_ACCENT:DK_PANEL;
        COLORREF bgHov =active?DK_ACCHV:DK_HOVER;
        COLORREF bg=hov?lerpc(bgBase,bgHov):bgBase;
        HBRUSH br=CreateSolidBrush(bg);FillRect(dc,&rc,br);DeleteObject(br);
        // Bordure du bas pour onglet actif (3px)
        if(isTab&&active){
            HPEN pen=CreatePen(PS_SOLID,3,DK_ACCHV);
            HPEN op=(HPEN)SelectObject(dc,pen);
            HBRUSH brnull=(HBRUSH)SelectObject(dc,GetStockObject(NULL_BRUSH));
            MoveToEx(dc,rc.left,rc.bottom-2,nullptr);LineTo(dc,rc.right,rc.bottom-2);
            SelectObject(dc,op);SelectObject(dc,brnull);DeleteObject(pen);
        }
        // Icône engrenage pour settings
        if(!isTab&&g_ico_gear){
            DrawIconEx(dc,rc.left+6,(rc.bottom-rc.top-16)/2,g_ico_gear,16,16,0,nullptr,DI_NORMAL);
        }
        // Texte
        SetTextColor(dc,DK_TEXT);SetBkMode(dc,TRANSPARENT);
        RECT trc=rc;
        if(!isTab&&g_ico_gear)trc.left+=26;
        HFONT hf=CreateFontW(14,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        HFONT of=(HFONT)SelectObject(dc,hf);
        wchar_t txt[64]={};GetWindowTextW(di->hwndItem,txt,64);
        DrawTextW(dc,txt,-1,&trc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,of);DeleteObject(hf);
        return TRUE;
    }
    case WM_MOUSEMOVE:{
        POINT pt={(short)LOWORD(lp),(short)HIWORD(lp)};
        HWND hov=ChildWindowFromPoint(hw,pt);
        bool isTab=(hov==g_tab_launch||hov==g_tab_purge||hov==g_tab_create||hov==g_btn_settings);
        HWND newtab=isTab?hov:nullptr;
        if(newtab!=g_hover_tab){
            HWND old=g_hover_tab;g_hover_tab=newtab;
            if(old)InvalidateRect(old,nullptr,TRUE);
            g_hanim_in=(newtab!=nullptr);
            SetTimer(hw,42,16,nullptr);
        }
        if(!g_tmouse){TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,hw,0};TrackMouseEvent(&tme);g_tmouse=true;}
        break;
    }
    case WM_MOUSELEAVE:{
        if(g_hover_tab){InvalidateRect(g_hover_tab,nullptr,TRUE);g_hover_tab=nullptr;}
        g_hanim_in=false;g_tmouse=false;SetTimer(hw,42,16,nullptr);
        break;
    }
    case WM_TIMER:
        if(wp==42){
            if(g_hanim_in)g_hanim=(g_hanim<8?g_hanim+1:8);else g_hanim=(g_hanim>0?g_hanim-1:0);
            HWND target=g_hover_tab?g_hover_tab:nullptr;
            if(target)InvalidateRect(target,nullptr,TRUE);
            if(g_hanim==0||g_hanim==8)KillTimer(hw,42);
        }
        break;
    case WM_FIRST_LAUNCH:{
        // Sélectionner et démarrer automatiquement la dernière skybox
        wchar_t last[MAX_PATH]={};
        {HKEY hk;
        if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,KEY_READ,&hk)==ERROR_SUCCESS){
            DWORD sz=sizeof(last);
            RegQueryValueExW(hk,L"LastSkybox",nullptr,nullptr,(BYTE*)last,&sz);
            RegCloseKey(hk);
        }}
        if(!last[0])break;
        int count=(int)SendMessageW(g_list,LB_GETCOUNT,0,0);
        for(int i=0;i<count;i++){
            wchar_t name[MAX_PATH]={};SendMessageW(g_list,LB_GETTEXT,i,(LPARAM)name);
            if(wcscmp(name,last)==0){
                SendMessageW(g_list,LB_SETCURSEL,i,0);
                auto sav=selected_save();
                if(!sav.empty())std::thread([sav]{update_preview_save(sav);}).detach();
                // Démarrer le watcher automatiquement
                PostMessageW(hw,WM_COMMAND,MAKEWPARAM(IDC_WATCH,BN_CLICKED),(LPARAM)g_btn_watch);
                break;
            }
        }
        break;
    }
    case WM_CLOSE:{
        // Minimiser dans le tray au lieu de quitter
        NOTIFYICONDATAW nid={sizeof(nid)};
        nid.hWnd=hw;nid.uID=IDI_TRAY;
        nid.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;
        nid.uCallbackMessage=WM_TRAY;
        nid.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));
        wcscpy_s(nid.szTip,L"Skybox — watcher actif");
        Shell_NotifyIconW(NIM_ADD,&nid);
        ShowWindow(hw,SW_HIDE);
        if(g_preview)ShowWindow(g_preview,SW_HIDE);
        return 0;
    }
    case WM_SIZE:
        if(wp==SIZE_MINIMIZED&&g_preview)ShowWindow(g_preview,SW_MINIMIZE);
        else if(wp==SIZE_RESTORED&&g_preview)ShowWindow(g_preview,SW_SHOWNOACTIVATE);
        break;
    case WM_TRAY:
        if(lp==WM_LBUTTONDBLCLK||lp==WM_LBUTTONUP){
            NOTIFYICONDATAW nid={sizeof(nid)};nid.hWnd=hw;nid.uID=IDI_TRAY;
            Shell_NotifyIconW(NIM_DELETE,&nid);
            ShowWindow(hw,SW_SHOW);SetForegroundWindow(hw);
            if(g_preview)ShowWindow(g_preview,SW_SHOWNOACTIVATE);
        }
        else if(lp==WM_RBUTTONUP){
            HMENU menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,1,L"Ouvrir");
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            AppendMenuW(menu,MF_STRING,2,L"Quitter");
            POINT pt;GetCursorPos(&pt);
            SetForegroundWindow(hw);
            int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,hw,nullptr);
            DestroyMenu(menu);
            if(cmd==1){
                NOTIFYICONDATAW nid={sizeof(nid)};nid.hWnd=hw;nid.uID=IDI_TRAY;
                Shell_NotifyIconW(NIM_DELETE,&nid);
                ShowWindow(hw,SW_SHOW);SetForegroundWindow(hw);
                if(g_preview)ShowWindow(g_preview,SW_SHOWNOACTIVATE);
            }
            else if(cmd==2){ DestroyWindow(hw); }
        }
        break;
    case WM_DESTROY:
        g_watching=false;
        if(g_dir_handle!=INVALID_HANDLE_VALUE)CancelIoEx(g_dir_handle,nullptr);
        unlock_bsp();
        {NOTIFYICONDATAW nid={sizeof(nid)};nid.hWnd=hw;nid.uID=IDI_TRAY;Shell_NotifyIconW(NIM_DELETE,&nid);}
        if(g_br_bg)DeleteObject(g_br_bg);
        if(g_br_panel)DeleteObject(g_br_panel);
        if(g_br_ctrl)DeleteObject(g_br_ctrl);
        if(g_ico_gear)DestroyIcon(g_ico_gear);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hw,msg,wp,lp);
}

// ── Fenêtre de setup premier lancement ────────────────────────────────────────
static LRESULT CALLBACK SetupWndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    static HWND eDir=nullptr;
    static bool success=false;
    switch(msg){
    case WM_CREATE:
        CreateWindowW(L"STATIC",
            L"Bienvenue ! Avant de commencer, indique où se trouve le dossier cache de GMod.\r\n\r\n"
            L"Comment le trouver :\r\n"
            L"  1. Ouvre Steam\r\n"
            L"  2. Clic droit sur Garry's Mod  →  Gérer  →  Parcourir les fichiers locaux\r\n"
            L"  3. Ouvre le dossier  garrysmod  puis  cache\r\n"
            L"  4. Copie le chemin depuis la barre d'adresse de l'Explorateur\r\n"
            L"  5. Colle-le dans le champ ci-dessous et clique Continuer",
            WS_CHILD|WS_VISIBLE|SS_LEFT,12,12,450,162,hw,nullptr,nullptr,nullptr);
        eDir=CreateWindowW(L"EDIT",CACHE_DIR.c_str(),
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            12,182,408,22,hw,(HMENU)1,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"...",WS_CHILD|WS_VISIBLE,424,182,30,22,hw,(HMENU)2,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"Continuer →",WS_CHILD|WS_VISIBLE,12,216,120,28,hw,(HMENU)3,nullptr,nullptr);
        break;
    case WM_COMMAND:
        if(LOWORD(wp)==2){
            wchar_t buf[MAX_PATH]={};
            BROWSEINFOW bi={};bi.hwndOwner=hw;bi.pszDisplayName=buf;
            bi.lpszTitle=L"Sélectionne le dossier cache de GMod (garrysmod\\cache)";
            bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST il=SHBrowseForFolderW(&bi);
            if(il){wchar_t p[MAX_PATH]={};SHGetPathFromIDListW(il,p);CoTaskMemFree(il);SetWindowTextW(eDir,p);}
        }
        if(LOWORD(wp)==3){
            wchar_t cdir[MAX_PATH]={};GetWindowTextW(eDir,cdir,MAX_PATH);
            DWORD attr=GetFileAttributesW(cdir);
            bool exists=(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY));
            std::wstring cd(cdir),cdl(cdir);
            std::transform(cdl.begin(),cdl.end(),cdl.begin(),::towlower);
            bool looks_ok=cdl.find(L"garrysmod\\cache")!=cdl.npos||cdl.find(L"garrysmod/cache")!=cdl.npos;
            if(!exists||!looks_ok){
                MessageBoxW(hw,
                    L"Chemin invalide.\n\n"
                    L"Le dossier doit exister et se terminer par :\\garrysmod\\cache\n\n"
                    L"Exemple :\n"
                    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\GarrysMod\\garrysmod\\cache",
                    L"Erreur",MB_OK|MB_ICONERROR);
                break;
            }
            CACHE_DIR=cd;
            HKEY hk;
            if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,nullptr,0,KEY_SET_VALUE,nullptr,&hk,nullptr)==ERROR_SUCCESS){
                RegSetValueExW(hk,L"CacheDir",0,REG_SZ,(BYTE*)cdir,(DWORD)((wcslen(cdir)+1)*sizeof(wchar_t)));
                RegCloseKey(hk);
            }
            success=true;
            DestroyWindow(hw);
        }
        break;
    case WM_CLOSE:
        if(MessageBoxW(hw,L"Tu n'as pas configuré le dossier cache.\nQuitter l'application ?",
            L"Skybox",MB_YESNO|MB_ICONQUESTION)==IDYES)
            DestroyWindow(hw);
        break;
    case WM_DESTROY:
        PostQuitMessage(success?1:0);
        break;
    default:return DefWindowProcW(hw,msg,wp,lp);
    }
    return 0;
}

// ── WinMain ────────────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR lpCmd,int){
    // ── Instance unique ────────────────────────────────────────────────────────
    HANDLE hMutex=CreateMutexW(nullptr,TRUE,L"SkyboxcppMutex_v1");
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        if(hMutex)CloseHandle(hMutex);
        return 0; // déjà en cours d'exécution
    }
    // ── Élévation admin ────────────────────────────────────────────────────────
    BOOL adm=FALSE;HANDLE tok;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&tok)){
        TOKEN_ELEVATION te;DWORD sz;
        if(GetTokenInformation(tok,TokenElevation,&te,sizeof(te),&sz))adm=te.TokenIsElevated;
        CloseHandle(tok);
    }
    if(!adm){
        if(hMutex){ReleaseMutex(hMutex);CloseHandle(hMutex);}
        wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);
        ShellExecuteW(nullptr,L"runas",path,lpCmd,nullptr,SW_SHOW);return 0;
    }
    // ── Démarrage en arrière-plan si /startup ──────────────────────────────────
    g_start_hidden=(lpCmd&&wcsstr(lpCmd,L"/startup")!=nullptr);
    CoInitialize(nullptr);
    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_WIN95_CLASSES};InitCommonControlsEx(&icc);
    // ── Setup premier lancement ────────────────────────────────────────────────
    // Essayer de détecter automatiquement via le registre Steam
    {bool configured=false;
    HKEY hk;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,KEY_READ,&hk)==ERROR_SUCCESS){
        configured=RegQueryValueExW(hk,L"CacheDir",nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;
        RegCloseKey(hk);
    }
    if(!configured){
        // Lire SteamPath depuis le registre
        wchar_t steamPath[MAX_PATH]={};
        HKEY hks;
        if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",0,KEY_READ,&hks)==ERROR_SUCCESS){
            DWORD sz=sizeof(steamPath);
            RegQueryValueExW(hks,L"SteamPath",nullptr,nullptr,(BYTE*)steamPath,&sz);
            RegCloseKey(hks);
        }
        if(steamPath[0]){
            std::wstring auto_cache=std::wstring(steamPath)+L"\\steamapps\\common\\GarrysMod\\garrysmod\\cache";
            // Remplacer les / par \ (Steam stocke avec /)
            std::replace(auto_cache.begin(),auto_cache.end(),L'/',L'\\');
            DWORD attr=GetFileAttributesW(auto_cache.c_str());
            if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY)){
                // Trouvé automatiquement — sauvegarder et skip le setup
                CACHE_DIR=auto_cache;
                HKEY hk2;
                if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,nullptr,0,KEY_SET_VALUE,nullptr,&hk2,nullptr)==ERROR_SUCCESS){
                    RegSetValueExW(hk2,L"CacheDir",0,REG_SZ,(BYTE*)auto_cache.c_str(),(DWORD)((auto_cache.size()+1)*sizeof(wchar_t)));
                    RegCloseKey(hk2);
                }
                configured=true;
            }
        }
    }
    if(!configured){
        WNDCLASSW wcs={};wcs.hInstance=hi;wcs.lpszClassName=L"SKYBOX_SETUP";
        wcs.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wcs.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wcs.lpfnWndProc=SetupWndProc;RegisterClassW(&wcs);
        HWND hs=CreateWindowW(L"SKYBOX_SETUP",L"Skyboxc++ — Configuration",
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,480,295,nullptr,nullptr,hi,nullptr);
        ShowWindow(hs,SW_SHOW);UpdateWindow(hs);
        MSG ms;while(GetMessageW(&ms,nullptr,0,0)){TranslateMessage(&ms);DispatchMessageW(&ms);}
        if(ms.wParam==0)return 0; // annulé
    }}
    // Charger le chemin cache sauvegardé
    {HKEY hk;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,KEY_READ,&hk)==ERROR_SUCCESS){
        wchar_t buf[MAX_PATH]={};DWORD sz=sizeof(buf);
        if(RegQueryValueExW(hk,L"CacheDir",nullptr,nullptr,(BYTE*)buf,&sz)==ERROR_SUCCESS&&buf[0])
            CACHE_DIR=buf;
        RegCloseKey(hk);
    }}
    // ── Classe panneau aperçu ──────────────────────────────────────────────────
    {WNDCLASSW wcp={};wcp.hInstance=hi;wcp.lpszClassName=L"PREVWND";
    wcp.lpfnWndProc=PreviewWndProc;wcp.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wcp.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wcp);}
    // ── Fenêtre de paramètres ───────────────────────────────────────────────────
    WNDCLASSW wc2={};
    wc2.hInstance=hi;wc2.lpszClassName=L"SI2_CFG";
    wc2.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc2.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc2.lpfnWndProc=[](HWND hw,UINT msg,WPARAM wp,LPARAM lp)->LRESULT{
        static HWND chk=nullptr,eCache=nullptr;
        switch(msg){
        case WM_CREATE:{
            HKEY hk;bool on=false;
            if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&hk)==ERROR_SUCCESS){
                on=RegQueryValueExW(hk,L"Skybox",nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;
                RegCloseKey(hk);
            }
            chk=CreateWindowW(L"BUTTON",L"Lancer au démarrage de Windows",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,10,12,280,20,hw,(HMENU)1,nullptr,nullptr);
            SendMessageW(chk,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);
            CreateWindowW(L"STATIC",L"Dossier cache GMod :",WS_CHILD|WS_VISIBLE,10,42,200,18,hw,nullptr,nullptr,nullptr);
            eCache=CreateWindowW(L"EDIT",CACHE_DIR.c_str(),WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,10,60,270,20,hw,(HMENU)3,nullptr,nullptr);
            CreateWindowW(L"BUTTON",L"...",WS_CHILD|WS_VISIBLE,284,60,30,20,hw,(HMENU)4,nullptr,nullptr);
            CreateWindowW(L"BUTTON",L"Enregistrer",WS_CHILD|WS_VISIBLE,10,92,100,26,hw,(HMENU)2,nullptr,nullptr);
            break;}
        case WM_COMMAND:
            if(LOWORD(wp)==4){
                // Parcourir dossier cache
                wchar_t buf[MAX_PATH]={};
                BROWSEINFOW bi={};bi.hwndOwner=hw;bi.pszDisplayName=buf;
                bi.lpszTitle=L"Sélectionne le dossier cache de GMod";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST il=SHBrowseForFolderW(&bi);
                if(il){wchar_t p[MAX_PATH]={};SHGetPathFromIDListW(il,p);CoTaskMemFree(il);SetWindowTextW(eCache,p);}
            }
            if(LOWORD(wp)==2){
                bool checked=SendMessageW(chk,BM_GETCHECK,0,0)==BST_CHECKED;
                HKEY hk;
                RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&hk);
                if(checked){
                    wchar_t path[MAX_PATH]={};GetModuleFileNameW(nullptr,path,MAX_PATH);
                    std::wstring val=std::wstring(L"\"")+path+L"\" /startup";
                    RegSetValueExW(hk,L"Skybox",0,REG_SZ,(BYTE*)val.c_str(),(DWORD)((val.size()+1)*sizeof(wchar_t)));
                }else{
                    RegDeleteValueW(hk,L"Skybox");
                }
                RegCloseKey(hk);
                // Valider et sauvegarder le chemin cache
                wchar_t cdir[MAX_PATH]={};GetWindowTextW(eCache,cdir,MAX_PATH);
                if(cdir[0]){
                    // Vérifier que le dossier existe
                    DWORD attr=GetFileAttributesW(cdir);
                    bool exists=(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY));
                    // Vérifier que le chemin contient garrysmod\cache
                    std::wstring cd(cdir);
                    std::wstring cdl=cd;std::transform(cdl.begin(),cdl.end(),cdl.begin(),::towlower);
                    bool looks_ok=cdl.find(L"garrysmod\\cache")!=std::wstring::npos||cdl.find(L"garrysmod/cache")!=std::wstring::npos;
                    if(!exists||!looks_ok){
                        MessageBoxW(hw,L"Chemin invalide.\nLe dossier doit exister et se terminer par ...\\garrysmod\\cache",L"Erreur",MB_OK|MB_ICONERROR);
                        break;
                    }
                    CACHE_DIR=cd;
                    HKEY hk2;
                    if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,nullptr,0,KEY_SET_VALUE,nullptr,&hk2,nullptr)==ERROR_SUCCESS){
                        RegSetValueExW(hk2,L"CacheDir",0,REG_SZ,(BYTE*)cdir,(DWORD)((wcslen(cdir)+1)*sizeof(wchar_t)));
                        RegCloseKey(hk2);
                    }
                }
                DestroyWindow(hw);
            }
            break;
        case WM_DESTROY:break;
        default:return DefWindowProcW(hw,msg,wp,lp);
        }
        return 0;
    };
    RegisterClassW(&wc2);
    // Charger le chemin cache sauvegardé
    {HKEY hk;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Skybox",0,KEY_READ,&hk)==ERROR_SUCCESS){
        wchar_t buf[MAX_PATH]={};DWORD sz=sizeof(buf);
        if(RegQueryValueExW(hk,L"CacheDir",nullptr,nullptr,(BYTE*)buf,&sz)==ERROR_SUCCESS&&buf[0])
            CACHE_DIR=buf;
        RegCloseKey(hk);
    }}
    // ── Fenêtre principale ─────────────────────────────────────────────────────
    HICON g_app_icon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));
    WNDCLASSEXW wc={sizeof(wc)};
    wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName=L"SI2";
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon  =g_app_icon;
    wc.hIconSm=g_app_icon;
    RegisterClassExW(&wc);
    HWND hw=CreateWindowW(L"SI2",L"Skyboxc++",
        WS_OVERLAPPEDWINDOW&~(WS_MAXIMIZEBOX|WS_THICKFRAME),
        CW_USEDEFAULT,CW_USEDEFAULT,665,380,nullptr,nullptr,hi,nullptr);
    SendMessageW(hw,WM_SETICON,ICON_BIG,(LPARAM)g_app_icon);
    SendMessageW(hw,WM_SETICON,ICON_SMALL,(LPARAM)g_app_icon);
    if(g_start_hidden){
        // Démarrage silencieux : fenêtre cachée, icône tray immédiate
        NOTIFYICONDATAW nid={sizeof(nid)};
        nid.hWnd=hw;nid.uID=IDI_TRAY;
        nid.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;
        nid.uCallbackMessage=WM_TRAY;
        nid.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));
        wcscpy_s(nid.szTip,L"Skyboxc++ — actif");
        Shell_NotifyIconW(NIM_ADD,&nid);
    } else {
        ShowWindow(hw,SW_SHOW);UpdateWindow(hw);
    }
    // Fenêtre aperçu séparée, placée à droite de la fenêtre principale
    {RECT mwr;GetWindowRect(hw,&mwr);
    g_preview=CreateWindowW(L"PREVWND",L"Aperçu skybox",
        WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX,
        mwr.right+8,mwr.top,680,540,hw,nullptr,hi,nullptr);
    if(!g_start_hidden){ShowWindow(g_preview,SW_SHOW);UpdateWindow(g_preview);}}
    MSG m;while(GetMessageW(&m,nullptr,0,0)){TranslateMessage(&m);DispatchMessageW(&m);}
    CoUninitialize();return(int)m.wParam;
}
