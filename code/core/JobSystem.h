#ifndef JOBSYSTEM_H
#define JOBSYSTEM_H
#include "nvutils/parallel_work.hpp"
namespace Play
{
namespace JobSystem
{

template <typename F>
void detach(F task)
{
    nvutils::get_thread_pool().detach_task(task);
}

} // namespace JobSystem

} // namespace Play

#endif // JOBSYSTEM_H
