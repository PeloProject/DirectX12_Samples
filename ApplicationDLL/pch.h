// pch.h: プリコンパイル済みヘッダー ファイルです。
// 次のファイルは、その後のビルドのビルド パフォーマンスを向上させるため 1 回だけコンパイルされます。
// コード補完や多くのコード参照機能などの IntelliSense パフォーマンスにも影響します。
// ただし、ここに一覧表示されているファイルは、ビルド間でいずれかが更新されると、すべてが再コンパイルされます。
// 頻繁に更新するファイルをここに追加しないでください。追加すると、パフォーマンス上の利点がなくなります。

#ifndef PCH_H
#define PCH_H

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// プリコンパイルするヘッダーをここに追加します
#include "framework.h"
#include <vector>


// @brief コンソール画面にフォーマット付き文字列を出力する関数  
inline void DebugOutputFormatString(const wchar_t* type, const char* format, ...)  
{  
#ifdef _DEBUG  
   char buffer[256];  
   va_list args;  
   va_start(args, format);  
   vsprintf_s(buffer, sizeof(buffer), format, args);  
   va_end(args);  

   // Convert char* to WCHAR*  
   wchar_t wbuffer[256];  
   MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, 256);  

   OutputDebugString(type);  
   OutputDebugString(wbuffer);  
   OutputDebugString(L"\n");  
#endif  
}  

#if _DEBUG  
#define LOG_DEBUG(str,...) DebugOutputFormatString(L"[DEBUG] ", str, __VA_ARGS__);
#define LOG_WARNING(str,...) DebugOutputFormatString(L"[WARNING] ", str, __VA_ARGS__);  
#else  
#define LOG_DEBUG(str)  
#define LOG_WARNING(str)
#endif


#endif //PCH_H
