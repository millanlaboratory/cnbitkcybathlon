#ifndef MI_UTILITIES_DEPRECATED_H
#define MI_UTILITIES_DEPRECATED_H

/********************************************************
 * DEPRECATED. USE MI_CONTROL, MI_CYBATHLON_CONTROLLER
 * AND MI_CYBATHLON_UTILITIES INSTEAD.
 *
 * UTILITIES FOR DEPRECATED CYBATHLON PROTOCOLS. 
 * TO BE DELETED
 ********************************************************/
#include <string>
#include <cnbicore/CcBasic.hpp>
#include <cnbiconfig/CCfgConfig.hpp>
#include <cnbismr/CSmrCopilot.hpp> 
#include <cnbiprotocol/CpTrials.hpp>

typedef struct dtmi_struct {
	float wait;
	float begin;
	float end;
	float waitmin;
	float waitmax;
	float fixation;
	float cue;
	float cfeedback;
	float boom;
	float timeout;
} dtmi;

typedef struct gdfmi_struct {
	GDFEvent wait;
	GDFEvent fixation;
	GDFEvent cfeedback;
	GDFEvent off;
	GDFEvent hit;
	GDFEvent miss;
	GDFEvent timeout;
} gdfmi;

typedef struct hwtmi_struct {
	HWTrigger wait;
	HWTrigger fixation;
	HWTrigger cfeedback;
	HWTrigger off;
	HWTrigger hit;
	HWTrigger miss;
	HWTrigger timeout;
} hwtmi;

typedef struct devmi_struct {
	GDFEvent right;
	GDFEvent left;
	GDFEvent start;
	GDFEvent stop;
	GDFEvent suspend;
	GDFEvent resume;
	GDFEvent top;
} devmi;

typedef struct cyb_opt {
	std::string gameip;
	int gameport;
	std::string bciip;
	int bciport;
	int speed;
	int jump;
	int slide;
} cybopts;

