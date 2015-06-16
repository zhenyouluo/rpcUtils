/*
 * PluginInterface.cpp
 *
 *  Created on: 16.06.2015
 *      Author: dave2
 */

#include "PluginInterface.hpp"

PluginInterface::PluginInterface(PluginInfo* plugin)
{
	pluginActive = false;

	sigemptyset(&origmask);
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGUSR1);
	sigaddset(&sigmask, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigmask, &origmask);

	this->pluginNumber = plugin->getPluginNumber();
	this->pluginName = plugin->getName();
	this->pluginPath = plugin->getUdsFilePath();


	optionflag = 1;
	connection_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, pluginPath->c_str(), pluginPath->size());
	addrlen = sizeof(address);
	infoIn.logLevel = LOG_INPUT;
	infoIn.logName = "IPC IN:";
	infoOut.logLevel = LOG_OUTPUT;
	infoOut.logName = "IPC OUT:";
	info.logLevel = LOG_INFO;
	info.logName = "Plugin:";

	pthread_mutex_init(&wLmutex, NULL);
	unlink(pluginPath->c_str());
	setsockopt(connection_socket, SOL_SOCKET, SO_REUSEADDR, &optionflag, sizeof(optionflag));
	bind(connection_socket, (struct sockaddr*)&address, addrlen);

	regClient = NULL;

}


PluginInterface::~PluginInterface()
{
	pthread_t accepter = getAccepter();
	if(accepter != 0)
		pthread_cancel(accepter);

	WaitForAcceptThreadToExit();

	deleteComPointList();
	close(connection_socket);
	pthread_mutex_destroy(&wLmutex);
}


void PluginInterface::start()
{
	try
	{
		regClient->connectToRSD();
		regClient->sendAnnounceMsg();

		pluginActive = true;
		while(pluginActive)
		{
			sleep(WAIT_TIME);
			checkForDeletableWorker();
			if(regClient->isDeletable())
				pluginActive = false;
		}
	}
	catch(Error &e)
	{
		log(logInfo, e.get());
		pluginActive = false;
	}
}


void PluginInterface::deleteComPointList()
{
	list<ComPoint*>::iterator worker= comPointList.begin();

	while(worker != comPointList.end())
	{
		delete *worker;
		worker = comPointList.erase(worker);
	}
}


void PluginInterface::pushComPointList(ComPoint* comPoint)
{
	pthread_mutex_lock(&wLmutex);
	comPointList.push_back(comPoint);
	//dyn_print("Uds---> Number of UdsWorker: %d \n", comPointList.size());
	pthread_mutex_unlock(&wLmutex);
}


void PluginInterface::checkForDeletableWorker()
{
	pthread_mutex_lock(&wLmutex);

	list<ComPoint*>::iterator i = comPointList.begin();
	while(i != comPointList.end())
	{
		if((*i)->isDeletable())
		{
			//dyn_print("Uds---> Deleting UdsWorker. %d still left.\n", comPointList.size()-1);
			delete (*i)->getProcessInterface(); //delete AardvarkCareTarker
			delete *i;
			i = comPointList.erase(i);
		}
		else
			++i;
	}
	pthread_mutex_unlock(&wLmutex);
}

