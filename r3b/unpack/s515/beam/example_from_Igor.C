// NeuLAND Online Monitoring
// run: root -l -q -b neuland_online.C

struct EXT_STR_h101_t
{
    EXT_STR_h101_unpack_t unpack;
    EXT_STR_h101_TPAT tpat;
    EXT_STR_h101_SOFSCI_onion_t sci;
    EXT_STR_h101_raw_nnp_tamex_t raw_nnp;
};

void neuland_online()
{
    TStopwatch timer;
    timer.Start();
  
    const Int_t nBarsPerPlane = 50; // number of scintillator bars per plane
    const Int_t nPlanes = 24;       // number of planes (for TCAL calibration)
    const double distanceToTarget = 1520.;

    const Int_t nev = -1;     /* number of events to read, -1 - until CTRL+C */
    const Int_t trigger = -1; // 1 - onspill, 2 - offspill. -1 - all

    //const TString filename = "stream://lxir133:7793";
    const TString filename = "stream://lxlanddaq01:9100";
    //const TString filename = "/d/land5/202104_s455/lmd/main0423_001*.lmd";
    
    //const TString ucesbPath = "/u/land/fake_cvmfs/9.13/upexps/202103_s455/202103_s455_part2";
    const TString ucesbPath = "/u/land/fake_cvmfs/9.13/upexps/202104_s455/202104_s455_part2";
    const TString usesbCall = ucesbPath + " --allow-errors --input-buffer=100Mi";

    const TString outputFileName = "/d/land2/Neuland/nearline_online.root";

    // Event IO Setup
    // -------------------------------------------
    EXT_STR_h101 ucesbStruct;
    auto source = new R3BUcesbSource(filename, "RAW,time-stitch=1000", usesbCall, &ucesbStruct, sizeof(ucesbStruct));
    source->SetMaxEvents(nev);

    source->AddReader(new R3BUnpackReader(&ucesbStruct.unpack, offsetof(EXT_STR_h101, unpack)));
    source->AddReader(new R3BTrloiiTpatReader((EXT_STR_h101_TPAT_t*)&ucesbStruct.tpat, offsetof(EXT_STR_h101, tpat)));
    source->AddReader(new R3BSofSciReader((EXT_STR_h101_SOFSCI_t*)&ucesbStruct.sci, offsetof(EXT_STR_h101, sci),2));
    source->AddReader(new R3BNeulandTamexReader(&ucesbStruct.raw_nnp, offsetof(EXT_STR_h101, raw_nnp)));

    auto run = new FairRunOnline(source);
    run->SetRunId(999);
    run->ActivateHttpServer(1, 8885);
    run->SetSink(new FairRootFileSink(outputFileName));

    // Parameter IO Setup
    // -------------------------------------------
    auto rtdb = run->GetRuntimeDb();

    //auto parList = new TList();
    //parList->Add(new TObjString("params_sync_ig_s455x.root"));
    //parList->Add(new TObjString("params_tcal-kb.root"));

    auto parIO = new FairParRootFileIo(false);
    //parIO->open("params_sync_ig_070321.root");
    //parIO->open("params_sync_ig_s455xxx.root");
    //parIO->open("params_sync_ig_s455_160321.root");
    //parIO->open("params_sync_ig_s455_0999.root");
    //parIO->open("params_ig_190321.root");
    //parIO->open("params_sync_ig_s455_0264.root");
    //parIO->open("params_sync_ig_s455_0297.root");
    //parIO->open("params_corr.root");
    //parIO->open(" params_sync_ig_s455_0307.root");
    //parIO->open(" params_sync_ig_s455_0311.root");
    //parIO->open(" params_sync_ig_s455_0378.root");
    //parIO->open(" params_sync_ig_s455_0413.root");
    parIO->open(" params_ig_x.root");
    //parIO->open(" params_sync_ig_s455_0423.root");
    
    //parIO->open(parList);
    rtdb->setFirstInput(parIO);

    auto parIOsofia = new FairParAsciiFileIo();
    //parIOsofia->open("CalibParam.par", "in");
    //parIOsofia->open("CalibParam_onesci.par", "in");
    parIOsofia->open("CalibParam_twosci_new.par", "in");
    rtdb->setSecondInput(parIOsofia);

    rtdb->addRun(999);
    rtdb->getContainer("LandTCalPar");
    rtdb->setInputVersion(999, (char*)"LandTCalPar", 1, 1);
    rtdb->getContainer("NeulandHitPar");
    rtdb->setInputVersion(999, (char*)"NeulandHitPar", 1, 1);
    
    run->AddTask(new R3BSofSciMapped2Tcal());
    run->AddTask(new R3BSofiaProvideTStart());

    auto tcal = new R3BNeulandMapped2Cal();
    tcal->SetTrigger(trigger);
    tcal->SetNofModules(nPlanes, nBarsPerPlane);
    tcal->SetNhitmin(1);
    tcal->EnableWalk(true);
    run->AddTask(tcal);
    
    auto nlhit = new R3BNeulandCal2Hit();
    //nlhit->SetDistanceToTarget(distanceToTarget);
    //nlhit->SetGlobalTimeOffset(705);
    //nlhit->SetGlobalTimeOffset(3450);
    //nlhit->SetGlobalTimeOffset(8250);
    //nlhit->SetGlobalTimeOffset(3390); // 6050 all 238U run
    //nlhit->SetGlobalTimeOffset(9450);
    //nlhit->SetGlobalTimeOffset(450);
    run->AddTask(nlhit);
    
    auto r3bNeulandOnlineSpectra = new R3BNeulandOnlineSpectra();
    r3bNeulandOnlineSpectra->SetDistanceToTarget(distanceToTarget);
    run->AddTask(r3bNeulandOnlineSpectra);

    // Go!
    // -------------------------------------------
    run->Init();
    FairLogger::GetLogger()->SetLogScreenLevel("ERROR");
    run->Run((nev < 0) ? nev : 0, (nev < 0) ? 0 : nev);

    timer.Stop();
    Double_t rtime = timer.RealTime();
    Double_t ctime = timer.CpuTime();
    cout << endl << endl;
    cout << "Macro finished succesfully." << endl;
    cout << "Output file is " << outputFileName << endl;
    cout << "Real time " << rtime << " s, CPU time " << ctime << " s" << endl << endl;
}
