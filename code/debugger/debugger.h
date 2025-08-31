#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "nvvk/context.hpp"
namespace Play
{
struct Debugger
{
};

struct NsightDebugger : public Debugger
{
    static std::vector<nvvk::ExtensionInfo> initInjection();
    static void capture();
};
}


#endif // DEBUGGER_H