bool mi_get_taskset_offline(CCfgConfig* config, CCfgTaskset** taskset,
		std::string bname, std::string tname) {
	try { 
		*taskset = config->OfflineEx(bname, tname);
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

bool mi_get_taskset_online(CCfgConfig* config, CCfgTaskset** taskset,
		std::string bname, std::string tname) {
	try { 
		*taskset = config->OnlineEx(bname, tname);
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}


bool mi_get_triggers(CCfgTaskset* taskset, HWTrigger** triggers, GDFEvent** events) {
	CCfgTasksetConstIt it;
	*triggers = new HWTrigger(taskset->Count());
	*events = new GDFEvent(taskset->Count());
	for(it = taskset->Begin(); it != taskset->End(); it++) {
		CCfgTask* task = it->second;
		(*triggers)[task->id] = task->hwt;
		(*events)[task->id] = task->gdf;
	}
	return true;
}

bool mi_configure_copilot(CCfgConfig* config, CSmrCopilot* copilot, 
		std::string& block, std::string& taskset) {
	/* Check if the offline block is ok
	 */
	CCfgTaskset* offline = NULL;
	try { 
		offline = config->OfflineEx(block, taskset);
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}

	// Reading scalar parameters
	unsigned int granularity;
	float threshold, viscosity;
	try {
		config->RootEx()->GoEx("offline")->SetBranch();
		threshold = config->BranchEx()->GoEx(block)->QuickFloatEx("threshold");
		viscosity = config->BranchEx()->GoEx(block)->QuickFloatEx("viscosity");
		granularity = config->BranchEx()->GoEx(block)->QuickIntEx("granularity");
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	copilot->SetClassNumber(offline->Count());
	copilot->SetControlGranularity(granularity);
	copilot->SetThreshold(threshold);
	copilot->SetCoefficientViscosity(viscosity);
	
	// Readi duration of trial
	float cfeedback;
	try {
		cfeedback = config->RootEx()->
			QuickFloatEx("protocol/mi/trial/cfeedback");
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	copilot->SetTrialDuration(cfeedback);
	
	// Read trials array
	unsigned int* trials = new unsigned int(offline->Count());
	CCfgTasksetConstIt it;
	for(it = offline->Begin(); it != offline->End(); it++) {
		CCfgTask* task = it->second;
		if(task->HasConfig("trials") == false) {
			CcLogErrorS("Task " << task->name << " does not define \"trials\"");
			return false;
		}
		trials[task->id] = task->config["trials"].Int();
	}
	copilot->SetTrialNumber(trials);
	delete trials;

	return true;
}

bool mi_configure_trials(CCfgTaskset* taskset, CpTrials* trials) {
	CCfgTasksetConstIt it;
	for(it = taskset->Begin(); it != taskset->End(); it++) {
		CCfgTask* task = it->second;
		if(task->HasConfig("trials") == false) {
			CcLogErrorS("Task " << task->name << " does not define \"trials\"");
			return false;
		}
		trials->AddClassLabel(task->id, task->config["trials"].Int());
	}
	trials->Create();
	trials->Shuffle();
	return true;
}

bool mi_get_thresholds(CCfgTaskset* taskset, float* thresholds) {
	CCfgTasksetConstIt it;

	for(it = taskset->Begin(); it != taskset->End(); it++) {
		CCfgTask* task = it->second;
		
		if(task->HasConfig("threshold") == false) {
			CcLogErrorS("Task " << task->name << " does not define \"threshold\"");
			return false;
		}
		thresholds[task->id] = task->config["threshold"].Float();
		CcLogConfigS("Class " << task->id << ", threshold " << thresholds[task->id]);
	}
	return true;
}

bool mi_get_dt(CCfgConfig* config, dtmi* timings) {
	try {
		config->RootEx()->QuickEx("protocol/mi/")->SetBranch();
		timings->wait = config->BranchEx()->QuickFloatEx("scenes/wait");
		timings->begin = config->BranchEx()->QuickFloatEx("scenes/begin");
		timings->end= config->BranchEx()->QuickFloatEx("scenes/end");
		timings->waitmin = config->BranchEx()->QuickFloatEx("trial/waitmin");
		timings->waitmax = config->BranchEx()->QuickFloatEx("trial/waitmax");
		timings->fixation = config->BranchEx()->QuickFloatEx("trial/fixation");
		timings->cue = config->BranchEx()->QuickFloatEx("trial/cue");
		timings->boom = config->BranchEx()->QuickFloatEx("trial/boom");
		timings->timeout = config->BranchEx()->QuickFloatEx("trial/timeout");
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

bool mi_get_markers(CCfgConfig* config, hwtmi* hwt, gdfmi* gdf) {
	try {
		config->RootEx()->QuickEx("events")->SetBranch();
		gdf->wait = config->BranchEx()->QuickGDFIntEx("gdfevents/wait");
		gdf->fixation = config->BranchEx()->QuickGDFIntEx("gdfevents/fixation");
		gdf->cfeedback = config->BranchEx()->QuickGDFIntEx("gdfevents/cfeedback");
		gdf->off = config->BranchEx()->QuickGDFIntEx("gdfevents/off");
		gdf->hit = config->BranchEx()->QuickGDFIntEx("gdfevents/targethit");
		gdf->miss = config->BranchEx()->QuickGDFIntEx("gdfevents/targetmiss");
		gdf->timeout = config->BranchEx()->QuickGDFIntEx("gdfevents/timeout");
		
		hwt->wait = config->BranchEx()->QuickIntEx("hwtriggers/wait");
		hwt->fixation = config->BranchEx()->QuickIntEx("hwtriggers/fixation");
		hwt->cfeedback = config->BranchEx()->QuickIntEx("hwtriggers/cfeedback");
		hwt->off = config->BranchEx()->QuickIntEx("hwtriggers/off");
		hwt->hit = config->BranchEx()->QuickIntEx("hwtriggers/targethit");
		hwt->miss = config->BranchEx()->QuickIntEx("hwtriggers/targetmiss");
		hwt->timeout = config->BranchEx()->QuickIntEx("hwtriggers/timeout");

	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

bool mi_get_devmarkers(CCfgConfig* config, devmi* dev) {
	try {
		config->RootEx()->QuickEx("events")->SetBranch();
		dev->right = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/right");
		dev->left = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/left");
		dev->top = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/top");
		dev->start = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/start");
		dev->stop = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/stop");
		dev->suspend = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/suspend");
		dev->resume = config->BranchEx()->QuickGDFIntEx("gdfevents/cnbi/resume");
		
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

bool mi_get_cybathlon_options(CCfgConfig* config, cybopts* opts) {
	try {
		config->RootEx()->QuickEx("options")->SetBranch();
		opts->gameip = config->BranchEx()->QuickStringEx("cybathlon/gameip");
		opts->gameport = config->BranchEx()->QuickIntEx("cybathlon/gameport");
		opts->bciip = config->BranchEx()->QuickStringEx("cybathlon/bciip");
		opts->bciport = config->BranchEx()->QuickIntEx("cybathlon/bciport");
		opts->speed = config->BranchEx()->QuickIntEx("cybathlon/speed");
		opts->jump = config->BranchEx()->QuickIntEx("cybathlon/jump");
		opts->slide = config->BranchEx()->QuickIntEx("cybathlon/slide");
		
	} catch(XMLException e) {
		CcLogException(e.Info());
		return false;
	}
	return true;
}

#endif
