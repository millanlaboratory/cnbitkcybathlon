/********************************************************
 * DEPRECATED. USE MI_CONTROL, MI_CYBATHLON_CONTROLLER
 * AND MI_CYBATHLON_UTILITIES INSTEAD.
 *
 * UTILITIES FOR DEPRECATED CYBATHLON PROTOCOLS. 
 * TO BE DELETED
 ********************************************************/

#include "mi_cybathlon_utilities_deprecated.hpp"
#include <cnbicore/CcClient.hpp>
#include <cnbicore/CcUDPListener.hpp>
#include <cnbicore/CcObject.hpp>
#include <cnbicore/CcCore.hpp>
#include <cnbicore/CcEndpoint.hpp>
#include <cnbicore/CcSocketProxy.hpp>
#include <cnbicore/CcTime.hpp>
#include <cnbicore/CcBasic.hpp>
#include <cnbicore/CcSocket.hpp>
#include <cnbicore/CcServer.hpp>
#include <cnbiloop/ClLoop.hpp>
#include <cnbiloop/ClTobiIc.hpp>
#include <cnbiloop/ClTobiId.hpp>
#include <cnbismr/CSmrBars.hpp>
#include <cnbiconfig/CCfgConfig.hpp>
#include <cnbiprotocol/CpProbability.hpp>
#include <cnbiprotocol/CpTriggerFactory.hpp>
#include <cnbiprotocol/CpTrials.hpp>
#include <iostream>
#include <getopt.h>


// Global declaration of /bus ID to allow forwarding to
// /bus upon UDP reception
ClTobiId id_glob(ClTobiId::SetOnly);
IDMessage idm_glob;
IDSerializerRapid ids_glob(&idm_glob);

class EventHandler : public CcSocketProxy {
	public:
		void HandleRecv(CcSocket* caller, CcStreamer* stream) { 
			CcUDPListener* server = (CcUDPListener*)caller;
			int* msg = (int*)(server->GetBuffer());
			printf("[EventHandler] Server received a message: %d\n",*msg);
			stream->Clear();

			idm_glob.SetDescription("mi_cybathlon");
			idm_glob.SetFamilyType(IDMessage::FamilyBiosig);
			idm_glob.SetEvent(0);
			int fidx = TCBlock::BlockIdxUnset;
			
			if(id_glob.IsAttached() == false) {
				id_glob.Attach("/bus");
			}
			idm_glob.SetEvent(*msg);
			id_glob.SetMessage(&ids_glob);
		}
		
};

void RegisterAll(CcUDPListener* server, EventHandler *handler) {
	CB_CcSocket(server->iOnRecv, handler, HandleRecv);
}


void usage(void) { 
	printf("Usage: mi_cybathlon [OPTION]\n\n");
	printf("  -s       Run synchronous\n");
	printf("  -a       Run asynchronous\n");
	printf("  -h       display this help and exit\n");
	CcCore::Exit(1);
}

