#include "ComPointB.hpp"


void ComPointB::thread_listen()
{
	listen_thread_active = true;
	string* content = NULL;
	RPCMsg* tempMsg = NULL;
	int recvSize = 0;
	int messageSize = 0;
	pthread_t worker_thread = getWorker();


	FD_ZERO(&rfds);
	FD_SET(currentSocket, &rfds);

	while(listen_thread_active)
	{
		memset(receiveBuffer, '\0', BUFFER_SIZE);

		//hier nur hin wenn nichts mehr im buffer ist und auf keine teile einer nachricht gewartet wird.
		recvSize = recv(currentSocket , receiveBuffer, BUFFER_SIZE, 0);
		if(recvSize > 0)
		{
			try
			{
				//copy receiveBuffer to a clean msgBuffer
				msgBufferSize = recvSize;
				msgBuffer = new char[msgBufferSize];
				memset(msgBuffer, '\0', msgBufferSize);
				memcpy(msgBuffer, receiveBuffer, msgBufferSize);

				//As long as there is data in the msgBuffer
				while(msgBufferSize > 0)
				{
					//headersize = 1 byte tagfield + 4 byte lengthfield
					if(msgBufferSize > HEADER_SIZE)
					{
						messageSize = readHeader(msgBuffer);

						//header ok ???
						if(messageSize > -1)
						{
							//Is there at least one complete message in msgBuffer ?
							if(msgBufferSize >= messageSize+HEADER_SIZE)
							{
								//add first complete msg of msgbuffer to the receivequeue and signal the worker
								content = new string(&msgBuffer[HEADER_SIZE], messageSize);
								tempMsg = new IncomingMsg(this, content);

								if(!pInterface->isBusy())
								{
									push_frontReceiveQueue(tempMsg);
									pthread_kill(worker_thread, SIGUSR1);
								}
								else
								{
									if(pInterface->isSubResponse(tempMsg))
									{
										log(logInfoIn, tempMsg->getContent());
										pthread_kill(worker_thread, SIGUSR2);
										delete tempMsg;
									}
									else
									{
										push_frontReceiveQueue(tempMsg);
										//TODO: send signal sigusr1 ?
									}
								}
								//Is there more data ?
								if(msgBufferSize > messageSize+HEADER_SIZE)
								{
									//copy rest of data to a new clean msgBuffer
									msgBufferSize = msgBufferSize - (messageSize+HEADER_SIZE);
									tempBuffer = new char[msgBufferSize];
									memset(tempBuffer, '\0', msgBufferSize);
									memcpy(tempBuffer, &(msgBuffer[messageSize+HEADER_SIZE]), msgBufferSize);
									delete[] msgBuffer;
									msgBuffer = tempBuffer;
									tempBuffer = NULL;
								}
								else
								{
									delete[] msgBuffer;
									msgBuffer = NULL;
									msgBufferSize = 0;
								}
							}

							else//message is not complete, wait for the rest
							{
								waitForFurtherData();
							}
						}
						else
						{
							delete[] msgBuffer;
							msgBuffer = NULL;
							msgBufferSize = 0;
							transmit(JSON_ERROR_RESPONSE_INCORECCT_MSG, strlen(JSON_ERROR_RESPONSE_INCORECCT_MSG));
						}
					}
					//not even the header was complete, wait for the rest
					else
					{
						waitForFurtherData();
					}
				}
			}
			catch(Error &e)
			{
				delete[] msgBuffer;
				msgBuffer = NULL;
				msgBufferSize = 0;
			}
		}
		else
		{
			deletable = true;
			listen_thread_active = false;
		}
	}
}


