#include <iostream>
#include <getopt.h>

#include <cnbicore/CcCore.hpp>
#include <cnbicore/CcTime.hpp>
#include <cnbicore/CcBasic.hpp>

#include <cnbicore/CcSocket.hpp>
#include <cnbicore/CcSocketProxy.hpp>
#include <cnbicore/CcEndpoint.hpp>

#include <cnbicore/CcServer.hpp>
#include <cnbicore/CcClient.hpp>
#include <cnbicore/CcUDPListener.hpp>

#include <cnbiloop/ClLoop.hpp>
#include <cnbiloop/ClTobiId.hpp>
#include <cnbiconfig/CCfgConfig.hpp>

#include <cnbiprotocol/cp_utilities.hpp>

#include <cnbiprotocols/artifact_utilities.hpp>
#include "mi_cybathlon_utilities.hpp"

class ListenerHandler : public CcSocketProxy {
	public:
		ListenerHandler(cybcfg_t* cybcfg) {
			this->cybcfg_ = cybcfg;
			this->id_    = new ClTobiId(ClTobiId::SetOnly);;
			this->idm_   = new IDMessage;
			this->ids_   = new IDSerializerRapid(this->idm_);
			this->idm_->SetDescription("mi_cybathlon");
			this->idm_->SetFamilyType(IDMessage::FamilyBiosig);
			this->idm_->SetEvent(0);

			// Attach iD to /bus
			if(this->id_->Attach("/bus") == false) {
				CcLogFatal("Listener - Cannot attach iD to /bus");
			}
		}
		~ListenerHandler(void) {
			this->id_->Detach();
			delete this->idm_;
			delete this->ids_;
		}

		void HandleRecv(CcSocket* caller, CcStreamer* stream) { 
			bool isvalid = true;
			unsigned int* gameevent;
			unsigned int  gdfevent;
			
			CcUDPListener* server = (CcUDPListener*)caller;
			gameevent = (unsigned int*)(server->GetBuffer());
			stream->Clear();
			
			if (event2gdf(gameevent, &gdfevent)) {
				
				// Sending event
				this->idm_->SetEvent(gdfevent);
				this->id_->SetMessage(this->ids_);
				
				CcLogInfoS("Player in "<<padnames[*gameevent % 10] 
					   <<" pad (gdfevent|"<< gdfevent<<")");

			}
		}

	private:
		bool event2gdf(unsigned int* gameevent, unsigned int* gdfevent) {
			bool retcode = false;
			for(auto it=cybcfg_->events.begin(); it!=cybcfg_->events.end(); it++) {
				if(it->pad == *gameevent) {
					*gdfevent = it->gdfevent;
					retcode = true;
					break;
				}
			}
			return retcode;
		}

	public:
		ClTobiId* 	   id_;
		IDMessage* 	   idm_;
		IDSerializerRapid* ids_;
		cybcfg_t*	   cybcfg_;
		std::vector<std::string> padnames {"empty", "speed", "jump", "slide"};
};

void RegisterAll(CcUDPListener* server, ListenerHandler *handler) {
	CB_CcSocket(server->iOnRecv, handler, HandleRecv);
};
 
