
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#include "net.h"

#include "logger.h"
#include "csimplescan.h"
#include "steamclient.h"


namespace NetHook
{


BBuildAndAsyncSendFrameFn BBuildAndAsyncSendFrame_Orig = nullptr;
RecvPktFn RecvPkt_Orig = nullptr;

CNet::CNet() noexcept
	: m_RecvPktDetour(nullptr),
	  m_BuildDetour(nullptr)
{
	CSimpleScan steamClientScan(STEAMCLIENT_DLL);

	BBuildAndAsyncSendFrameFn pBuildFunc = nullptr;
	const bool bFoundBuildFunc = steamClientScan.FindFunction(
#if __x86_64__
		"\x48\x8B\xC4\x55\x53\x48\x8D\x68\xA1\x48\x81\xEC\xCC\xCC\xCC\xCC\x48\x89\x70\x10\x33",
		"xxxxxxxxxxxx????xxxxx",
#else
		"\x55\x8B\xEC\x83\xEC\x70\xA1\x2A\x2A\x2A\x2A\x53\x8B\xD9",
		"xxxxxxx????xxx",
#endif
		(void**)&pBuildFunc
	);

	BBuildAndAsyncSendFrame_Orig = pBuildFunc;

	g_pLogger->LogConsole("CWebSocketConnection::BBuildAndAsyncSendFrame = 0x%p\n", BBuildAndAsyncSendFrame_Orig);

	RecvPktFn pRecvPktFunc = nullptr;
	const bool bFoundRecvPktFunc = steamClientScan.FindFunction(
#if __x86_64__
		"\x48\x8B\xC4\x55\x48\x8D\xA8\xCC\xCC\xCC\xCC\x48\x81\xEC\xCC\xCC\xCC\xCC\x48\x89\x58\x08\x48\x8B",
		"xxxxxxx????xxx????xxxxxx",
#else
		"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x56\x57\x8B\xF9",
		"xxxxx????x????xxxx",
#endif
		(void**)&pRecvPktFunc
	);

	RecvPkt_Orig = pRecvPktFunc;

	g_pLogger->LogConsole("CCMInterface::RecvPkt = 0x%p\n", RecvPkt_Orig);


	if (bFoundBuildFunc)
	{
		BBuildAndAsyncSendFrameFn thisBuildFunc = CNet::BBuildAndAsyncSendFrame;

		m_BuildDetour = new CSimpleDetour((void **)&BBuildAndAsyncSendFrame_Orig, (void *)thisBuildFunc);
		m_BuildDetour->Attach();

		g_pLogger->LogConsole("Detoured CWebSocketConnection::BBuildAndAsyncSendFrame!\n");
	}
	else
	{
		g_pLogger->LogConsole("Unable to hook CWebSocketConnection::BBuildAndAsyncSendFrame: func scan failed.\n");
	}

	if (bFoundRecvPktFunc)
	{
		RecvPktFn thisRecvPktFunc = CNet::RecvPkt;

		m_RecvPktDetour = new CSimpleDetour((void **)&RecvPkt_Orig, (void *)thisRecvPktFunc);
		m_RecvPktDetour->Attach();

		g_pLogger->LogConsole("Detoured CCMInterface::RecvPkt!\n");
	}
	else
	{
		g_pLogger->LogConsole("Unable to hook CCMInterface::RecvPkt: func scan failed.\n");
	}

}

CNet::~CNet()
{
	if (m_RecvPktDetour)
	{
		m_RecvPktDetour->Detach();
		delete m_RecvPktDetour;
	}

	if (m_BuildDetour)
	{
		m_BuildDetour->Detach();
		delete m_BuildDetour;
	}
}


bool CNet::BBuildAndAsyncSendFrame(
	void *webSocketConnection,
#ifndef __x86_64__
	void *,
#endif
	EWebSocketOpCode eWebSocketOpCode, 
	const uint8 *pubData, 
	uint32 cubData)
{
	if (eWebSocketOpCode == EWebSocketOpCode::k_eWebSocketOpCode_Binary)
	{
		g_pLogger->LogNetMessage(ENetDirection::k_eNetOutgoing, pubData, cubData);
	}
	else
	{
		g_pLogger->LogConsole("Sending websocket frame with opcode %d (%s), ignoring\n",
			eWebSocketOpCode, EWebSocketOpCodeToName(eWebSocketOpCode)
		);
	}

	return (*BBuildAndAsyncSendFrame_Orig)(webSocketConnection, eWebSocketOpCode, pubData, cubData);
}

void CNet::RecvPkt(
	void *cmConnection,
#ifndef __x86_64__
	void *,
#endif
	CNetPacket *pPacket)
{
	g_pLogger->LogNetMessage(ENetDirection::k_eNetIncoming, pPacket->m_pubData, pPacket->m_cubData);

	(*RecvPkt_Orig)(cmConnection, pPacket);
}


}
