#ifndef MOZILLA_GFX_COMMANDBUFFER
#define MOZILLA_GFX_COMMANDBUFFER

#include <stdint.h>

#include "Pool.h"

namespace mozilla {
namespace gfx {

class DrawingCommand;
class WaitCommand;
class SignalCommand;
class PrintCommand;

class SyncObject;
class TaskQueueMT;

struct IntPoint { int32_t x; int32_t y; };
struct Matrix {};
class DrawTarget;

enum class CommandType {
  // operations
  DRAWING_COMMAND,
  // control flow
  WAIT,
  SIGNAL,
  YIELD,
  // debug
  PRINT
};

/// returns the size of the command structure associated to a given CommandType.
size_t SizeOfCommand(CommandType type);

/// Base class for Commands
class Command {
public:
    CommandType GetType() const { return mType; }
    WaitCommand* AsWaitCommand() { return mType == CommandType::WAIT ? (WaitCommand*)this : nullptr; }
    SignalCommand* AsSignalCommand() { return mType == CommandType::SIGNAL ? (SignalCommand*)this : nullptr; }
    PrintCommand* AsPrintCommand() { return mType == CommandType::PRINT ? (PrintCommand*)this : nullptr; }
    DrawingCommand* AsDrawingCommand() { return mType == CommandType::DRAWING_COMMAND ? (DrawingCommand*)this : nullptr; }
protected:
    Command(CommandType aType) : mType(aType) {}
    CommandType mType;
};

struct CommandBufferData {
    DrawTarget* mDrawTarget;
    Matrix mTransform;
    IntPoint mOrigin;
};

class CommandBufferBuilder;

/// Stores multiple commands to be executed sequencially.
class CommandBuffer {
public:
    CommandBuffer(TaskQueueMT* aTaskQueue);

    ~CommandBuffer() {}

    Command* PopCommand();

    TaskQueueMT* GetTaskQueue() { return mQueue; }

    CommandBufferData& GetData() { return mData; }

protected:
    GrowablePool mStorage;
    ptrdiff_t mCursor;
    uint32_t mNumCommands;
    TaskQueueMT* mQueue;
    CommandBufferData mData;

    friend class CommandBufferBuilder;
};

/// Generates CommandBuffer objects.
class CommandBufferBuilder {
public:
    CommandBufferBuilder();

    ~CommandBufferBuilder();

    /// Allocates a CommandBuffer.
    ///
    /// call this method before starting to add commands.
    void BeginCommandBuffer(TaskQueueMT* aTaskQueue);

    /// Build the CommandBuffer, command after command.
    /// This must be used between BeginCommandBuffer (or BeginRecycleAndBeginCommandBuffer)
    /// and EndCommandBuffer.
    template<typename T>
    void AddCommand(const T& aCmd)
    {
        assert(mCommands);
        mCommands->mStorage.AddItem(aCmd);
        mCommands->mNumCommands += 1;
    }

    /// Finalizes and returns the command buffer.
    CommandBuffer* EndCommandBuffer();

    /// Similar to BeginCommandBuffer, avoiding the potentially expensive allocation
    /// of a new CommandBuffer.
    void RecycleAndBeginCommandBuffer(CommandBuffer* aBuffer, TaskQueueMT* aTaskQueue);

protected:
    CommandBuffer* mCommands;
};


class DrawingOperation {
public:
  virtual bool ExecuteOnTarget(DrawTarget* aDT, const Matrix* aTransform) = 0;
};

class DrawingCommand : public Command {
public:
    DrawingCommand(DrawingOperation* aOperation)
    : Command(CommandType::DRAWING_COMMAND)
    , mOp(aOperation)
    {}

    bool ExecuteOnTarget(DrawTarget* aTarget, const Matrix* aTransform)
    {
        return mOp->ExecuteOnTarget(aTarget, aTransform);
    }

protected:
    DrawingOperation* mOp;
};

/// Ensures that the subsequent commands are processed after the SyncObject is
/// signaled. In other words, this expresses that subsequent commands depend on
/// whatever will signal the SynObject.
class WaitCommand : public Command {
public:
    WaitCommand(SyncObject* sync)
    : Command(CommandType::WAIT)
    , mSyncObject(sync)
    {}
    SyncObject* GetSyncObject() { return mSyncObject; }
protected:
    SyncObject* mSyncObject;
};

/// Calls Signal() on a SyncObject.
class SignalCommand : public Command {
public:
    SignalCommand(SyncObject* sync)
    : Command(CommandType::SIGNAL)
    , mSyncObject(sync)
    {}

    SyncObject* GetSyncObject() { return mSyncObject; }
protected:
    SyncObject* mSyncObject;
};

/// Causes to CommandBuffer to pause and go back to the queue of available
/// CommandBuffers, giving other CommandBuffers a chance to be scheduled in
/// its place.
class YieldCommand : public Command {
public:
    YieldCommand() : Command(CommandType::YIELD) {}
};

/// For debugging purposes.
struct PrintCommand : public Command {
    PrintCommand(const char* str) : Command(CommandType::PRINT), mStr(str) {}
    const char* mStr;
};

} // namespace
} // namespace

#endif