int main(int argc, char *argv[]) {

	int opt;
	bool asasync = false;
	
	while((opt = getopt(argc, argv, "nh")) != -1) {
		if(opt == 's') {
			asasync = false;
		} else if(opt == 'a') {
			asasync = true;
		} else {
			usage();
			CcCore::Exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	}

	CcCore::OpenLogger("mi_cybathlon");
	CcCore::CatchSIGINT();
	CcCore::CatchSIGTERM();
	ClLoop::Configure();

	CcLogInfoS("Running " << (asasync ? "synchronous" : "asynchronous"));

	// Tools for configuration
	CCfgConfig config;
	CCfgTaskset* taskset = NULL;
	CCfgTasksetConstIt tit;

	// Parameters for experiment
	std::string xml_modality, xml_block, xml_taskset, xml_file;
	
	// Tools for feedback
	CpTrigger* trigger = NULL;
	CSmrBars* bars = NULL;

	// Tools for protocol
	dtmi p_dt;
	hwtmi p_hwt;
	gdfmi p_gdf;
	cybopts p_cybopts;
	CpProbability* probs = NULL;
	HWTrigger* t_hwt = NULL;
	GDFEvent* t_gdf = NULL;
	float* thresholds = new float[4];

	// Tools for UDP command sending
	CcClient* cybComm;
	CcEndpoint* cybCommPeer;

	// Tools for UDP event reception
	EventHandler *handler = new EventHandler();
	CcUDPListener *server;
	CcEndpoint* cybEventPeer;

	// Tools for TOBI iD on the /bus bus
	ClTobiId id(ClTobiId::SetOnly);
	IDMessage idm;
	IDSerializerRapid ids(&idm);
	idm.SetDescription("mi_cybathlon");
	idm.SetFamilyType(IDMessage::FamilyBiosig);
	idm.SetEvent(0);
	int fidx = TCBlock::BlockIdxUnset;

	// Tools for TOBI iD on the /dev bus
	ClTobiId id2(ClTobiId::SetOnly);
	devmi p_dev;
	
	// Tools for TOBI iC
	ClTobiIc ic(ClTobiIc::GetOnly);
	ICMessage icm;
	ICSerializerRapid ics(&icm);
	ICClassifier* icc = NULL;
	ICSetClassConstIter icit;
	bool waitflow = true, waitic = true;
	float progress = 0.00f;
	

	
	CcTimeValue tic; //tic for sending slide commands once timeout occurs
	int LastCommandIdx = -1;
	int comm = -1;

	// Connect to loop
	CcLogInfo("Connecting to loop...");
	if(ClLoop::Connect() == false) {
		CcLogFatal("Cannot connect to loop");
		goto shutdown;
	}
	
	// Attach iD to /bus
	if(id.Attach("/bus") == false) {
		CcLogFatal("Cannot attach iD to /bus");
		goto shutdown;
	}
	
	// Attach iD2 to /dev
	if(id2.Attach("/dev") == false) {
		CcLogFatal("Cannot attach iD to /dev");
		goto shutdown;
	}
	
	// Get configuration from nameserver
	xml_modality = ClLoop::nms.RetrieveConfig("mi", "modality");
	xml_block    = ClLoop::nms.RetrieveConfig("mi", "block");
	xml_taskset  = ClLoop::nms.RetrieveConfig("mi", "taskset");
	xml_file     = ClLoop::nms.RetrieveConfig("mi", "xml");
	CcLogConfigS("Modality=" << xml_modality <<
			", Block=" << xml_block <<
			", Taskset=" << xml_taskset << 
			", Configuration=" << xml_file);

	// Import XML file
	try {
		config.ImportFileEx(xml_file);
	} catch(XMLException e) {
		CcLogException(e.Info());
		goto shutdown;
	}

	// Configure CpTrigger using XML
	trigger = CpTriggerFactory::Find(&config);
	if(trigger == NULL) {
		CcLogFatal("Trigger not available");
		goto shutdown;
	}
	
	// Configure CSmrBars using XML
	bars = new CSmrBars(&config, trigger);

	/* Configuration block 
	 * - Get the appropriate taskset
	 * - Extract the protocol p_dt
	 * - Extract the protocol t_hwt and t_gdf
	 */
	if(mi_get_taskset_online(&config, &taskset, xml_block, xml_taskset) == false) {
		CcLogFatal("Online taskset configuration failed");
		goto shutdown;
	}
	if(mi_get_dt(&config, &p_dt) == false) {
		CcLogFatal("Online timings configuration failed");
		goto shutdown;
	}
	if(mi_get_triggers(taskset, &t_hwt, &t_gdf) == false) {
		CcLogFatal("Online triggers configuration failed");
		goto shutdown;
	}
	if(mi_get_markers(&config, &p_hwt, &p_gdf) == false) {
		CcLogFatal("Online markers configuration failed");
		goto shutdown;
	}
	if(mi_get_devmarkers(&config, &p_dev) == false) {
		CcLogFatal("Online markers configuration failed");
		goto shutdown;
	}
	if(mi_get_thresholds(taskset, thresholds) == false) {
		CcLogFatal("Online thresholds configuration failed");
		goto shutdown;
	}
	if(mi_get_cybathlon_options(&config, &p_cybopts) == false) {
		CcLogFatal("Cybathlon options configuration failed");
		goto shutdown;
	}
	CcLogInfo("Protocol configured");
	
	// Attach iC
	if(ic.Attach(taskset->ndf.ic) == false) {
		CcLogFatal("Cannot attach iC");
		goto shutdown;
	}
	
	/* Finish configuration:
	 * - initialize the control probabilities
	 * - set the number of classes for the bars
	 */
	bars->SetParameters2(taskset->Count(), thresholds);
	bars->Open();

	//Tools for UDP communication to the game
	cybCommPeer =  new CcEndpoint();
	cybComm = new CcClient();
	cybCommPeer->SetIp(p_cybopts.gameip);
	cybCommPeer->SetPortUInt(p_cybopts.gameport);

	while(cybComm->Connect(cybCommPeer->GetAddress(), CcSocket::UDP) == false) {
		CcLogInfo("Waiting for connection to UDP game command server...");
		CcTime::Sleep(1000);
	}
	CcLogInfo("Connection to UDP game command server established...");

	//Tools for UDP reception from the game
	cybEventPeer =  new CcEndpoint();
	cybEventPeer->SetIp(p_cybopts.bciip);
	cybEventPeer->SetPortUInt(p_cybopts.bciport);
	server = new CcUDPListener();
	RegisterAll(server, handler);
	server->Bind(cybEventPeer->GetIp(),cybEventPeer->GetPort());
	CcLogInfo("Waiting asynchronously for UDP messages from the game...");

	/* Wait for iC flow 
	 */
	bars->SwitchScene("wait");
	while(waitflow == true) {
		if(waitic == true) {
			if(progress <= 1.00f)
				progress += 0.10f;
			else
				progress = 0.00f;
		} else {
			progress = 1.00f;
		}
		bars->ShowWait(progress);
		
		if(ClLoop::IsConnected() == false) {
			CcLogFatal("Cannot connect to loop");
			goto shutdown;
		}

		if(CcCore::receivedSIGAny.Get()) {
			CcLogWarning("User asked to go down");
			goto shutdown;
		}
		
		switch(bars->eventkeyboard.Get(0)) {
			case DTKK_ESCAPE:
				CcLogWarning("User asked to quit");
				goto shutdown;
				break;
			case DTKK_RETURN:
			case DTKK_SPACE:
				if(waitic == false)
					waitflow = false;
				break;
			default:
				break;
		}
		
		if(waitic == true) {
			switch(ic.GetMessage(&ics)) {
				case ClTobiIc::Detached:
					CcLogFatal("iC detached");
					goto shutdown;
				case ClTobiIc::HasMessage:
					CcLogInfo("iC message received");
					waitic = false;
					break;
				default:
				case ClTobiIc::NoMessage:
					break;
			}
		}
		
		if(waitflow == true)
			CcTime::Sleep(200.00f);
	}
	bars->SwitchScene("feedback");
	CcTime::Sleep(p_dt.wait);

	/* Verify incoming iC message
	 * - check if the message has the same classifier specified in the taskset
	 * - this is useful to check weather we are attached in the write place
	 * - ... or to verify is some iC client managed to attach to the wrong place
	 */
	try { 
		icc = icm.GetClassifier(taskset->classifier.id);
		CcLogConfigS("iC message verified: '" << taskset->classifier.id << 
				"' classifier found"); 
	} catch(TCException e) {
		CcLogFatalS("Wrong iC messsage: '" << taskset->classifier.id << 
				"' classifier missing");
		goto shutdown;
	}

	/* Perform the mapping between GDF labels and tasket IDs
	 * - iC does not provide a direct mapping method (aka floating labels)
	 * - we do it looping trough out the whole taskset
	 * - ... getting a task
	 * - ... checking if the classifier has that task
	 * - ... and finally performing the mapping
	 * This is the most boring part with iC mappings. Still, I have no fucking 
	 * will of fighting to have an extra field with TOBI's WP8. Plus, after
	 * fighting, I have to maintain it, so screw it. We handle it here that is
	 * more awesome. Plus, also the MEX interfaces support the very same mapping
	 * tricks.
	 */
	probs = new CpProbability(&icm, taskset->classifier.id);
	for(tit = taskset->Begin(); tit != taskset->End(); tit++) {
		CCfgTask* task = tit->second;
		if(icc->classes.Has(task->gdf) == false) {
			CcLogFatalS("Taskset class " << task->gdf << " not found in iC message");
			goto shutdown;
		}
		CcLogConfigS("Mapping taskset/iC class GDF=" << task->gdf 
				<< " '" << task->description << "'" 
				<< " as " << task->id);
		probs->MapICClass(task->gdf, task->id);	
	}
	

	// Start /dev 
	idm.SetEvent(p_dev.start);
	id2.SetMessage(&ids);

	/* Reset time, this is the beginning of the race*/
	CcTime::Tic(&tic);

	unsigned int idx_hit;
	while(true){

		/* Reset bars */
		bars->Control(probs->Priors());

		/* Continuous feedback */
		bars->ScreenSync(p_hwt.cfeedback);
		idm.SetEvent(p_gdf.cfeedback);
		id.SetMessage(&ids, TCBlock::BlockIdxUnset, &fidx);
			
		// Resume /dev 
		idm.SetEvent(p_dev.resume);
		id2.SetMessage(&ids);

		/* Continuous feedback
		 * - Cosume all the messages 
		 * - Enter main loop 
		 * - Dispatch events on /bus and /dev
		 */
		while(ic.WaitMessage(&ics) == ClTobiIc::HasMessage);
		while(true) {
			while(true) { 
				switch(ic.WaitMessage(&ics)) {
					case ClTobiIc::Detached:
						CcLogFatal("iC detached");
						goto shutdown;
						break;
					case ClTobiIc::NoMessage:
						continue;
						break;
				}
				if(icm.GetBlockIdx() > fidx) {
					CcLogDebugS("Sync iD/iC : " << fidx << "/" << icm.GetBlockIdx());
					break;
				}
			}
			probs->Update(icc);

			if(probs->Max(&idx_hit) >= thresholds[idx_hit]) {
				/* Reset time, a command has been delivered*/
				CcTime::Tic(&tic);
				probs->Hard(idx_hit);
				if(idx_hit == 0) {
					if( (CcTime::Toc(&tic) <= p_dt.timeout) && (LastCommandIdx==1)){
						LastCommandIdx = 2; //2 stands for slide
						bars->ScreenSync(p_hwt.hit);
						idm.SetEvent(p_dev.top);
						comm = p_cybopts.slide;
					}else {
						LastCommandIdx = 0;
						bars->ScreenSync(p_hwt.hit);
						idm.SetEvent(p_dev.right);
						comm = p_cybopts.speed;
					}
					//Send 3 times in a row as advised by organizers
					for(int i=0;i<2;i++){
						cybComm->Send((const void*)(&comm),sizeof(int));
					}
				} else {
					if( (CcTime::Toc(&tic) <= p_dt.timeout) && (LastCommandIdx==0)){
						LastCommandIdx = 2; //2 stands for slide
						bars->ScreenSync(p_hwt.hit);
						idm.SetEvent(p_dev.top);
						comm = p_cybopts.slide;
					}else {
						LastCommandIdx = 1;
						bars->ScreenSync(p_hwt.hit);
						idm.SetEvent(p_dev.left);
						comm = p_cybopts.jump;
					}
					//Send 3 times in a row as advised by organizers
					for(int i=0;i<2;i++){
						cybComm->Send((const void*)(&comm),sizeof(int));
					}
				}
				bars->Control(probs, idx_hit, true);
				bars->FixationCue(CSmrBars::DrawFixation);
				id.SetMessage(&ids);

				CcTime::Sleep(p_dt.boom);
				break;
			} else {
				bars->Control(probs, idx_hit, false);
			}
		}
		
		// Suspend /dev 
		idm.SetEvent(p_dev.suspend);
		id2.SetMessage(&ids);
			
		/* Handbrake */
		if(ClLoop::IsConnected() == false) {
			CcLogFatal("Cannot connect to loop");
			goto shutdown;
		}
		if(bars->eventkeyboard.Get(0) == DTKK_ESCAPE) {
			CcLogWarning("User asked to quit");
			goto shutdown;
		}
		if(CcCore::receivedSIGAny.Get()) {
			CcLogWarning("User asked to go down");
			goto shutdown;
		}

	}

shutdown:
	cybComm->Disconnect();
	if(cybComm != NULL){
		delete(cybComm);
		delete(cybCommPeer);
	}

	server->Release();	
	delete server;


	idm.SetEvent(p_dev.stop);
	id2.SetMessage(&ids);

	id.Detach();
	id2.Detach();
	
	if(bars != NULL) {
		if(bars->IsOpen())
			bars->Close();
		delete bars;
	}

	if(taskset != NULL)
		delete(taskset);
	if(trigger != NULL)
		delete(trigger);
	if(probs != NULL)
		delete(probs);
	if(t_hwt != NULL)
		delete(t_hwt);
	if(t_gdf != NULL)
		delete(t_gdf);
	CcCore::Exit(0);
}
