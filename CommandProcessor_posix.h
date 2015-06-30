
#include <string>
#include <string.h>
#include <vector>
#include <list>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "CommandBuffer.h"
#include "Pool.h"

namespace mozilla {
namespace gfx {

enum class CommandStatus {
    CONTINUE,
    WAIT,
    YIELD,
    ERROR,
    COMPLETE
};

/// Process commands until the command buffer needs to block on a sync object,
/// completes, yields, or encounters an error.
///
/// Can be used on any thread.
CommandStatus ProcessCommands(CommandBuffer* commands);

/// A simple and naive multithreaded task queue
class TaskQueueMT {
public:
  // Producer thread
  TaskQueueMT();

  // Producer thread
  ~TaskQueueMT();

  // Worker threads
  bool WaitForCommands(CommandBuffer*& aOutCommands);

  // Any threads
  void SubmitCommands(CommandBuffer* aCommands);

  // Producer thread
  void ShutDown();

  // Any thread
  size_t NumTasks();

  // Producer thread
  void RegisterThread();

  // Worker threads
  void UnregisterThread();

protected:

  std::list<CommandBuffer*> mTasks;
  pthread_mutex_t mMutex;
  pthread_cond_t mAvailableCondvar;
  pthread_cond_t mShutdownCondvar;
  uint32_t mTasksSize;
  int32_t mThreadsCount;
  bool mShuttingDown;

  friend class WorkerThread;
};


/// RAII helper for pthread muteces.
struct PThreadAutoLock {
    PThreadAutoLock(pthread_mutex_t* aMutex)
    : mMutex(aMutex)
    {
        pthread_mutex_lock(mMutex);
    }

    ~PThreadAutoLock()
    {
        pthread_mutex_unlock(mMutex);
    }

    pthread_mutex_t* mMutex;
};

/// A synchronization object that can be used to express dependencies and ordering between
/// command buffers.
///
/// CommandBuffers can register to SyncObjects in order to asynchronously wait for a signal.
/// Threads can also synchronously be blocked until the SyncObject gets into the signaled state.
class SyncObject {
public:
    /// Create a synchronization object.
    ///
    /// aNumSignals determines the number of times the Signal() must be called
    /// before the object gets into the signaled state.
    SyncObject(int aNumSignals = 1);

    ~SyncObject();

    /// Attempt to register a command buffer.
    ///
    /// If the sync object is already in the signaled state, the buffer is *not*
    /// registered and the sync object does not take ownership if the CommandBuffer.
    /// If the object is not yet in the signaled state, it takes ownership of
    /// the command buffer and places it in a list of pending command buffers.
    /// Pending command buffers will not be processed by the worker thread.
    /// When the SyncObject reaches the signaled state, it places the pending
    /// command buffers back in the available buffer queue, so that they can be
    /// scheduled again.
    ///
    /// Returns true if the SyncOject is already in the signaled state.
    bool Register(CommandBuffer* commands);

    /// Signal the SyncObject.
    ///
    /// This decrements an internal counter. The sync object reaches the signaled
    /// state when the counter gets to zero.
    /// calling Signal on a SyncObject that is already in the signaled state has
    /// no effect.
    void Signal();

    /// Block the calling thread until another thread puts the SyncObject
    /// in the signaled state.
    void WaitSync();

private:
    std::vector<CommandBuffer*> mWaitingCommands;
    pthread_mutex_t mMutex;
    pthread_cond_t mCond;
    int mSignals;
};

/// Worker thread that continuously dequeue CommandBuffers from a TaskQueueMT
/// and process them.
struct WorkerThread {
  WorkerThread(TaskQueueMT* aTaskQueue);

  ~WorkerThread();

  void Run();
protected:
  TaskQueueMT* mQueue;
  pthread_t mThread;
};

} // namespace
} // namespace

