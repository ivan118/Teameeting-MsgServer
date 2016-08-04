#include <iostream>
#include <stdio.h>
#include <assert.h>

#include "GRTGrouper.h"
#include "atomic.h"
#include "OS.h"
#include "OSThread.h"
#include "IdleTask.h"
#include "Socket.h"
#include "SocketUtils.h"
#include "TimeoutTask.h"
#include "RTUtils.hpp"
#include "GRTGrouperManager.h"
#include "GRTConnManager.h"
#include "GRTChannelManager.h"
#include "GRTSubChannel.h"

#include "GRTTransferSession.h"

static bool		g_inited = false;
static char*	g_pVersion = (char*)"0.01.20150810";
static GRTGrouper*	g_pGrouper = NULL;

void GRTGrouper::PrintVersion()
{
	printf("<******** DYNC MSG Server ********>\r\n");
	printf("* Version:\t %s \r\n", g_pVersion);
	printf("* Authed:\t Xmc & Mzw \r\n");
	printf("* \r\n");
	printf("* Copyright:\t www.dync.cc \r\n");
	printf("<**********************************>\r\n");
	printf("\r\n");
}


void GRTGrouper::Initialize(int evTbSize)
{
	// Initialize utility classes
    LI("Hello server...\n");
	bool inDontFork = false;
	OS::Initialize();//initialize
	OSThread::Initialize();// initialize
    IdleTask::Initialize();// initialize IdleTaskThread

	// Initialize socket classes
	Socket::Initialize(evTbSize);// initialize EventThread
	SocketUtils::Initialize(!inDontFork);
#if !MACOSXEVENTQUEUE
	//Initialize the select() implementation of the event queue
	::select_startevents();
#endif

	{
		SInt32 numShortTaskThreads = 0;
		SInt32 numBlockingThreads = 0;
		SInt32 numThreads = 0;
		SInt32 numProcessors = 0;
		numProcessors = OS::GetNumProcessors();
		//////LI("Processer core Num:%lu\n",numProcessors);
		// Event thread need 1 position;
		numShortTaskThreads = numProcessors*2 - 1;
		if(numShortTaskThreads == 0)
			numShortTaskThreads = 2;
		numBlockingThreads = 1;
		numThreads = numShortTaskThreads + numBlockingThreads;
		// ////LI("Add threads shortask=%lu blocking=%lu\n",numShortTaskThreads, numBlockingThreads);
		TaskThreadPool::SetNumShortTaskThreads(numShortTaskThreads);
		TaskThreadPool::SetNumBlockingTaskThreads(numBlockingThreads);
		TaskThreadPool::AddThreads(numThreads);//initialize TaskThread & Start
	}

	TimeoutTask::Initialize();// initialize Task

	Socket::StartThread();// start EventThread
	OSThread::Sleep(100);
	g_pGrouper = new GRTGrouper();
	g_inited = true;
}

void GRTGrouper::DeInitialize()
{
	delete g_pGrouper;
	g_pGrouper = NULL;
	g_inited = false;
	TaskThreadPool::RemoveThreads();

	//Now, make sure that the server can't do any work
	TimeoutTask::UnInitialize();

	SocketUtils::UnInitialize();
    sleep(1);
    IdleTask::UnInitialize();

#if !MACOSXEVENTQUEUE
	::select_stopevents();
#endif

	Socket::Uninitialize();// uninitialize EventThread
	OS::UnInitialize();
    LI("ByeBye server...\n");
}

GRTGrouper* GRTGrouper::Inst()
{
	Assert(g_pGrouper != NULL);
	return g_pGrouper;
}

GRTGrouper::GRTGrouper(void)
: m_pGrouperListener(NULL)
{

}

GRTGrouper::~GRTGrouper(void)
{
    if (m_pGrouperListener)
    {
        delete m_pGrouperListener;
        m_pGrouperListener = NULL;
    }
}

int	GRTGrouper::Start(const char*pGrouperIp, unsigned short usGrouperPort, const char*pGroupMgrIp, unsigned short usGroupMgrPort, const char* pRedisIp, unsigned short usRedisPort)
{
	Assert(g_inited);
	Assert(pGrouperIp != NULL && strlen(pGrouperIp)>0);
    GRTGrouperManager::Instance().InitManager(pRedisIp, usRedisPort);
    GRTChannelManager::Instance().InitManager();

    std::string channel1("follow_group");
    std::string channel2("unfollow_group");
    GRTSubChannel* ch1 = new GRTSubChannel(pRedisIp, usRedisPort, channel1);
    if (!ch1)
        return -1;
    if (!ch1->Init())
    {
        delete ch1;
        return -2;
    }
    GRTSubChannel* ch2 = new GRTSubChannel(pRedisIp, usRedisPort, channel2);
    if (!ch2)
        return -1;
    if (!ch2->Init())
    {
        delete ch2;
        return -2;
    }
    GRTChannelManager::Instance().AddChannel(channel1, ch1);
    GRTChannelManager::Instance().AddChannel(channel2, ch2);
    GRTChannelManager::Instance().SubChannel(channel1);
    GRTChannelManager::Instance().SubChannel(channel2);

    std::string ssid;
    if (usGrouperPort > 0) {
        m_pGrouperListener = new GRTGrouperListener();
        OS_Error err = m_pGrouperListener->Initialize(INADDR_ANY, usGrouperPort);
        if (err!=OS_NoErr) {
            LE("CreateGrouperListener error port:%d\n", usGrouperPort);
            delete m_pGrouperListener;
            m_pGrouperListener = NULL;
            return -1;
        }
        LI("Start Grouper service:(%d) ok...,socketFD:%d\n", usGrouperPort, m_pGrouperListener->GetSocketFD());
        m_pGrouperListener->RequestEvent(EV_RE);
    }

    if(usGroupMgrPort > 0)
	{
        char addr[24] = {0};
        sprintf(addr, "%s %u", pGroupMgrIp, usGroupMgrPort);
        GRTConnManager::Instance().GetGroupMgrAddrList()->push_front(addr);

        LI("ConnectGroupMgr was called...\n");
        if (!(GRTConnManager::Instance().ConnectGroupMgr())) {
            LE("Start to ConnectGroupMgr failed\n");
            return -1;
        }
	}

	return 0;
}

void GRTGrouper::DoTick()
{
#if 1
#endif
}

void GRTGrouper::Stop()
{
    GRTGrouperManager::Instance().SignalKill();
    GRTGrouperManager::Instance().ClearAll();
    GRTGrouperManager::Instance().UninManager();
    GRTChannelManager::Instance().UninManager();
}
