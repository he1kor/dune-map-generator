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
        auto path = std::filesystem::path(outPath);
        NFD_FreePathU8(outPath);
        return path;
    }
    else if (result == NFD_CANCEL){
        return std::unexpected(FileDialogStatus::CANCELED);
    }


    return std::unexpected(FileDialogStatus::ERROR);
}

FileDialog::FileDialog(){
    NFD_Init();
}

FileDialog::~FileDialog(){
    NFD_Quit();
}
