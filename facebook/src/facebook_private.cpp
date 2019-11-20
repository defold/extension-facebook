#include "facebook_private.h"

#include <dmsdk/dlib/mutex.h>

namespace dmFacebook
{

void QueueCreate(CommandQueue* queue)
{
	queue->m_Mutex = dmMutex::New();
}

void QueueDestroy(CommandQueue* queue)
{
	dmMutex::Delete(queue->m_Mutex);
}

void QueuePush(CommandQueue* queue, FacebookCommand* cmd)
{
    DM_MUTEX_SCOPED_LOCK(queue->m_Mutex);
    if (queue->m_Commands.Full())
    {
        queue->m_Commands.OffsetCapacity(8);
    }
    queue->m_Commands.Push(*cmd);
}

void QueueFlush(CommandQueue* queue, FacebookCommandFn fn, void* ctx)
{
	if (queue->m_Commands.Empty())
		return;
    DM_MUTEX_SCOPED_LOCK(queue->m_Mutex);
    queue->m_Commands.Map(fn, ctx);
    queue->m_Commands.SetSize(0);
}

} // namespace
