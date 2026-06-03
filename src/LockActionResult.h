#pragma once

enum class LockActionResult
{
    Success,
    UnknownAction,
    AccessDenied,
    Failed,
    Busy        ///< Queue full — another action is already pending. HTTP 429.
};