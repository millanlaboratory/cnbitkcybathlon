#ifndef MI_CYBATHLON_UTILITIES_H
#define MI_CYBATHLON_UTILITIES_H

#include <string>
#include <cnbicore/CcBasic.hpp>
#include <cnbiconfig/CCfgConfig.hpp>

/*********************************** 
 * Struct and function definitions *
 ***********************************/

/**
 * Structure to represent cybathlon event
 */
typedef struct {
	/*@{*/
	std::string  name; 	/**< the event name **/
	std::string  key;	/**< the bci task label (e.g., mi_both_hands) **/
	unsigned int gdfevent;	/**< the bci task gdf event (e.g., 773) **/
	unsigned int command;	/**< the cybathlon related command (e.g., 11 for player 1; 21 for player two) **/
	unsigned int pad; 	/**< the cybathlon pad code **/
	/*@}*/
} cybevt_t;

/**
 * Structure to represent cybathlon configuration
 */
typedef struct {
	/*@{*/
	unsigned int 		 player;	/**< the player number **/
	std::vector<std::string> gameaddress;	/**< the game addresses **/
	std::string		 bciaddress;	/**< the bci address **/
	std::vector<cybevt_t> 	 events;	/**< the cybathlon events **/
	unsigned int 		 devevent;	/**< the device event mask code **/
	float 	     		 timerevert;	/**< the timeout to implement the third command **/
	/*@}*/
} cybcfg_t;

/*!
 Configures cybathlon events read from xml config structure. It raises exception if errors occur.
 @param[in]	config	Pointer to XML config class 
 @param[in]  	cybcfg	Pointer to cybathlon config structure
 @return		True if the cybcfg is configured correctly
 */
bool mi_cybathlon_configure_events(CCfgConfig* config, cybcfg_t* cybcfg);

/*!
 Configures cybathlon networks read from xml config structure. It raises exception if errors occur.
 @param[in]	config	Pointer to XML config class 
 @param[in]  	cybcfg	Pointer to cybathlon config structure
 @return		True if the cybcfg is configured correctly
 */
bool mi_cybathlon_configure_network(CCfgConfig* config, cybcfg_t* cybcfg);

/*!
 Dump cybathlon configurations. It raises exception if errors occur.
 @param[in]  	cybcfg	Pointer to cybathlon config structure
 */
void mi_cybathlon_configure_dump(cybcfg_t* cybcfg);


/*********************************** 
 *    Function implementations     *
 **********************************/
bool mi_cybathlon_configure_events(CCfgConfig* config, cybcfg_t* cybcfg) {
	XMLType converter;
	
	try {
		// Retrieve player number
		config->RootEx()->QuickEx("controller/cybathlon")->SetBranch();
		cybcfg->player  = config->BranchEx()->QuickIntEx("player");
		
		
		// Retrieve and associate event/task/commands 
		config->RootEx()->QuickEx("controller/cybathlon/event")->SetBranch();
		XMLNode node = config->Leaf();
		
		while(true) {
			if(node == NULL)
				break;

			cybevt_t event;
			converter.Guess(node->first_attribute("name")->value());
			event.name = converter.String();
			converter.Guess(node->first_attribute("key")->value());
			event.key  = converter.String();
			converter.Guess(node->first_attribute("command")->value());
			event.command = cybcfg->player*10 + (unsigned int)converter.Int();
			converter.Guess(node->first_attribute("pad")->value());
			event.pad = cybcfg->player*10 + (unsigned int)converter.Int();
			
			cybcfg->events.push_back(event);
			node = node->next_sibling("event");
		}
		
		// Compose events for device commands 
		config->RootEx()->QuickEx("tasks")->SetBranch();
		for(auto it = cybcfg->events.begin(); it!=cybcfg->events.end(); it++)
			it->gdfevent = config->BranchEx()->QuickGDFIntEx(it->key + "/gdfevent");
		
		// Retrieve time to revert
		config->RootEx()->QuickEx("controller/cybathlon")->SetBranch();
		cybcfg->timerevert = config->BranchEx()->QuickFloatEx("timerevert");
		
		// Retrieve device command mask
		config->RootEx()->QuickEx("events/gdfevents")->SetBranch();
		cybcfg->devevent = config->BranchEx()->QuickGDFIntEx("device");

	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

bool mi_cybathlon_configure_network(CCfgConfig* config, cybcfg_t* cybcfg) {
	XMLType converter;
	try {
		// Retrieve all the address fields	
		cybcfg->gameaddress.clear();
		config->RootEx()->QuickEx("controller/cybathlon/network/address")->SetBranch();
		XMLNode node = config->Leaf();
		while(true) {
			if(node == NULL)
				break;
				
			std::string atype;
			std::string address;
			converter.Guess(node->first_attribute("type")->value());
			atype = converter.String();
			converter.Guess(node->value());
			address = converter.String();

			if (atype.compare("game") == 0)
				cybcfg->gameaddress.push_back(address);
			else if(atype.compare("bci") == 0)
				cybcfg->bciaddress = address;
		
			node = node->next_sibling("address");
		}

	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
};

void mi_cybathlon_configure_dump(cybcfg_t* cybcfg) {

	// Dump player number
	CcLogInfoS("Game    configuration: player|" << cybcfg->player); 
	// Dump commands configuration
	for (auto it=cybcfg->events.begin(); it!=cybcfg->events.end(); it++)
		CcLogInfoS("Event   configuration: "<<it->name<<"|"<<it->command<<"\t("<<it->key<<"|"<<it->gdfevent <<")");
	// Dump device event value
	CcLogInfoS("Event   configuration: device|" << cybcfg->devevent);
	
	// Dump game addresses
	for (auto it=cybcfg->gameaddress.begin(); it!=cybcfg->gameaddress.end(); it++)
		CcLogInfoS("Network configuration: game|" << *it);
	// Dump bci address
		CcLogInfoS("Network configuration:  bci|"<<cybcfg->bciaddress);
};

#endif
