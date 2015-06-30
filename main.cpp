#include "CommandProcessor_posix.cpp"
#include "CommandBuffer.cpp"

using namespace mozilla::gfx;

int main()
{

    TaskQueueMT queue;

    SyncObject syncObj;
    SyncObject CompletionSync(3);

    CommandBufferBuilder cmd_a;
    CommandBufferBuilder cmd_b;
    CommandBufferBuilder cmd_c;

    // prepare the command buffer cmd_a
    cmd_a.BeginCommandBuffer(&queue);
        cmd_a.AddCommand(PrintCommand("1A"));
        // cmd_a will wait for cmd_b to signal syncObj before executing "2A"
        cmd_a.AddCommand(WaitCommand(&syncObj));
        cmd_a.AddCommand(PrintCommand("2A"));
        cmd_a.AddCommand(YieldCommand());
        cmd_a.AddCommand(PrintCommand("3A"));
        cmd_a.AddCommand(PrintCommand("4A"));
        // signal that the command queue is processed so that the producer
        // thread can wait.
        cmd_a.AddCommand(SignalCommand(&CompletionSync));
    queue.SubmitCommands(cmd_a.EndCommandBuffer());


    // prepare the command buffer cmd_b
    cmd_b.BeginCommandBuffer(&queue);
        cmd_b.AddCommand(PrintCommand("1B"));
        cmd_b.AddCommand(WaitCommand(&syncObj));
        cmd_b.AddCommand(PrintCommand("2B"));
        cmd_b.AddCommand(PrintCommand("3B"));
        cmd_b.AddCommand(PrintCommand("4B"));
        cmd_b.AddCommand(SignalCommand(&CompletionSync));
    queue.SubmitCommands(cmd_b.EndCommandBuffer());


    // prepare the command buffer cmd_c
    cmd_c.BeginCommandBuffer(&queue);
        cmd_c.AddCommand(PrintCommand("1C"));
        cmd_c.AddCommand(PrintCommand("2C"));
        cmd_c.AddCommand(PrintCommand("3C"));
        cmd_c.AddCommand(PrintCommand("4C"));
        cmd_c.AddCommand(PrintCommand("5C"));
        cmd_c.AddCommand(YieldCommand());
        cmd_c.AddCommand(PrintCommand("6C"));
        cmd_c.AddCommand(PrintCommand("7C"));
        cmd_c.AddCommand(PrintCommand("8C"));
        cmd_c.AddCommand(PrintCommand("9C"));
        // signal syncObj, enables cmd_a and cmd_b to process beyound
        cmd_c.AddCommand(SignalCommand(&syncObj));
        cmd_c.AddCommand(SignalCommand(&CompletionSync));
    queue.SubmitCommands(cmd_c.EndCommandBuffer());

    WorkerThread thread1(&queue);
    WorkerThread thread2(&queue);
    WorkerThread thread3(&queue);
    WorkerThread thread4(&queue);

    printf(" -- wait for completion\n");
    CompletionSync.WaitSync();

    printf(" -- Shutdown\n");
    queue.ShutDown();
    printf(" -- bye\n");
}
