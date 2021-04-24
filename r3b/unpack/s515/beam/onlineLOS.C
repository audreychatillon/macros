
/*
 * In order to generate input for this, please go to $UCESB_DIR and run:
 *
 * Additional info:
 * To generate the header file used for the R3BUcesbSource (ext_h101.h), use:
 *
 * $UCESB_DIR/upexps/s438b/s438b --ntuple=UNPACK:TRIGGER,UNPACK:EVENTNO,RAW\
 *     STRUCT_HH,ext_h101.h
 *
 * Put this header file into the 'r3bsource' directory and recompile.
 * 
 * // Tpat: 1-8 on-beam
 * //       1=los; 2=los+tofd; 3= los+tofd+neuland
 * // Tpat: 9-16 off-beam
 * //       9=tofd; 10=neuland; 11-15 fi7/8/10/11/3
 * // if 0, no tpat selection
 *
 * // Trigger: 1=physics; 10=keep-alive on-spill; 11=keep-alive off-spill; 12=begin of spill; 13=end-of-spill
 * 
 */

#include <sstream>
#include <fstream>
#include <iostream>
using namespace std;
extern "C" {
	#include "/u/kelic/R3BRoot/r3bsource/ext_h101_los_s515.h"
	#include "/u/kelic/R3BRoot/r3bsource/ext_h101_unpack.h"
	#include "/u/kelic/R3BRoot/r3bsource/ext_h101_tpat.h"
    #include "/u/kelic/R3BRoot/r3bsource/ext_h101_timestamp_master.h"

}
struct EXT_STR_h101_t
{
	EXT_STR_h101_unpack_t unpack;
	EXT_STR_h101_TPAT_t TPAT;
	EXT_STR_h101_LOS_t los;	
	EXT_STR_h101_timestamp_master_t timestamp_master;
};

