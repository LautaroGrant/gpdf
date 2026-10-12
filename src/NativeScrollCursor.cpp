#include "NativeScrollCursor.h"
#include <QPixmap>
#include <array>

#ifdef Q_OS_WIN
HCURSOR NativeScrollCursor::handle(Direction direction) {
    // Windows' own vertical reader-mode cursors, loaded from the system module.
    // They are shared resources; do not destroy the cursor handles.
    static HMODULE module=LoadLibraryExW(L"user32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    static const std::array<HCURSOR,3> handles={
        module ? LoadCursorW(module,MAKEINTRESOURCEW(32655)) : nullptr,
        module ? LoadCursorW(module,MAKEINTRESOURCEW(32652)) : nullptr,
        module ? LoadCursorW(module,MAKEINTRESOURCEW(32656)) : nullptr
    };
    return handles.at(int(direction)+1);
}
#endif
QCursor NativeScrollCursor::cursor(Direction direction) {
#ifdef Q_OS_WIN
    static const std::array<QCursor,3> cursors=[] {
        std::array<QCursor,3> result;
        for(int index=0;index<3;++index) {
            auto native=handle(Direction(index-1));
            auto image=native ? QImage::fromHICON(reinterpret_cast<HICON>(native)) : QImage();
            ICONINFO info{};
            if(!image.isNull() && GetIconInfo(native,&info)) {
                result[index]=QCursor(QPixmap::fromImage(image),int(info.xHotspot),int(info.yHotspot));
                if(info.hbmColor) DeleteObject(info.hbmColor);
                if(info.hbmMask) DeleteObject(info.hbmMask);
            } else result[index]=QCursor(Qt::SizeVerCursor);
        }
        return result;
    }();
    return cursors.at(int(direction)+1);
#else
    Q_UNUSED(direction);
    return QCursor(Qt::SizeVerCursor);
#endif
}
