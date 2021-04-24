
/* In order to generate input for this, please go to $UCESB_DIR and run:
 *
 * Additional info:
 * To generate the header file used for the R3BUcesbSource (ext_h101.h), use:
 *
 * $UCESB_DIR/upexps/s438b/s438b --ntuple=UNPACK:TRIGGER,UNPACK:EVENTNO,RAW\
 *     STRUCT_HH,ext_h101.h
 *
 * Put this header file into the 'r3bsource' directory and recompile.
 * */

struct EXT_STR_h101_t
{
	EXT_STR_h101_unpack_t unpack;
	EXT_STR_h101_SCI2_t s2;
};

void calib_sci2(Int_t RunId = 1)
{
  TString runNumber=Form ("%03d", RunId);
	TStopwatch timer;
	timer.Start();

  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
	

	const Int_t nev = -1; /* number of events to read, -1 - until CTRL+C */
	
	// --- ------------------------------------------------------ ---	
	// --- check and modify if necessary ------------------------ ---
	// --- for unpacking ---------------------------------------- ---
	// --- ------------------------------------------------------ ---	
	// --- for local computer
	TString filename = "~/data/s515/main*.lmd --allow-errors --input-buffer=50Mi"; 
  TString ucesb_dir  = getenv("UCESB_DIR"); 
  TString ucesb_path = ucesb_dir + "/../upexps/202104_s515/202104_s515";  
  ucesb_path.ReplaceAll("//", "/");
	
	// --- land account
	//TString filename = "/d/land5/202104_s515/lmd/main*.lmd --allow-errors --input-buffer=50Mi"; 
	//TString filename = " --stream=lxir136:9001 --allow-errors --input-buffer=50Mi"; 
	//TString ucesb_path = "/u/land/fake_cvmfs/9.13/upexps/202104_s515/202104_s515";
	
	TString ntuple_options = "RAW:SCITWO";                    // ntuple options for stitched data  
	//TString ntuple_options = "RAW,time_stitch=1000"; // ntuple_options for raw data

 
	// --- ------------------------------------------------------ ---	
	// --- check and modify if necessary ------------------------ ---
  // --- for online run --------------------------------------- --- 
	// --- ------------------------------------------------------ ---	
	TString output_path = "~/data/s515/";
	//TString output_path = "/d/land5/202104_s515/rootfiles/beam/";
	TString outputFilename = output_path+"s515_tcal_sci2_" + oss.str() + ".root";
  const Int_t refresh = 1;                 
  Int_t port = 5555;
	

  EXT_STR_h101_t ucesb_struct;
	R3BUcesbSource* source = new R3BUcesbSource(filename, ntuple_options,	ucesb_path, &ucesb_struct, sizeof(ucesb_struct));
	source->SetMaxEvents(nev);
	source->AddReader(new R3BUnpackReader(&ucesb_struct.unpack,offsetof(EXT_STR_h101_t, unpack)));
  source->AddReader( new R3BSci2Reader (&ucesb_struct.s2, offsetof(EXT_STR_h101_t, s2)) );

  /* Create online run ------------------------------------ */
#define RUN_ONLINE
#define USE_HTTP_SERVER
#ifdef RUN_ONLINE
  FairRunOnline* run = new FairRunOnline(source);
  run->SetRunId(RunId);
#ifdef USE_HTTP_SERVER
  run->ActivateHttpServer(refresh);
#endif
#else
  /* Create analysis run ---------------------------------- */
  FairRunAna* run = new FairRunAna();
#endif

    run->SetOutputFile(outputFilename.Data());


	/* Runtime data base ------------------------------------ */
	FairRuntimeDb* rtdb = run->GetRuntimeDb();
	Bool_t kParameterMerged = kTRUE;
	TString parFileName    = "parameter/tcal_s2.root";  // name of parameter file	"+runNumber+"
	FairParRootFileIo* parOut = new FairParRootFileIo(kParameterMerged);
	parOut->open(parFileName);
    rtdb->setFirstInput(parOut);
    rtdb->setOutput(parOut);


	/* Calibrate Sci2 ---------------------------------------- */
    const Int_t updateRate = 1000;
    const Int_t minStats = 10000;        // minimum number of entries for TCAL calibration
    const Int_t trigger = 1;
    R3BSci2Mapped2CalPar* s2Calibrator = new R3BSci2Mapped2CalPar("R3BSci2Mapped2CalPar", 1);
    s2Calibrator->SetUpdateRate(updateRate);
    s2Calibrator->SetMinStats(minStats);
    s2Calibrator->SetTrigger(trigger);
    s2Calibrator->SetNofModules(1, 3); // dets, bars(incl. master trigger)
    run->AddTask(s2Calibrator);
	/* Calibrate Los - END */


	/* Initialize ------------------------------------------- */
	run->Init();
	FairLogger::GetLogger()->SetLogScreenLevel("INFO");
	FairLogger::GetLogger()->SetLogScreenLevel("WARNING");
	FairLogger::GetLogger()->SetLogScreenLevel("ERROR");
	/* ------------------------------------------------------ */


	/* Run -------------------------------------------------- */
	run->Run((nev < 0) ? nev : 0, (nev < 0) ? 0 : nev);
	if (rtdb->getCurrentRun()) cout << "have run" << endl;
	else cout << "have no run" << endl;
	rtdb->saveOutput();
	/* ------------------------------------------------------ */

	timer.Stop();
	Double_t rtime = timer.RealTime();
	Double_t ctime = timer.CpuTime();
	cout << endl << endl;
	cout << "Macro finished succesfully." << endl;
	cout << "Output file is " << outputFilename << endl;
	cout << "Real time " << rtime << " s, CPU time " << ctime << " s"
		     << endl << endl;
}