int main (int argc, char** argv) {


	/********************************/
	/*   Variables initialization   */
	/********************************/
	// Parameters for experiment
	std::string xml_file;

	// Tools for configuration
	CCfgConfig config;
	cybcfg_t   cybcfg;
	artcfg_t   artcfg;
	GDFEvent   event;

	// Artifact variables
	bool       artifact = false;

	// Cybathlon commands
	bool 		cmdflg = false;
	cybevt_t* 	evtrcv_c = NULL;
	cybevt_t* 	evtrcv_p = NULL;
	cybevt_t* 	evtdel_c = NULL;
	cybevt_t* 	evtdel_p = NULL;
	CcTimeValue 	cmdtic;
	CcTimeValue 	arttic;
	float 		cmdtime;
	float 		arttime;
	
	// Tools for TOBI iD on the /bus bus
	ClTobiId id(ClTobiId::SetGet);
	IDMessage idm;
	IDSerializerRapid ids(&idm);
	idm.SetDescription("mi_cybathlon");
	idm.SetFamilyType(IDMessage::FamilyBiosig);
	idm.SetEvent(0);
	int fidx = TCBlock::BlockIdxUnset;
	
	// Tools for UDP Connection
	CcClient*    	CybGames = NULL;
	CcAddress 	cybaddress;
	CcUDPListener 	*server = NULL;
	ListenerHandler *handler = NULL;
	
	/********************************/
	/*   CNBITK Loop Configuration  */
	/********************************/
	// General CNBI configuration
	CcCore::OpenLogger("mi_cybathlon_controller");
	CcCore::CatchSIGINT();
	CcCore::CatchSIGTERM();
	ClLoop::Configure();

	// Connect to loop
	CcLogInfo("Connecting to loop...");
	if(ClLoop::Connect() == false) {
		CcLogFatal("Cannot connect to loop");
		CcCore::Exit(0);
	}
	CcLogInfo("Loop connected");
	
	// Attach iD to /bus
	if(id.Attach("/bus") == false) {
		CcLogFatal("Cannot attach iD to /bus");
		CcCore::Exit(0);
	}
	
	// Get configuration from nameserver
	xml_file = ClLoop::nms.RetrieveConfig("mi", "xml");
	if(xml_file.empty() == true) {
		CcLogFatal("Xml file not found in the nameserver");
		CcCore::Exit(0);
	}
	CcLogConfigS("Configuration=" << xml_file);

	/********************************/
	/*       XML Configuration      */
	/********************************/
	CcLogInfo("Importing xml configuration...");
	try {
		config.ImportFileEx(xml_file);
	} catch(XMLException e) {
		CcLogException(e.Info());
		CcCore::Exit(0);
	}
	CcLogInfo("Xml configuration correctly imported");

	// Retrieve xml information for events configuration
	if(mi_cybathlon_configure_events(&config, &cybcfg) == false) {
		CcLogFatal("Cybathlon events configuration failed");
		goto shutdown;
	}
	// Retrieve xml information for network configuration
	if(mi_cybathlon_configure_network(&config, &cybcfg) == false) {
		CcLogFatal("Cybathlon network configuration failed");
		goto shutdown;
	}
	//Retrieve xml information for artifact configuration
	if(artifact_configure_events(&config, &artcfg) == false) {
		CcLogWarning("Artifacts structure not configured");
	}

	CcLogInfo("Cybathlon controller configured");
	mi_cybathlon_configure_dump(&cybcfg);

	/********************************/
	/*    Network Configuration     */
	/********************************/
	CybGames  = new CcClient[cybcfg.gameaddress.size()];
	for (auto i=0; i<cybcfg.gameaddress.size(); i++) {
		cybaddress = cybcfg.gameaddress.at(i);
		while(CybGames[i].Connect(cybaddress, CcSocket::UDP) == false) {
			CcLogInfoS("Waiting for connection to the game ("<<cybaddress<<")...");
			if(CcCore::receivedSIGAny.Get()) { goto shutdown; }
			CcTime::Sleep(1000);
		}
		CcLogInfoS("Connection to the game ("<<cybaddress<<") established");
	}
	
	handler = new ListenerHandler(&cybcfg);
	server  = new CcUDPListener();
	RegisterAll(server, handler);
	server->Bind(cybcfg.bciaddress);
	CcLogInfoS("Listener configured ("<<cybcfg.bciaddress<<")");

	/********************************/
	/*     Starting controller      */
	/********************************/
	evtrcv_c = new cybevt_t;
	evtrcv_p = new cybevt_t;
	evtdel_c = new cybevt_t;
	evtdel_p = new cybevt_t;
	CcTime::Tic(&cmdtic);
	CcTime::Sleep(cybcfg.timerevert);

	while (true) {

		// Logic to delivery commands
		if(id.GetMessage(&ids) == true) {
			
			event = idm.GetEvent();

			// Iterate on cybathlon structure to compare the
			// received commands with those in the structure
			for(auto it=cybcfg.events.begin(); it!=cybcfg.events.end(); it++) {
				if(event == it->gdfevent + cybcfg.devevent) {
					// Copy the previous command
					evtrcv_p = new cybevt_t(*evtrcv_c);
					// Initialize the new command
					evtrcv_c = new cybevt_t(*it);
					// Setting true the receive flag
					cmdflg = true;
					// Store the time elapsed between the
					// two last commands
					cmdtime = CcTime::Toc(&cmdtic);
					// Update the tic
					CcTime::Tic(&cmdtic);
					break;
				}
			}

			// Copy the current command to the delivery command
			evtdel_c = new cybevt_t(*evtrcv_c);
			
			// Implement third command (reverse)
			// If two different commands are sent in the required
			// time and the previous command delivered was not 
			// "mi_reverse", then the third command is delivered. 
			// The third command is identified by the task "mi_reverse"
			if((cmdtime <= cybcfg.timerevert) && 
			   (evtrcv_c->gdfevent != evtrcv_p->gdfevent) &&
			   (evtdel_p->key.compare("mi_reverse") != 0)) {

				for(auto it=cybcfg.events.begin(); it!=cybcfg.events.end(); it++) {
					if(it->key.compare("mi_reverse") == 0) {
						evtdel_c = new cybevt_t(*it);
						break;
					}
				}
			}

			// Artifact detection
			if (event == artcfg.on.gdfevent) {
				CcLogInfoS("Artifact detection (event|"<<artcfg.on.gdfevent<<") | Command disabled ("<<artcfg.on.timeout<<" ms)"); 
				artifact = true;
				CcTime::Tic(&arttic);
			} 
		
		}
		
		// Check for artifact timeout
		arttime = CcTime::Toc(&arttic);
		if(artifact == true && arttime >= artcfg.on.timeout) {
			CcLogInfoS("Artifact detection timeout: Commands enabled"); 
			artifact = false;
			cmdflg = false;
			idm.SetEvent(artcfg.off.gdfevent);
			id.SetMessage(&ids);
		}
	
		// Sending command procedures
		if (cmdflg == true && artifact == false) {
			// Sending the commands 3 times as suggested by
			// organizers
			for (auto i=0; i<cybcfg.gameaddress.size(); i++) {
				CybGames[i].Send((const void*)(&evtdel_c->command),sizeof(unsigned int));
				CcTime::Sleep(25);
			}
		
			CcLogInfoS("Player command: " <<
				   evtdel_c->name << "|" << evtdel_c->command << 
				   " (" << evtdel_c->key << "|" << evtdel_c->gdfevent << ")");
		
			// Command has been sent. Switch command flag to false
			// and save it as previous delivered command
			cmdflg  = false;
			evtdel_p = new cybevt_t(*evtdel_c);
		}

		// Update artifact timeout parameter
		if(cp_parameter_update("artifact", "artifact_eog", "timeout", &artcfg.on.timeout)) {
			CcLogConfigS("Timeout changed to "<<
				     artcfg.on.timeout<<" for artifact|artifact_eog");
		}



		CcTime::Sleep(25);
		if(CcCore::receivedSIGAny.Get()) {
			CcLogWarning("User asked to go down");
			goto shutdown;
		}

	}
	

shutdown:
	
	// Game clients shutdown
	for(unsigned int i=0; i<cybcfg.gameaddress.size(); i++)
		CybGames[i].Disconnect();

	// BCI server shutdown
	if (server != NULL)
		server->Release();
	delete server;
	
	delete evtrcv_p;
	delete evtrcv_c;
	delete evtdel_c;
	delete evtdel_p;

	id.Detach();
	CcCore::Exit(0);
}
