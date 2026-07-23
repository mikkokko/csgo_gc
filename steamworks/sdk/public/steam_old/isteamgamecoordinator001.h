// Extracted from steamworks_sdk_164
#ifndef ISTEAMGAMECOORDINATOR001_H
#define ISTEAMGAMECOORDINATOR001_H

class ISteamGameCoordinator001
{
public:

	// sends a message to the Game Coordinator
	virtual EGCResults SendMessage( uint32 unMsgType, const void *pubData, uint32 cubData ) = 0;

	// returns true if there is a message waiting from the game coordinator
	virtual bool IsMessageAvailable( uint32 *pcubMsgSize ) = 0; 

	// fills the provided buffer with the first message in the queue and returns k_EGCResultOK or 
	// returns k_EGCResultNoMessage if there is no message waiting. pcubMsgSize is filled with the message size.
	// If the provided buffer is not large enough to fit the entire message, k_EGCResultBufferTooSmall is returned
	// and the message remains at the head of the queue.
	virtual EGCResults RetrieveMessage( uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize ) = 0; 

};

#endif // ISTEAMGAMECOORDINATOR001_H
