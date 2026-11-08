#include "filedialog.h"

#include <nfd.h>

std::expected<std::filesystem::path, FileDialogStatus> FileDialog::tryGetPathFromDialog(){

    nfdu8char_t* outPath;
    nfdu8filteritem_t filters[1] = { { "Template config file", "json" }};

    nfdopendialogu8args_t args = {0};
    args.filterList = filters;
    args.filterCount = 1;

    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
    if (result == NFD_OKAY){
        auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(outPath));
        NFD_FreePathU8(outPath);
        return path;
    }
    else if (result == NFD_CANCEL){
        return std::unexpected(FileDialogStatus::CANCELED);
    }


    return std::unexpected(FileDialogStatus::ERROR);
}

std::expected<std::filesystem::path, FileDialogStatus> FileDialog::tryGetSavePathFromDialog(const std::string& defaultName) {
    nfdu8char_t* outPath = nullptr;
    nfdu8filteritem_t filters[] = {{"Template config file", "json"}};
    nfdsavedialogu8args_t args = {};
    args.filterList = filters;
    args.filterCount = 1;
    args.defaultName = defaultName.c_str();
    const auto result = NFD_SaveDialogU8_With(&outPath, &args);
    if (result == NFD_OKAY) {
        auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(outPath));
        NFD_FreePathU8(outPath);
        return path;
    }
    return std::unexpected(result == NFD_CANCEL ? FileDialogStatus::CANCELED : FileDialogStatus::ERROR);
}

FileDialog::FileDialog(){
    NFD_Init();
}

FileDialog::~FileDialog(){
    NFD_Quit();
}
