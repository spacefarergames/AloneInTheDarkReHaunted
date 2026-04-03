///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
//
// Job System Implementation - Thread Pool and Job Queue Management
// Author: Jake Jackson (jake@spacefarergames.com)
///////////////////////////////////////////////////////////////////////////////

#include "jobSystem.h"
#include <algorithm>
#include <iostream>

// Get number of CPU cores (portable implementation)
static int getCPUCoreCount()
{
    int count = std::thread::hardware_concurrency();
    return (count > 0) ? count : 4;  // Fallback to 4 if detection fails
}

///////////////////////////////////////////////////////////////////////////////
// JobSystem Singleton
///////////////////////////////////////////////////////////////////////////////

JobSystem& JobSystem::instance()
{
    static JobSystem s_instance;
    return s_instance;
}

JobSystem::JobSystem()
    : m_jobQueues(4)  // One queue per priority level
{
}

JobSystem::~JobSystem()
{
    if (m_running)
    {
        shutdown();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Initialization and Shutdown
///////////////////////////////////////////////////////////////////////////////

void JobSystem::init(int numIOThreads, int numWorkerThreads)
{
    if (m_initialized)
    {
        return;  // Already initialized
    }

    // Determine number of threads
    m_numIOThreads = (numIOThreads > 0) ? numIOThreads : 2;
    m_numWorkerThreads = (numWorkerThreads > 0) ? numWorkerThreads : 
                         std::max(1, getCPUCoreCount() - 1);

    m_running = true;
    m_initialized = true;

    // Create I/O threads (thread type 0)
    for (int i = 0; i < m_numIOThreads; ++i)
    {
        m_ioThreads.emplace_back(&JobSystem::workerThreadMain, this, 0);
    }

    // Create worker threads (thread type 1+)
    for (int i = 0; i < m_numWorkerThreads; ++i)
    {
        m_workerThreads.emplace_back(&JobSystem::workerThreadMain, this, 1);
    }
}

void JobSystem::shutdown()
{
    if (!m_running)
    {
        return;  // Already shut down
    }

    // Signal all threads to stop
    m_running = false;
    m_jobAvailable.notify_all();

    // Join all I/O threads
    for (auto& thread : m_ioThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    // Join all worker threads
    for (auto& thread : m_workerThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    // Clear all jobs
    {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        m_allJobs.clear();
    }

    for (auto& queue : m_jobQueues)
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        queue.jobs.clear();
    }

    m_ioThreads.clear();
    m_workerThreads.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Worker Thread Main
///////////////////////////////////////////////////////////////////////////////

void JobSystem::workerThreadMain(int threadType)
{
    while (m_running)
    {
        Job job;

        // Try to dequeue a job
        if (!dequeueJob(job, threadType))
        {
            // No job available, wait for notification
            std::unique_lock<std::mutex> lock(m_jobQueues[0].mutex);
            m_jobAvailable.wait_for(lock, std::chrono::milliseconds(100),
                [this] { return !m_running; });
            continue;
        }

        // Execute the job
        if (job.work)
        {
            job.work();
        }

        // Mark job as complete
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            auto it = m_allJobs.find(job.handle);
            if (it != m_allJobs.end())
            {
                it->second->isComplete = true;
            }
        }

        // Queue completion callback to be processed on main thread
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedJobs.push_back(job);
        }

        // Notify any threads waiting on this job
        m_jobComplete.notify_all();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Job Queue Management
///////////////////////////////////////////////////////////////////////////////

void JobSystem::enqueueJob(const Job& job)
{
    // Store job in global tracking map
    {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        m_allJobs[job.handle] = std::make_shared<Job>(job);
    }

    // Add to appropriate priority queue
    {
        int queueIndex = static_cast<int>(job.priority);
        std::lock_guard<std::mutex> lock(m_jobQueues[queueIndex].mutex);
        m_jobQueues[queueIndex].jobs.push_back(job);
    }

    // Notify a waiting thread
    m_jobAvailable.notify_one();
}

bool JobSystem::dequeueJob(Job& outJob, int threadType)
{
    // Try to get a job from any priority queue (highest priority first)
    for (int queueIndex = 0; queueIndex < 4; ++queueIndex)
    {
        std::lock_guard<std::mutex> lock(m_jobQueues[queueIndex].mutex);
        if (!m_jobQueues[queueIndex].jobs.empty())
        {
            outJob = m_jobQueues[queueIndex].jobs.front();
            m_jobQueues[queueIndex].jobs.pop_front();
            return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Job Scheduling
///////////////////////////////////////////////////////////////////////////////

JobHandle JobSystem::scheduleJob(
    JobType type,
    Priority priority,
    JobFunction work,
    CompleteCallback onComplete)
{
    if (!m_initialized)
    {
        // If not initialized, execute synchronously
        if (work)
        {
            work();
        }
        if (onComplete)
        {
            onComplete();
        }
        return INVALID_JOB_HANDLE;
    }

    Job job;
    job.handle = m_nextJobHandle++;
    job.type = type;
    job.priority = priority;
    job.work = work;
    job.onComplete = onComplete;
    job.isComplete = false;

    enqueueJob(job);
    return job.handle;
}

JobHandle JobSystem::scheduleJobWithDependencies(
    JobType type,
    Priority priority,
    JobFunction work,
    const std::vector<JobHandle>& dependencies,
    CompleteCallback onComplete)
{
    if (!m_initialized)
    {
        // If not initialized, execute synchronously
        if (work)
        {
            work();
        }
        if (onComplete)
        {
            onComplete();
        }
        return INVALID_JOB_HANDLE;
    }

    // Wrap the work function to wait for dependencies
    JobFunction wrappedWork = [this, work, dependencies]() {
        // Wait for all dependencies to complete
        for (JobHandle dep : dependencies)
        {
            waitForJob(dep);
        }
        // Execute the actual work
        if (work)
        {
            work();
        }
    };

    Job job;
    job.handle = m_nextJobHandle++;
    job.type = type;
    job.priority = priority;
    job.work = wrappedWork;
    job.onComplete = onComplete;
    job.isComplete = false;
    job.dependencies = dependencies;

    enqueueJob(job);
    return job.handle;
}

///////////////////////////////////////////////////////////////////////////////
// Job Completion and Waiting
///////////////////////////////////////////////////////////////////////////////

void JobSystem::waitForJob(JobHandle handle)
{
    if (handle == INVALID_JOB_HANDLE)
    {
        return;
    }

    std::unique_lock<std::mutex> lock(m_jobsMutex);
    
    auto it = m_allJobs.find(handle);
    if (it == m_allJobs.end())
    {
        return;  // Job not found (already cleaned up)
    }

    // Wait until job is complete
    m_jobComplete.wait(lock, [this, handle]() {
        auto it = m_allJobs.find(handle);
        return (it == m_allJobs.end() || it->second->isComplete);
    });
}

void JobSystem::waitForAllJobs()
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            if (m_allJobs.empty())
            {
                return;
            }

            // Check if all jobs are complete
            bool allComplete = true;
            for (const auto& pair : m_allJobs)
            {
                if (!pair.second->isComplete)
                {
                    allComplete = false;
                    break;
                }
            }
            if (allComplete)
            {
                return;
            }
        }

        // Wait a bit before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool JobSystem::isJobComplete(JobHandle handle) const
{
    if (handle == INVALID_JOB_HANDLE)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_jobsMutex);
    auto it = m_allJobs.find(handle);
    if (it == m_allJobs.end())
    {
        return true;  // Job not found (already completed and cleaned)
    }
    return it->second->isComplete;
}

///////////////////////////////////////////////////////////////////////////////
// Callback Processing (Main Thread)
///////////////////////////////////////////////////////////////////////////////

void JobSystem::processPendingCallbacks()
{
    std::vector<Job> completedJobs;

    // Swap completed jobs with local copy (minimal lock time)
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        completedJobs.swap(m_completedJobs);
    }

    // Execute all completion callbacks (no lock held)
    for (auto& job : completedJobs)
    {
        if (job.onComplete)
        {
            job.onComplete();
        }

        // Clean up completed job from tracking map
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_allJobs.erase(job.handle);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Statistics and Debugging
///////////////////////////////////////////////////////////////////////////////

int JobSystem::getCompletedJobCount() const
{
    std::lock_guard<std::mutex> lock(m_completedMutex);
    return static_cast<int>(m_completedJobs.size());
}

int JobSystem::getPendingJobCount() const
{
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    return static_cast<int>(m_allJobs.size());
}

void JobSystem::dumpStats() const
{
    if (!m_initialized)
    {
        std::cout << "JobSystem: Not initialized\n";
        return;
    }

    int pending = getPendingJobCount();
    int completed = getCompletedJobCount();
    int total = pending + completed;

    std::cout << "JobSystem Stats:\n";
    std::cout << "  I/O Threads: " << m_numIOThreads << "\n";
    std::cout << "  Worker Threads: " << m_numWorkerThreads << "\n";
    std::cout << "  Pending Jobs: " << pending << "\n";
    std::cout << "  Completed (Waiting for Callback): " << completed << "\n";
    std::cout << "  Total Tracked: " << total << "\n";

    // Count jobs by priority
    for (int i = 0; i < 4; ++i)
    {
        const auto& queue = m_jobQueues[i];
        std::lock_guard<std::mutex> lock(queue.mutex);
        const char* priorityNames[] = {"IMMEDIATE", "HIGH", "NORMAL", "LOW"};
        std::cout << "  " << priorityNames[i] << " queue: " 
                  << queue.jobs.size() << " jobs\n";
    }
}

