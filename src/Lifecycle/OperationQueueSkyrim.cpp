#include "PCH.h"

#include "Lifecycle/OperationQueueSkyrim.h"

namespace whereabouts
{
    OperationQueue::Submitter MakeGameTaskSubmitter()
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return {};
        return [tasks](OperationQueue::Task task) { tasks->AddTask(std::move(task)); };
    }

    OperationQueue::Submitter MakeUiTaskSubmitter()
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return {};
        return [tasks](OperationQueue::Task task) { tasks->AddUITask(std::move(task)); };
    }
}
