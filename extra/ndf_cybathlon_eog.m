
function ndf_cybathlon_eog()

% Include any required toolboxes
ndf_cybathlon_include();

% Prepare and enter main loop
try 
    % Initialize loop structure
	cl = cl_new();
    
	% Connect to the CnbiTk loop
	if(cl_connect(cl) == false)
		disp('[ndf_eogartifact] Cannot connect to CNBI Loop, killing matlab');
		exit;
    end	

    % Initialize dynamic thresholds here
    user.eog.th = 500;
    user.dynfilepath = ['/tmp/cnbitk-' getenv('USER') '/' datestr(now,'yyyymmdd') '.dynamic_parameters.txt'];
    
     % Read the XML filename from the nameserver
     % Assume always that another mi protocol has loaded the mi.xml
     % variable
    xmlpath = cl_retrieve(cl,'mi.xml');
    % Prepare config
    config = ccfg_new();
    % Import the XML file into config
    if(ccfg_importfile(config, xmlpath) == 0)
        disp('[ndf_eogartifact] Cannot load XML file');
        ccfg_delete(config);
        return;
    end

    % Retrieve configurations
    ccfg_root(config); % Go to root
    cfg.pipe =  ccfg_quickstring(config, 'classifiers/artifact/ndf/pipe');
    if(strfind(cfg.pipe,'/pipe')==1)
        try
            cfg.pipepath = ['/tmp/cl.pipe.ndf.' cfg.pipe(6:end)];
        catch
            disp('[ndf_eogartifact] Something went wrong while loading pipe name from XML...');
        end
    else
        disp('[ndf_eogartifact] Wrong pipe entry');
    end
    ccfg_root(config); % Go to root
    cfg.ic =  ccfg_quickstring(config, 'classifiers/artifact/ndf/ic');
    ccfg_root(config); % Go to root
    cfg.id =  ccfg_quickstring(config, 'classifiers/artifact/ndf/id');
    
	% Prepare NDF srtructure
	ndf.conf  = {};
	ndf.size  = 0;
	ndf.frame = ndf_frame();
	ndf.sink  = ndf_sink(cfg.pipepath);
    
    %% Create ID client objects
    ID = tid_new();
    idm = idmessage_new(); % Create ID message for both sening/receiving
    ids = idserializerrapid_new(idm); % Create ID message serializer
    % Configure ID message
    idmessage_setdescription(idm, 'io');
    idmessage_setfamilytype(idm, idmessage_familytype('biosig'));
    idmessage_dumpmessage(idm);   
    
    % Read in parameters
    ccfg_root(config); % Go to root
    cfg.gdfevent.on = ccfg_quickgdf(config,'tasks/artifact_on/gdfevent/');
    ccfg_root(config); % Go to root
    % Note by L.Tonin  <luca.tonin@epfl.ch> on 26/09/16 12:01:08
    %cfg.gdfevent.off = ccfg_quickgdf(config,'tasks/artifact_off/gdfevent/');
    
    %cl_updatelog(loop.cl, sprintf('EOGth=%f', user.eog.EOGth));

	% Pipe opening and NDF configuration
	% - Here the pipe is opened
	% - ... and the NDF ACK frame is received
	disp('[ndf_eogartifact] Receiving ACK...');
	[ndf.conf, ndf.size] = ndf_ack(ndf.sink);
    
    % Check if EOG are enabled when there are 32 channels detected
    if(ndf.conf.eeg_channels == 32)
        disp('[ndf_eogartifact] Second gUSBamp detected, enabling EOG.');
        user.enable = 1;
    else
        disp('[ndf_eogartifact] No second gUSBamp detected, disabling EOG.');        
        user.enable = 0;
    end

    %% User initializations
    % Filter EOG between [1, 10] Hz
    [user.butter.eog.b user.butter.eog.a] = butter(2,[1 10]*2/ndf.conf.sf);  
    
    % Decision buffer, holds last two detection decisions
    % 1: Artifact detected, 0: No artifact detected
    user.decbuffer = [0 0];
    
	disp('[ndf_eogartifact] Receiving NDF frames...');
	while(true)
		[ndf.frame, ndf.size] = ndf_read(ndf.sink, ndf.conf, ndf.frame);
        
		% Acquisition is down, exit
		if(ndf.size == 0)
			disp('[ndf_eogartifact] Broken pipe');
			break;
        end
        
        % Update dynamic parameters here
        pval = ndf_read_param(user.dynfilepath, 'artifact', 'artifact_eog', 'threshold');
        if(~isempty(pval))
            user.eog.th = str2num(pval);
        end
        
        if(user.enable == 1)
            
            % Isolate EOG channels. First block belongs to EOG
            buffer.eog = ndf.frame.eeg(:,17:19);
           
            %% EOG processing here if enabled
            % Make horizontal and vertical bipolar channels
            % Assume 1=17= Right eye canthus,2=18= Nasion, 3=19=Left eye
            % canthus 4=20=Forhead (REF)
            user.eog.EOGv = buffer.eog(:,1)-buffer.eog(:,2);
            user.eog.EOGh = buffer.eog(:,2) - ...
                (buffer.eog(:,1) + buffer.eog(:,3))/2;
            %% Monopolar for blinks
            user.eog.EOGblink = buffer.eog;

            % Apply Butterworth bandpass in [1 10] Hz
            user.eog.EOGh = filter(user.butter.eog.b,user.butter.eog.a,user.eog.EOGh);
            user.eog.EOGv = filter(user.butter.eog.b,user.butter.eog.a,user.eog.EOGv);
            user.eog.EOGblink = filter(user.butter.eog.b,user.butter.eog.a,user.eog.EOGblink);

            % Rectify 
            user.eog.EOGh = abs(user.eog.EOGh);
            user.eog.EOGv = abs(user.eog.EOGv);
            user.eog.EOGblink = mean(abs(user.eog.EOGblink),2);

            % Thresholding and decision 
            if ( (sum(user.eog.EOGh > user.eog.th) > 0) || (sum(user.eog.EOGv > user.eog.th) > 0) || (sum(user.eog.EOGblink > user.eog.th) > 0) )
                user.dec.eog = 1;
            else
                user.dec.eog = 0;
            end
            
            % Update decision buffer 
            user.decbuffer = [user.decbuffer(end) user.dec.eog]; 
            
            % Send TiD message
            if(tid_isattached(ID) == true)
                
                if(isequal(user.decbuffer,[0 1]))
                    % Artifact onset
                    idmessage_setevent(idm, cfg.gdfevent.on);
                    tid_setmessage(ID, ids, ndf.frame.index);
                end
               
			
		% Note by L.Tonin  <luca.tonin@epfl.ch> on 26/09/16 12:00:12	
		% Removed the offset event (it is sent by 
		% mi_cybathlon_controller)
                %if(isequal(user.decbuffer,[1 0]))
                %    % Artifact offset
                %    idmessage_setevent(idm, cfg.gdfevent.off);
                %    tid_setmessage(ID, ids, ndf.frame.index);
                %end
              
            else
                tid_attach(ID, cfg.id);
            end
        end
    end
    
catch exception
	ndf_printexception(exception);
     % Tear down loop structure
    tid_detach(ID);
    ndf_close(ndf.sink); 
    idserializerrapid_delete(ids);
    idmessage_delete(idm); 
	tid_delete(ID);
    fclose('all');   
	disp('[ndf_eogartifact] Going down');
end
