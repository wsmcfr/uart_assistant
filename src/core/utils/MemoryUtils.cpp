#include "MemoryUtils.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace ComAssistant {

void MemoryUtils::trimProcessMemory()
{
#ifdef Q_OS_WIN
    /*
     * 先压缩默认堆，尽量把 clear/swap 后形成的空闲段合并出去。
     * HeapCompact 返回值不是本函数成功与否的必要条件，因此这里不强依赖它。
     */
    HANDLE processHeap = GetProcessHeap();
    if (processHeap != nullptr) {
        HeapCompact(processHeap, 0);
    }

    /*
     * 把当前进程工作集收缩到系统建议大小。
     * EmptyWorkingSet 对“对象尚未释放”的内存无效，
     * 这里只服务于“对象已经销毁，但任务管理器暂时还没回落”的场景。
     */
    HANDLE currentProcess = GetCurrentProcess();
    if (currentProcess != nullptr) {
        EmptyWorkingSet(currentProcess);
    }
#endif
}

} // namespace ComAssistant