void onlineLOS(Int_t RunId=1)
{
    TString runNumber=Form ("%03d", RunId);
    TStopwatch timer;
    timer.Start();
    

        // Turn off automatic backtrace 
        gSystem->ResetSignals();   
    
    const Int_t nev = -1; /* number of events to read, -1 - until CTRL+C */
    
 /* Create source using ucesb for input ------------------ */
	TString filename = " --stream=lxlanddaq01:9100 --allow-errors --input-buffer=50Mi"; 
	
	//TString filename = "/d/land6/202104_s515/lmd/main0"+runNumber+"* --input-buffer=50Mi";  

	TString output_path = "/lustre/land/kelic/s515/rootfiles/";
	TString outputFileName = output_path+"los_stream.root";

	TString ntuple_options = "RAW";
    TString ucesb_dir = getenv("UCESB_DIR");
   
    TString ucesb_path;
 	ucesb_path = "/u/land/fake_cvmfs/9.13/upexps/202104_s515/202104_s515";

   EXT_STR_h101_t ucesb_struct;
	R3BUcesbSource* source = new R3BUcesbSource(filename, ntuple_options,
		ucesb_path, &ucesb_struct, sizeof(ucesb_struct));
	source->SetMaxEvents(nev);

	source->AddReader(new R3BUnpackReader(
		&ucesb_struct.unpack,
		offsetof(EXT_STR_h101_t, unpack)));
		
	source->AddReader(new R3BTrloiiTpatReader(
        (EXT_STR_h101_TPAT_t *)&ucesb_struct.TPAT,
        offsetof(EXT_STR_h101, TPAT)));	

     source->AddReader(new R3BTimestampMasterReader(
        (EXT_STR_h101_timestamp_master_t *)&ucesb_struct.timestamp_master,
        offsetof(EXT_STR_h101, timestamp_master)));
        
    source->AddReader( new R3BLosReader (
	   &ucesb_struct.los, 
	   offsetof(EXT_STR_h101_t, los)) );

    
    const Int_t refresh = 100;                 // refresh rate for saving 
    
  /* Create online run ------------------------------------ */
#define RUN_ONLINE
#define USE_HTTP_SERVER
#ifdef RUN_ONLINE
  FairRunOnline* run = new FairRunOnline(source);
  run->SetRunId(RunId);
#ifdef USE_HTTP_SERVER
  Int_t port=7272;
  run->ActivateHttpServer(refresh,port);
#endif
#else
  /* Create analysis run ---------------------------------- */
  FairRunAna* run = new FairRunAna();
#endif

    // Create analysis run ----------------------------------
    run->SetOutputFile(outputFileName.Data());


    FairRuntimeDb* rtdb1 = run->GetRuntimeDb();
    Bool_t kParameterMerged = kTRUE;
    FairParRootFileIo* parOut1 = new FairParRootFileIo(kParameterMerged);
    TList *parList = new TList();
    
    parList->Add(new TObjString("parameter/tcal_los_pulser.root")); //"+runNumber+"

    parOut1->open(parList);
    rtdb1->setFirstInput(parOut1);
    rtdb1->print();

    rtdb1->addRun(RunId);
    rtdb1->getContainer("LosTCalPar");
    rtdb1->setInputVersion(RunId, (char*)"LosTCalPar", 1, 1);


    /* Add analysis task ------------------------------------ */
    // convert Mapped => Cal
    R3BLosMapped2Cal* losMapped2Cal=new R3BLosMapped2Cal("LosTCalPar", 1);
    losMapped2Cal->SetNofModules(1,8);
    losMapped2Cal->SetTrigger(1);
    run->AddTask( losMapped2Cal );
     
    R3BOnlineSpectraLosStandalone* r3bOnlineSpectra=new R3BOnlineSpectraLosStandalone("OnlineSpectraLosStandalone", 1); 

// Nof LOS detectors:    
     r3bOnlineSpectra->SetNofLosModules(1); // 1 or 2 LOS detectors  
  //  Set parameters for X,Y calibration
                            //  (offsetX, offsetY,VeffX,VeffY)
     r3bOnlineSpectra->SetLosXYTAMEX(0,0,1,1,0,0,1,1);
     r3bOnlineSpectra->SetLosXYMCFD(1.011,1.216,1.27,1.88,0,0,1,1);//(0.9781,1.152,1.5,1.5,0,0,1,1);                       
     r3bOnlineSpectra->SetLosXYToT(-0.002373,0.007423,2.27,3.22,0,0,1,1);//(-0.02054,-0.02495,2.5,3.6,0,0,1,1);
     
     r3bOnlineSpectra->SetEpileup(350.);  // Events with ToT>Epileup are not considered
     
/*
 Triggers:
1  = on-/off-spill
3  = clock triggers, AMS = raw data, others = to test DAQ PC/electronics sync
8  = sync triggers to check timestamp correlations
10 = keep-alive on-spill
11 = keep-alive off-spill
12 = begin of spill
13 = end-of-spill

Tpats, you can in principle find in the run-sheets, but here's a nicer version:
1 = beam & ams_alive & los
2 = beam & ams_alive & los & tofd
3 = beam & ams_alive & los & califa-or
4 = beam & ams_alive & los & califa-and
5 = beam & ams_alive & los & neuland

6 = beam & ams_early_dead & los
7 = beam & ams_early_dead & los & tofd
8 = beam & ams_early_dead & los & califa-or
9 = beam & ams_early_dead & los & califa-and
10 = beam & ams_early_dead & los & neuland (i.e. these are duplicates of 1..5 where AMS is not in)

11 = !beam & tofd
12 = !beam & neuland  
16 = sync
*/
// -1 = no trigger selection
    r3bOnlineSpectra->SetTrigger(1);
// if 0, no tpat selection
    r3bOnlineSpectra->SetTpat(0);


  
  run->AddTask( r3bOnlineSpectra );

    /* Initialize ------------------------------------------- */
    run->Init();
    rtdb1->print();
    //  FairLogger::GetLogger()->SetLogScreenLevel("INFO");    
    //  FairLogger::GetLogger()->SetLogScreenLevel("WARNING");
    //  FairLogger::GetLogger()->SetLogScreenLevel("DEBUG");
     // FairLogger::GetLogger()->SetLogScreenLevel("DEBUG1");
     FairLogger::GetLogger()->SetLogScreenLevel("ERROR");

    /* Run -------------------------------------------------- */
    run->Run((nev < 0) ? nev : 0, (nev < 0) ? 0 : nev);
//    rtdb1->saveOutput();

    timer.Stop();
    Double_t rtime = timer.RealTime();
    Double_t ctime = timer.CpuTime();
    cout << endl << endl;
    cout << "Macro finished succesfully." << endl;
    cout << "Output file is " << outputFileName << endl;
    cout << "Real time " << rtime << " s, CPU time " << ctime << " s"
         << endl << endl;
}

