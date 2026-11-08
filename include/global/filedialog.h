#pragma once

#include <filesystem>
#include <expected>
#include <string>

enum class FileDialogStatus{
    CANCELED,
    ERROR
};

class FileDialog{
public:
    static FileDialog& instance(){
        static FileDialog instance;
        return instance;
    }
    std::expected<std::filesystem::path, FileDialogStatus> tryGetPathFromDialog();
    std::expected<std::filesystem::path, FileDialogStatus> tryGetSavePathFromDialog(const std::string& defaultName);
private:
    FileDialog();
    ~FileDialog();
};

inline FileDialog& fileDialog = FileDialog::instance();
