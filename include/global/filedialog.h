#pragma once

#include <filesystem>
#include <expected>

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
private:
    FileDialog();
    ~FileDialog();
};

inline FileDialog& fileDialog = FileDialog::instance();