///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
//
// Job System / Multithreading Infrastructure
// Author: Jake Jackson (jake@spacefarergames.com)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <map>
#include <deque>

///////////////////////////////////////////////////////////////////////////////
// Job System - Thread Pool based asynchronous task execution
///////////////////////////////////////////////////////////////////////////////

// Handle to track job completion and status
using JobHandle = uint64_t;
const JobHandle INVALID_JOB_HANDLE = 0;

class JobSystem
{
public:
    enum class Priority
    {
        IMMEDIATE = 0,  // Execute ASAP (high priority)
        HIGH = 1,       // High priority (asset loading)
        NORMAL = 2,     // Normal priority (background tasks)
        LOW = 3         // Low priority (optimization tasks)
    };

    enum class JobType
    {
        GENERIC,
        LOAD_PAK,
        LOAD_HD_BACKGROUND,
        DECOMPRESS_PAK,
        LOAD_VOC,
        DECODE_VOC,
        PARSE_ROOM_DATA,
        PARSE_ANIMATION,
        BUILD_TEXTURE_ATLAS,
        CUSTOM
    };

    // Job callback function types
    using JobFunction = std::function<void()>;
    using CompleteCallback = std::function<void()>;

    struct Job
    {
        JobHandle handle;
        JobType type;
        Priority priority;
        JobFunction work;
        CompleteCallback onComplete;
        std::vector<JobHandle> dependencies;  // Jobs this depends on
        bool isComplete;
        
        Job() 
            : handle(0), type(JobType::GENERIC), priority(Priority::NORMAL),
              isComplete(false)
        {
        }
    };

    // Singleton access
    static JobSystem& instance();

    // Initialization and shutdown
    void init(int numIOThreads = 2, int numWorkerThreads = -1);
    void shutdown();

    // Job scheduling
    JobHandle scheduleJob(
        JobType type,
        Priority priority,
        JobFunction work,
        CompleteCallback onComplete = nullptr
    );

    // Job scheduling with dependencies
    JobHandle scheduleJobWithDependencies(
        JobType type,
        Priority priority,
        JobFunction work,
        const std::vector<JobHandle>& dependencies,
        CompleteCallback onComplete = nullptr
    );

    // Wait for job completion
    void waitForJob(JobHandle handle);
    void waitForAllJobs();

    // Query job status
    bool isJobComplete(JobHandle handle) const;
    int getCompletedJobCount() const;
    int getPendingJobCount() const;

    // Process callbacks from main thread (call once per frame)
    void processPendingCallbacks();

    // Get statistics
    int getNumIOThreads() const { return m_numIOThreads; }
    int getNumWorkerThreads() const { return m_numWorkerThreads; }
    bool isInitialized() const { return m_initialized; }

    // For debugging
    void dumpStats() const;

private:
    JobSystem();
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Thread pool worker
    void workerThreadMain(int threadType);  // 0=IO, 1+=worker

    // Job queue management
    void enqueueJob(const Job& job);
    bool dequeueJob(Job& outJob, int threadType);

    // Member variables
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<uint64_t> m_nextJobHandle{1};

    int m_numIOThreads = 0;
    int m_numWorkerThreads = 0;

    // Job queues (one per priority level)
    struct JobQueue
    {
        std::deque<Job> jobs;
        mutable std::mutex mutex;
    };
    
    std::vector<JobQueue> m_jobQueues;  // One per priority level (4 levels)
    std::vector<Job> m_completedJobs;   // Jobs that finished this frame
    mutable std::mutex m_completedMutex;

    // Thread management
    std::vector<std::thread> m_ioThreads;
    std::vector<std::thread> m_workerThreads;
    std::condition_variable m_jobAvailable;

    // Job completion tracking
    mutable std::mutex m_jobsMutex;
    std::map<JobHandle, std::shared_ptr<Job>> m_allJobs;
    std::condition_variable m_jobComplete;
};

///////////////////////////////////////////////////////////////////////////////
// RAII Job completion waiter
///////////////////////////////////////////////////////////////////////////////

class JobWaiter
{
public:
    JobWaiter(JobHandle handle)
        : m_handle(handle)
    {
    }

    ~JobWaiter()
    {
        JobSystem::instance().waitForJob(m_handle);
    }

    bool isComplete() const
    {
        return JobSystem::instance().isJobComplete(m_handle);
    }

    void wait()
    {
        JobSystem::instance().waitForJob(m_handle);
    }

private:
    JobHandle m_handle;
};

