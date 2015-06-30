
#include "CommandBuffer.h"

namespace mozilla {
namespace gfx {

size_t SizeOfCommand(CommandType type)
{
    switch (type) {
        case CommandType::DRAWING_COMMAND: return sizeof(DrawingCommand);
        case CommandType::WAIT: return sizeof(WaitCommand);
        case CommandType::SIGNAL: return sizeof(SignalCommand);
        case CommandType::YIELD: return sizeof(YieldCommand);
        case CommandType::PRINT: return sizeof(PrintCommand);
    }
    assert(false);
    return 0;
}

CommandBufferBuilder::CommandBufferBuilder()
: mCommands(nullptr)
{}

CommandBufferBuilder::~CommandBufferBuilder()
{
    if (mCommands) {
        delete mCommands;
    }
}

void
CommandBufferBuilder::BeginCommandBuffer(TaskQueueMT* aTaskQueue)
{
    assert(!mCommands);
    assert(aTaskQueue);
    mCommands = new CommandBuffer(aTaskQueue);
}

void
CommandBufferBuilder::RecycleAndBeginCommandBuffer(CommandBuffer* aBuffer, TaskQueueMT* aTaskQueue)
{
    assert(!mCommands);
    assert(aTaskQueue);
    mCommands = aBuffer;
    aBuffer->mQueue = aTaskQueue;
    aBuffer->mCursor = 0;
    aBuffer->mNumCommands = 0;
}

CommandBuffer*
CommandBufferBuilder::EndCommandBuffer()
{
    assert(mCommands);
    CommandBuffer* cmd = mCommands;
    mCommands = nullptr;
    return cmd;
}

CommandBuffer::CommandBuffer(TaskQueueMT* aTaskQueue)
: mStorage(512)
, mCursor(0)
, mNumCommands(0)
, mQueue(aTaskQueue)
{}

Command*
CommandBuffer::PopCommand()
{
    if (mNumCommands == 0) {
        return nullptr;
    }

    Command* cmd = (Command*)mStorage.GetStorage(mCursor);
    mCursor += SizeOfCommand(cmd->GetType());
    mNumCommands -= 1;

    return cmd;
}

} // namespace
} // namespace
