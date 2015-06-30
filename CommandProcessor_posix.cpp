
#include "CommandProcessor_posix.h"
#include "CommandBuffer.h"

using namespace std;

namespace mozilla {
namespace gfx {

CommandStatus ProcessCommands(CommandBuffer* commands)
{
    while (true) {
        Command* cmd = commands->PopCommand();
        if (!cmd) {
            return CommandStatus::COMPLETE;
        }

        switch (cmd->GetType()) {
            case CommandType::DRAWING_COMMAND: {
                cmd->AsDrawingCommand()->ExecuteOnTarget(commands->GetData().mDrawTarget,
                                                         &commands->GetData().mTransform);
                break;
            }
            case CommandType::SIGNAL: {
                cmd->AsSignalCommand()->GetSyncObject()->Signal();
                break;
            }
            case CommandType::WAIT: {
                bool signaled = cmd->AsWaitCommand()->GetSyncObject()->Register(commands);
                if (!signaled) {
                    return CommandStatus::WAIT;
                }
                break;
            }
            case CommandType::YIELD: {
                commands->GetTaskQueue()->SubmitCommands(commands);
                return CommandStatus::YIELD;
            }
            case CommandType::PRINT: {
                printf(" ** %s\n", cmd->AsPrintCommand()->mStr);
                break;
            }
            default: {
                assert(false);
            }
        }
    }
}

TaskQueueMT::TaskQueueMT()
: mTasksSize(0)
, mThreadsCount(0)
, mShuttingDown(false)
{
    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mAvailableCondvar, nullptr);
    pthread_cond_init(&mShutdownCondvar, nullptr);
}

TaskQueueMT::~TaskQueueMT()
{
    assert(mTasksSize == 0);
}

bool TaskQueueMT::WaitForCommands(CommandBuffer*& aOutCommands)
{
  while (true) {
    PThreadAutoLock lock(&mMutex);

    while (!mShuttingDown && mTasksSize == 0) {
      pthread_cond_wait(&mAvailableCondvar, &mMutex);
    }

    if (mShuttingDown) {
        return false;
    }

    CommandBuffer* task = mTasks.front();
    mTasks.pop_front();
    mTasksSize--;

    aOutCommands = task;
    return true;
  }
}

void TaskQueueMT::SubmitCommands(CommandBuffer* aCommands)
{
    PThreadAutoLock lock(&mMutex);
    mTasks.push_back(aCommands);
    mTasksSize++;
    pthread_cond_broadcast(&mAvailableCondvar);
}

size_t TaskQueueMT::NumTasks()
{
    PThreadAutoLock lock(&mMutex);
    return mTasksSize;
}

void TaskQueueMT::ShutDown()
{
    PThreadAutoLock lock(&mMutex);
    mShuttingDown = true;
    while (mThreadsCount) {
        printf(" -- waiting for %i threads\n", (int)mThreadsCount);
        pthread_cond_broadcast(&mAvailableCondvar);
        pthread_cond_wait(&mShutdownCondvar, &mMutex);
    }
}

void TaskQueueMT::RegisterThread()
{
    mThreadsCount += 1;
}

void TaskQueueMT::UnregisterThread()
{
    PThreadAutoLock lock(&mMutex);
    mThreadsCount -= 1;
    if (mThreadsCount == 0) {
        pthread_cond_broadcast(&mShutdownCondvar);
    }
}

void* ThreadRoutine(void* threadData)
{
  WorkerThread* thread = (WorkerThread*)threadData;
  thread->Run();
  return nullptr;
}

WorkerThread::WorkerThread(TaskQueueMT* aTaskQueue)
: mQueue(aTaskQueue)
{
  printf(" -- creating thread\n");
  aTaskQueue->RegisterThread();
  pthread_create(&mThread, nullptr, ThreadRoutine, this);
}

WorkerThread::~WorkerThread()
{
  printf(" -- joining thread\n");
  pthread_join(mThread, nullptr);
}

void
WorkerThread::Run()
{
  for (;;) {
    CommandBuffer* commands = nullptr;
    if (!mQueue->WaitForCommands(commands)) {
      mQueue->UnregisterThread();
      return;
    }

    CommandStatus status = ProcessCommands(commands);

    if (status == CommandStatus::COMPLETE) {
        delete commands;
    }
  }
}

bool
SyncObject::Register(CommandBuffer* commands)
{
    PThreadAutoLock lock(&mMutex);

    if (mSignals) {
        mWaitingCommands.push_back(commands);
    }

    return mSignals == 0;
}

void
SyncObject::Signal()
{
    PThreadAutoLock lock(&mMutex);
    if (mSignals == 0) {
        return;
    }
    mSignals -= 1;

    if (mSignals == 0) {
        for (int i = 0; i < mWaitingCommands.size(); ++i) {
            CommandBuffer* cmd = mWaitingCommands[i];
            cmd->GetTaskQueue()->SubmitCommands(cmd);
        }
        mWaitingCommands.clear();
        pthread_cond_broadcast(&mCond);
    }
}

void
SyncObject::WaitSync()
{
    PThreadAutoLock lock(&mMutex);
    if (mSignals == 0) {
        return;
    }
    pthread_cond_wait(&mCond, &mMutex);
}

SyncObject::SyncObject(int aNumSignals)
: mSignals(aNumSignals)
{
    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mCond, nullptr);
}

SyncObject::~SyncObject()
{
    PThreadAutoLock lock(&mMutex);
    assert(mWaitingCommands.size() == 0);
}


} // namespce
} // namespce
