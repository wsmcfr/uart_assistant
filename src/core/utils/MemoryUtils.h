#ifndef MEMORYUTILS_H
#define MEMORYUTILS_H

namespace ComAssistant {

/**
 * @brief 进程内存整理辅助工具。
 *
 * 该工具不通过降低缓存上限、降低绘图质量或牺牲性能来“假装优化”，
 * 而是在确认大块绘图资源已经被主动释放之后，向操作系统请求回收
 * 当前进程中已经空闲的工作集页与堆段，帮助任务管理器里的内存占用
 * 更及时地下行到真实水平。
 */
class MemoryUtils
{
public:
    /**
     * @brief 请求操作系统回收当前进程已空闲的内存页。
     *
     * 主要流程：
     * 1. 让各线程局部构造的临时对象先析构；
     * 2. 尝试压缩默认进程堆，减少 clear 后仍保留的大块空闲段；
     * 3. 请求系统收缩当前进程工作集，把已不用的页尽快还给 Windows。
     *
     * 该操作只应在“刚刚释放完大块对象”的低频节点调用，例如：
     * - 关闭 OpenGL
     * - 清空绘图历史
     * - 关闭绘图窗口
     */
    static void trimProcessMemory();
};

} // namespace ComAssistant

#endif // MEMORYUTILS_H
