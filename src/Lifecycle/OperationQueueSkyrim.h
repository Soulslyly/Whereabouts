#pragma once

#include "Lifecycle/OperationQueue.h"

namespace whereabouts
{
    [[nodiscard]] OperationQueue::Submitter MakeGameTaskSubmitter();
    [[nodiscard]] OperationQueue::Submitter MakeUiTaskSubmitter();
}
