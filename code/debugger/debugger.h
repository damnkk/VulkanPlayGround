#ifndef DEBUGGER_H
#define DEBUGGER_H
namespace Play
{
struct Debugger
{
};

struct NsightDebugger : public Debugger
{
    static bool initInjection();
    static void capture();
};
}


#endif // DEBUGGER_H