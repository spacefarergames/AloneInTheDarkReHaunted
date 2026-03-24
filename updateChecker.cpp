///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Background update checker — queries GitHub Releases API for new versions
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "updateChecker.h"
#include "consoleLog.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <thread>
#include <string>

#pragma comment(lib, "winhttp.lib")

// Console tags for the update checker subsystem
#define UPD_TAG   CON_MAKE_TAG("UPD")
#define UPD_OK    CON_MAKE_OK("UPD")
#define UPD_WARN  CON_MAKE_WARN("UPD")
#define UPD_ERR   CON_MAKE_ERR("UPD")

static const wchar_t* kGitHubHost = L"api.github.com";
static const wchar_t* kReleasePath = L"/repos/spacefarergames/AloneInTheDarkReHaunted/releases/latest";
static const char* kReleasesURL = "https://github.com/spacefarergames/AloneInTheDarkReHaunted/releases";

// Simple version comparison: returns true if remote > local.
// Expects dotted numeric strings like "1.0.4.2403".
static bool IsNewerVersion(const std::string& remote, const std::string& local)
{
    auto parseSegments = [](const std::string& v) -> std::vector<int>
    {
        std::vector<int> segs;
        size_t start = 0;
        while (start < v.size())
        {
            size_t dot = v.find('.', start);
            if (dot == std::string::npos) dot = v.size();
            segs.push_back(std::atoi(v.substr(start, dot - start).c_str()));
            start = dot + 1;
        }
        return segs;
    };

    auto r = parseSegments(remote);
    auto l = parseSegments(local);

    size_t len = (r.size() > l.size()) ? r.size() : l.size();
    for (size_t i = 0; i < len; i++)
    {
        int rv = (i < r.size()) ? r[i] : 0;
        int lv = (i < l.size()) ? l[i] : 0;
        if (rv > lv) return true;
        if (rv < lv) return false;
    }
    return false;
}

// Extract the value of "tag_name" from a raw JSON string.
// Avoids pulling in a full JSON parser for this single field.
static std::string ExtractTagName(const std::string& json)
{
    const char* key = "\"tag_name\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return {};

    pos = json.find('\"', pos + strlen(key));
    if (pos == std::string::npos) return {};
    pos++; // skip opening quote

    size_t end = json.find('\"', pos);
    if (end == std::string::npos) return {};

    std::string tag = json.substr(pos, end - pos);

    // Strip leading 'v' or 'V' if present (e.g. "v1.0.4.2403" -> "1.0.4.2403")
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
        tag = tag.substr(1);

    return tag;
}

static void CheckForUpdatesWorker()
{
    HINTERNET hSession = WinHttpOpen(L"FITD-UpdateChecker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        printf(UPD_WARN "Update check: could not create HTTP session" CON_RESET "\n");
        return;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, kGitHubHost,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
    {
        printf(UPD_WARN "Update check: could not connect to GitHub" CON_RESET "\n");
        WinHttpCloseHandle(hSession);
        return;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", kReleasePath,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        printf(UPD_WARN "Update check: could not create request" CON_RESET "\n");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL))
    {
        printf(UPD_WARN "Update check: request failed" CON_RESET "\n");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    std::string body;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::string chunk(bytesAvailable, '\0');
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, &chunk[0], bytesAvailable, &bytesRead);
        chunk.resize(bytesRead);
        body += chunk;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::string remoteVersion = ExtractTagName(body);
    if (remoteVersion.empty())
    {
        printf(UPD_WARN "Update check: could not parse latest release tag" CON_RESET "\n");
        return;
    }

    const std::string localVersion = REHAUNTED_VERSION;

    if (IsNewerVersion(remoteVersion, localVersion))
    {
        printf(UPD_OK "A new version of Alone In The Dark Re-Haunted has been released! "
            "(%s -> %s)\n", localVersion.c_str(), remoteVersion.c_str());
        printf(UPD_TAG "Opening releases page...\n");

        ShellExecuteA(NULL, "open", kReleasesURL, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        printf(UPD_TAG "You are running the latest version (%s)\n", localVersion.c_str());
    }
}

void CheckForUpdatesAsync()
{
    std::thread(CheckForUpdatesWorker).detach();
}

#else
// Non-Windows: update checking not implemented
void CheckForUpdatesAsync() {}
#endif
