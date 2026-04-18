#include "FileXfer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <string.h>
#include "BuddyProtocol.h"
#include "BuddyStats.h"
#include "EpaperGifRenderer.h"
#include "../Logger.h"

static const char* TAG = "Xfer";
FileXfer* FileXfer::_instance = nullptr;

// File handle must live across chunks. Use a file-scope variable and treat
// the _filePtr in the class as an "is it open" marker — File is a value type
// in the Arduino LittleFS API.
static File g_file;

FileXfer::FileXfer()
    : _active(false), _fileOpen(false), _expectedTotal(0),
      _totalWritten(0), _fileExpected(0), _fileWritten(0),
      _filePtr(nullptr) {
    _charName[0] = 0;
    _filePath[0] = 0;
}

FileXfer* FileXfer::getInstance() {
    if (!_instance) _instance = new FileXfer();
    return _instance;
}

bool FileXfer::_pathSafe(const char* path) const {
    if (!path || !path[0]) return false;
    if (path[0] == '/') return false;
    if (strstr(path, "..")) return false;
    return true;
}

uint32_t FileXfer::_charactersSize() {
    uint32_t total = 0;
    File root = LittleFS.open("/characters");
    if (!root || !root.isDirectory()) return 0;
    File f = root.openNextFile();
    while (f) {
        if (f.isDirectory()) {
            // Recurse 1 level — characters/<name>/*
            File sub = f;
            File ff = sub.openNextFile();
            while (ff) {
                total += ff.size();
                ff = sub.openNextFile();
            }
        } else {
            total += f.size();
        }
        f = root.openNextFile();
    }
    return total;
}

void FileXfer::_wipeCharactersDir() {
    File root = LittleFS.open("/characters");
    if (!root || !root.isDirectory()) return;
    // Collect names first to avoid mutating while iterating.
    char names[16][32];
    uint8_t n = 0;
    File f = root.openNextFile();
    while (f && n < 16) {
        const char* fn = f.name();
        strncpy(names[n], fn, 31);
        names[n][31] = 0;
        n++;
        f = root.openNextFile();
    }
    for (uint8_t i = 0; i < n; i++) {
        char path[48];
        snprintf(path, sizeof(path), "/characters/%s", names[i]);
        // Subdir: wipe contents then rmdir
        File sd = LittleFS.open(path);
        if (sd && sd.isDirectory()) {
            File sub = sd.openNextFile();
            while (sub) {
                char subpath[96];
                snprintf(subpath, sizeof(subpath), "%s/%s", path, sub.name());
                LittleFS.remove(subpath);
                sub = sd.openNextFile();
            }
            LittleFS.rmdir(path);
        } else {
            LittleFS.remove(path);
        }
    }
}

bool FileXfer::tryHandle(JsonDocument& doc) {
    const char* cmd = doc["cmd"];
    if (!cmd) return false;

    BuddyProtocol* proto = BuddyProtocol::getInstance();

    if (strcmp(cmd, "char_begin") == 0) {
        const char* name = doc["name"] | "pet";
        _expectedTotal = (uint32_t)(doc["total"] | 0);

        if (!_pathSafe(name)) {
            proto->sendAck("char_begin", false, 0, "bad name");
            return true;
        }

        uint32_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        uint32_t reclaimable = _charactersSize();
        uint32_t available = freeBytes + reclaimable;

        if (_expectedTotal > 0 && _expectedTotal + 4096 > available) {
            char err[64];
            snprintf(err, sizeof(err), "need %u have %u",
                     (unsigned)_expectedTotal, (unsigned)available);
            proto->sendAck("char_begin", false, 0, err);
            return true;
        }

        strncpy(_charName, name, sizeof(_charName) - 1);
        _charName[sizeof(_charName) - 1] = 0;

        // Wipe existing /characters/*
        _wipeCharactersDir();
        LittleFS.mkdir("/characters");
        char dir[64];
        snprintf(dir, sizeof(dir), "/characters/%s", _charName);
        LittleFS.mkdir(dir);

        _active = true;
        _totalWritten = 0;
        LOG_INFO(TAG, "char_begin: %s (total=%u)", _charName, _expectedTotal);
        proto->sendAck("char_begin", true);
        return true;
    }

    if (strcmp(cmd, "file") == 0) {
        if (!_active) { proto->sendAck("file", false, 0, "no char_begin"); return true; }
        const char* path = doc["path"] | "";
        _fileExpected = (uint32_t)(doc["size"] | 0);
        _fileWritten  = 0;

        if (!_pathSafe(path)) {
            proto->sendAck("file", false, 0, "bad path");
            return true;
        }

        snprintf(_filePath, sizeof(_filePath), "/characters/%s/%s",
                 _charName, path);

        if (g_file) g_file.close();
        g_file = LittleFS.open(_filePath, "w");
        _fileOpen = (bool)g_file;
        LOG_INFO(TAG, "file: %s (size=%u)", _filePath, _fileExpected);
        proto->sendAck("file", _fileOpen);
        return true;
    }

    if (strcmp(cmd, "chunk") == 0) {
        if (!_active || !_fileOpen) { proto->sendAck("chunk", false); return true; }
        const char* b64 = doc["d"] | "";
        size_t inLen = strlen(b64);
        uint8_t buf[300];
        size_t outLen = 0;
        int rc = mbedtls_base64_decode(buf, sizeof(buf), &outLen,
                                       (const uint8_t*)b64, inLen);
        if (rc != 0) {
            proto->sendAck("chunk", false, _fileWritten, "b64 decode");
            return true;
        }
        size_t wrote = g_file.write(buf, outLen);
        if (wrote != outLen) {
            proto->sendAck("chunk", false, _fileWritten, "fs write");
            return true;
        }
        _fileWritten  += outLen;
        _totalWritten += outLen;
        proto->sendAck("chunk", true, _fileWritten);
        return true;
    }

    if (strcmp(cmd, "file_end") == 0) {
        bool ok = _fileOpen && (_fileExpected == 0 || _fileWritten == _fileExpected);
        if (g_file) g_file.close();
        _fileOpen = false;
        LOG_INFO(TAG, "file_end: %s %u/%u %s", _filePath,
                 _fileWritten, _fileExpected, ok ? "ok" : "size mismatch");
        proto->sendAck("file_end", ok, _fileWritten);
        return true;
    }

    if (strcmp(cmd, "char_end") == 0) {
        _active = false;
        if (g_file) g_file.close();
        _fileOpen = false;

        // Persist name so next boot loads this character.
        BuddyStats::getInstance()->setCharacterName(_charName);
        LOG_INFO(TAG, "char_end: %s total=%u", _charName, _totalWritten);

        // Hot-reload the GIF pack so the new character takes effect without
        // a reboot. Returns false if the dropped folder wasn't a valid pack
        // (no manifest.json) — in which case we ack ok=false so Claude
        // Desktop knows the push "arrived" but wasn't a usable pack.
        bool reloadOk = EpaperGifRenderer::getInstance()->reload();
        if (!reloadOk) {
            LOG_WARNING(TAG, "Pack %s has no valid manifest.json — ignoring",
                        _charName);
        }
        proto->sendAck("char_end", reloadOk, _totalWritten,
                       reloadOk ? nullptr : "no manifest.json");
        return true;
    }

    return false;
}
