#include "facebook_private.h"
#include "facebook_util.h"

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

void HandleCommand(dmFacebook::FacebookCommand* cmd, void* ctx)
{
    lua_State* paramsL = (lua_State*)ctx;

    // Checking that we're in the correct context (in case of a non shared Lua state)
    lua_State* L = dmScript::GetCallbackLuaContext(cmd->m_Callback);
    if (L != paramsL)
        return;

    switch (cmd->m_Type)
    {
        case dmFacebook::COMMAND_TYPE_LOGIN:
            dmFacebook::RunStateCallback(cmd->m_Callback, cmd->m_State, cmd->m_Error); // DEPRECATED
            break;
        case dmFacebook::COMMAND_TYPE_REQUEST_READ:
        case dmFacebook::COMMAND_TYPE_REQUEST_PUBLISH:
        case dmFacebook::COMMAND_TYPE_LOGIN_WITH_PERMISSIONS:
            dmFacebook::RunStatusCallback(cmd->m_Callback, cmd->m_Error, cmd->m_State);
            break;
        case dmFacebook::COMMAND_TYPE_DIALOG_COMPLETE:
        case dmFacebook::COMMAND_TYPE_DEFERRED_APP_LINK:
            dmFacebook::RunJsonResultCallback(cmd->m_Callback, cmd->m_Results, cmd->m_Error);
            break;
    }
    free((void*)cmd->m_Results);
    free((void*)cmd->m_Error);
    dmScript::DestroyCallback(cmd->m_Callback);
}

} // namespace
