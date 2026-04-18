#pragma once

// FileXfer — receiver for the folder-push protocol:
//   char_begin → (file → chunk* → file_end)* → char_end
// Decodes base64 chunks to LittleFS under /characters/<name>/.

#include <Arduino.h>
#include <ArduinoJson.h>

class FileXfer {
public:
    static FileXfer* getInstance();

    // Inspect a parsed JSON doc; if it matches a file-push command, execute
    // it and return true (caller should stop further dispatch). Returns false
    // if the doc isn't one of our commands.
    bool tryHandle(JsonDocument& doc);

    bool isActive() const { return _active; }
    const char* currentName() const { return _charName; }
    uint32_t bytesWritten() const { return _totalWritten; }

private:
    FileXfer();
    static FileXfer* _instance;

    bool _active;
    bool _fileOpen;
    char _charName[32];
    uint32_t _expectedTotal;   // char_begin.total
    uint32_t _totalWritten;
    uint32_t _fileExpected;
    uint32_t _fileWritten;
    char _filePath[64];
    void* _filePtr;            // opaque File handle (File is a value type)

    void _wipeCharactersDir();
    uint32_t _charactersSize();
    bool _pathSafe(const char* path) const;
};
