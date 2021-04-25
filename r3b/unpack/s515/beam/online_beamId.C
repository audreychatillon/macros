#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;

struct EXT_STR_h101_t
{
    EXT_STR_h101_unpack_t unpack;
    EXT_STR_h101_TPAT_t TPAT;
    EXT_STR_h101_LOS_t los;
    EXT_STR_h101_timestamp_master_t timestamp_master;
    EXT_STR_h101_SCI2_t s2;
    EXT_STR_h101_MUSIC_onion_t music;
};

void online_beamId()
{
    TStopwatch timer;
    timer.Start();

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");

    // --- ------------------------------------------------------ ---
    // --- check and modify if necessary ------------------------ ---
    // --- for unpacking ---------------------------------------- ---
    // --- ------------------------------------------------------ ---
    // --- for local computer
    // TString filename = "~/data/s515/calFrs/main0457_0001.lmd --allow-errors --input-buffer=50Mi";
    TString filename = "~/data/s515/calFrs/main0458_0001_stitched.lmd --allow-errors --input-buffer=50Mi";
    TString ucesb_dir = getenv("UCESB_DIR");
    TString ucesb_path = ucesb_dir + "/../upexps/202104_s515/202104_s515";
    ucesb_path.ReplaceAll("//", "/");

    // --- land account
    // TString filename = "/d/land5/202104_s515/lmd/main*.lmd --allow-errors --input-buffer=50Mi";
    // TString filename = " --stream=lxlanddaq01:9001 --allow-errors --input-buffer=50Mi"; //PSPx and Incoming ID online
    // analysis TString ucesb_path = "/u/land/fake_cvmfs/9.13/upexps/202104_s515/202104_s515";

    TString ntuple_options = "RAW"; // ntuple options for stitched data
    // TString ntuple_options = "RAW,time_stitch=1000"; // ntuple_options for raw data

    // --- ------------------------------------------------------ ---
    // --- check and modify if necessary ------------------------ ---
    // --- for online run --------------------------------------- ---
    // --- ------------------------------------------------------ ---
    TString output_path = "~/data/s515/";
    // TString output_path = "/d/land5/202104_s515/rootfiles/beam/";
    TString outputFilename = output_path + "s515_TofPt_stitched_458_" + oss.str() + ".root";
    const Int_t refresh = 1;
    Int_t port = 5555;

    // Turn off automatic backtrace
    gSystem->ResetSignals();

    // --- ------------------------------------------------------ ---
    // --- number of events to read, ifnev = -1 read until CTRL+C ---
    // --- ------------------------------------------------------ ---
    const Int_t nev = -1;
    const Int_t RunId = 1;

    // --- -------------------------------------------------------- ---
    // --- Create source using ucesb for input -------------------- ---
    // --- -------------------------------------------------------- ---
    EXT_STR_h101_t ucesb_struct;
    R3BUcesbSource* source =
        new R3BUcesbSource(filename, ntuple_options, ucesb_path, &ucesb_struct, sizeof(ucesb_struct));
    source->SetMaxEvents(nev);

    // --- -------------------------------------------------------- ---
    // --- Add Readers -------------------------------------------- ---
    // --- -------------------------------------------------------- ---
    source->AddReader(new R3BUnpackReader(&ucesb_struct.unpack, offsetof(EXT_STR_h101_t, unpack)));
    source->AddReader(new R3BTrloiiTpatReader((EXT_STR_h101_TPAT_t*)&ucesb_struct.TPAT, offsetof(EXT_STR_h101, TPAT)));
    source->AddReader(new R3BTimestampMasterReader((EXT_STR_h101_timestamp_master_t*)&ucesb_struct.timestamp_master,
                                                   offsetof(EXT_STR_h101, timestamp_master)));
    source->AddReader(new R3BLosReader(&ucesb_struct.los, offsetof(EXT_STR_h101_t, los)));
    source->AddReader(new R3BSci2Reader(&ucesb_struct.s2, offsetof(EXT_STR_h101_t, s2)));
    source->AddReader(new R3BMusicReader((EXT_STR_h101_MUSIC_t*)&ucesb_struct.music, offsetof(EXT_STR_h101, music)));

    // --- -------------------------------------------------------- ---
    // --- Create online run -------------------------------------- ---
    // --- -------------------------------------------------------- ---
    FairRunOnline* run = new FairRunOnline(source);
    run->SetRunId(RunId);
    run->SetSink(new FairRootFileSink(outputFilename));
    // run->SetOutputFile(outputFileName.Data());
    run->ActivateHttpServer(refresh, port);

    // --- -------------------------------------------------------- ---
    // --- Parameter IO setup ------------------------------------- ---
    // --- -------------------------------------------------------- ---
    auto rtdb = run->GetRuntimeDb();

    // TCAL PARAMETERS
    Bool_t kParameterMerged = kTRUE;
    FairParRootFileIo* parTcal = new FairParRootFileIo(kParameterMerged);
    TList* parList = new TList();
    parList->Add(new TObjString("parameter/tcal_los_pulser.root"));
    parList->Add(new TObjString("parameter/tcal_s2.root"));
    parTcal->open(parList);
    rtdb->setFirstInput(parTcal);

    // R3B-MUSIC CALIBRATION PARAMETER if ascii
    FairParAsciiFileIo* parMus = new FairParAsciiFileIo(); // Ascii
    // auto parIOs2 = new FairParAsciiFileIo();
    parMus->open("parameter/CalibParam_onesci.par", "in");
    rtdb->setSecondInput(parMus);

    rtdb->print();
    rtdb->addRun(RunId);
    rtdb->getContainer("LosTCalPar");
    rtdb->setInputVersion(RunId, (char*)"LosTCalPar", 1, 1);
    rtdb->getContainer("Sci2TCalPar");
    rtdb->setInputVersion(RunId, (char*)"Sci2TCalPar", 1, 1);

    // --- -------------------------------------------------------- ---
    // --- Add analysis tasks ------------------------------------- ---
    // --- -------------------------------------------------------- ---

    // LOS Mapped => Cal
    R3BLosMapped2Cal* losMapped2Cal = new R3BLosMapped2Cal("LosTCalPar", 1);
    losMapped2Cal->SetNofModules(1, 8);
    losMapped2Cal->SetTrigger(1);
    run->AddTask(losMapped2Cal);

    // S2 Mapped2Tcal
    R3BSci2Mapped2Tcal* s2Mapped2Tcal = new R3BSci2Mapped2Tcal("Sci2Map2Tcal", 1);
    run->AddTask(s2Mapped2Tcal);

    // R3B-MUSIC Mapped2Cal
    R3BMusicMapped2Cal* MusMap2Cal = new R3BMusicMapped2Cal();
    run->AddTask(MusMap2Cal);

    // R3B-MUSIC Cal2Hit
    R3BMusicCal2Hit* MusCal2Hit = new R3BMusicCal2Hit();
    run->AddTask(MusCal2Hit);

    // --- -------------------------------------------------------- ---
    // --- Add online tasks --------------------------------------- ---
    // --- -------------------------------------------------------- ---

    // --- LOS STANDALONE
    // R3BOnlineSpectraLosStandalone* r3bOnlineSpectra=new R3BOnlineSpectraLosStandalone("OnlineSpectraLosStandalone",
    // 1); r3bOnlineSpectra->SetNofLosModules(1); // 1 or 2 LOS detectors
    //  Set parameters for X,Y calibration
    //  (offsetX, offsetY,VeffX,VeffY)
    // r3bOnlineSpectra->SetLosXYTAMEX(0,0,1,1,0,0,1,1);
    // r3bOnlineSpectra->SetLosXYMCFD(1.011,1.216,1.27,1.88,0,0,1,1);//(0.9781,1.152,1.5,1.5,0,0,1,1);
    // r3bOnlineSpectra->SetLosXYToT(-0.002373,0.007423,2.27,3.22,0,0,1,1);//(-0.02054,-0.02495,2.5,3.6,0,0,1,1);
    // r3bOnlineSpectra->SetEpileup(350.);  // Events with ToT>Epileup are not considered
    // r3bOnlineSpectra->SetTrigger(1);     // -1 = no trigger selection
    // r3bOnlineSpectra->SetTpat(0);        // if 0, no tpat selection
    // run->AddTask( r3bOnlineSpectra );

    // --- SCI2 STANDALONE
    R3BOnlineSpectraSci2* s2online = new R3BOnlineSpectraSci2();
    s2online->SetNbDetectors(1);
    s2online->SetNbChannels(3);
    run->AddTask(s2online);

    // --- LOS VS SCI2
    R3BOnlineSpectraLosVsSci2* loss2online = new R3BOnlineSpectraLosVsSci2("OnlineSpectraLosVsSci2", 1);
    loss2online->SetNofLosModules(1); // 1 or 2 LOS detectors
    //  Set parameters for X,Y calibration
    //  (offsetX, offsetY,VeffX,VeffY)
    loss2online->SetLosXYTAMEX(0, 0, 1, 1, 0, 0, 1, 1);
    loss2online->SetLosXYMCFD(1.011, 1.216, 1.27, 1.88, 0, 0, 1, 1);       //(0.9781,1.152,1.5,1.5,0,0,1,1);
    loss2online->SetLosXYToT(-0.002373, 0.007423, 2.27, 3.22, 0, 0, 1, 1); //(-0.02054,-0.02495,2.5,3.6,0,0,1,1);
    loss2online->SetEpileup(350.);                                         // Events with ToT>Epileup are not considered
    loss2online->SetTrigger(1);                                            // -1 = no trigger selection
    loss2online->SetTpat(0);                                               // if 0, no tpat selection
    // AoQ calibration :
    loss2online->SetToFmin(-8803);
    loss2online->SetToFmax(-8801);
    loss2online->SetTof2InvV_p0(67.69245);
    loss2online->SetTof2InvV_p1(0.007198663);
    loss2online->SetFlightLength(139.915);
    loss2online->SetPos_p0(126.451);
    loss2online->SetPos_p1(56.785);
    loss2online->SetDispersionS2(7000);
    loss2online->SetBrho0_S2toCC(11.9891); // main 458
    // loss2online->SetBrho0_S2toCC(9.458); // main 461
    run->AddTask(loss2online);

    // --- -------------------------------------------------------- ---
    // --- Initialize --------------------------------------------- ---
    // --- -------------------------------------------------------- ---
    run->Init();
    rtdb->print();
    FairLogger::GetLogger()->SetLogScreenLevel("INFO");
    // FairLogger::GetLogger()->SetLogScreenLevel("WARNING");
    // FairLogger::GetLogger()->SetLogScreenLevel("DEBUG");
    // FairLogger::GetLogger()->SetLogScreenLevel("DEBUG1");
    // FairLogger::GetLogger()->SetLogScreenLevel("ERROR");

    // --- -------------------------------------------------------- ---
    // --- Run ---------------------------------------------------- ---
    // --- -------------------------------------------------------- ---
    run->Run((nev < 0) ? nev : 0, (nev < 0) ? 0 : nev);
    // rtdb->saveOutput();

    // --- -------------------------------------------------------- ---
    // --- Finish ------------------------------------------------- ---
    // --- -------------------------------------------------------- ---
    timer.Stop();
    Double_t rtime = timer.RealTime();
    Double_t ctime = timer.CpuTime();
    cout << endl << endl;
    cout << "Macro finished succesfully." << endl;
    cout << "Output file is " << outputFilename << endl;
    cout << "Real time " << rtime << " s, CPU time " << ctime << " s" << endl << endl;
}
