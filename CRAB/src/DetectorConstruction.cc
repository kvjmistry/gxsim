#include "DetectorConstruction.hh"
#include "G4PVParameterised.hh"
#include "G4PVReplica.hh"
#include "G4RotationMatrix.hh"
#include "G4UnitsTable.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4OpticalSurface.hh"
#include "G4Trd.hh"
#include "G4Threading.hh"
#include "G4RegionStore.hh"
#include "G4UniformMagField.hh"
#include "G4FieldManager.hh"
#include "G4Cons.hh"
#include "G4IntersectionSolid.hh"
#include "G4Trd.hh"
#include "DetectorMessenger.hh"
#include "GasBoxSD.hh"
#include "DegradModel.hh"
#include "GarfieldVUVPhotonModel.hh"
#include "G4SDManager.hh"
#include "Visibilities.hh"



DetectorConstruction::DetectorConstruction(GasModelParameters* gmp)
    :
    fGasModelParameters(gmp),
    checkOverlaps(1),
    gasPressure(1.*bar), // Pressure inside the gas
    temperature(273.15*kelvin), // temperature
    Lab_size(3. *m),
    chamber_diam   (15. * cm),
    chamber_length (43.18 * cm),
    chamber_thickn (2. * mm),
    SourceEn_offset (5.2 *cm),
    SourceEn_diam   (1. * cm),
    SourceEn_length (1 * cm),
    SourceEn_thickn (2. * mm),
    SourceEn_holedia (2. * mm),
    gas_pressure_(9.7 * bar),
    vtx_(0,0,0),
    Active_diam(8.5 * cm),
    sc_yield_(25510./MeV),
    e_lifetime_(1000. * ms),
    pmt_hole_length_ (18.434 * cm),
    MgF2_window_thickness_ (6. * mm),
    MgF2_window_diam_ (10 * mm),
    wndw_ring_stand_out_ (1.5 * mm), //how much the ring around sapph windows stands out of them
    pedot_coating_thickness_ (200. * nanometer), // copied from NEW
    optical_pad_thickness_ (1. * mm), // copied from NEW
    pmt_base_diam_ (47. * mm),
    pmt_base_thickness_ (5. * mm),
    HideSourceHolder_(true),
    max_step_size_(1.*mm),
    ElGap_(7*mm),
    ELyield_(970/cm),
    PMT1_Pos_(2.32*cm),
    PMT3_Pos_(3.52*cm),
    HideCollimator_(true)
{
  detectorMessenger = new DetectorMessenger(this);
}

DetectorConstruction::~DetectorConstruction() {
  delete detectorMessenger;
}

G4VPhysicalVolume* DetectorConstruction::Construct(){
  /* The World volume is a vacuum in which a gastube is placed with the walls made out of Aluminum. The
  endcaps are Silicon detectors, used as calorimeter 
  */
    
  //Colors for visualization
  G4VisAttributes* red = new G4VisAttributes(G4Colour(1., 0., 0.));
  G4VisAttributes* green = new G4VisAttributes(G4Colour(0., 1., 0.));
  G4VisAttributes* blue = new G4VisAttributes(G4Colour(0., 0., 1.));
  G4VisAttributes* yellow = new G4VisAttributes(G4Colour(1.0, 1.0, 0.));
  G4VisAttributes* purple = new G4VisAttributes(G4Colour(1.0, 0., 1.0));

  //Materials
  G4Material *gxe    = materials::GXe(gas_pressure_,68);
  G4Material *MgF2   = materials::MgF2();
  G4Material *Steel  = materials::Steel();
  G4Material *vacuum = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");

  // Optical Properties Assigned here
  MgF2->SetMaterialPropertiesTable(opticalprops::MgF2());
  vacuum->SetMaterialPropertiesTable(opticalprops::Vacuum());
  gxe->SetMaterialPropertiesTable(opticalprops::GXe(gas_pressure_, 68,sc_yield_,e_lifetime_));
  //Steel->SetMaterialPropertiesTable(opticalprops::STEEL());
  
  // Constructing Lab Space
  G4String lab_name="LAB";
  G4Box * lab_solid_volume = new G4Box(lab_name,Lab_size/2,Lab_size/2,Lab_size/2);
  G4LogicalVolume * lab_logic_volume = new G4LogicalVolume(lab_solid_volume,G4NistManager::Instance()->FindOrBuildMaterial("G4_AIR"),lab_name) ;


  // lab_logic_volume->SetVisAttributes(G4VisAttributes::Invisible);

  // Krish -- Edited to here

  // Creating the Steel Cylinder that we need

  /// First Creating the Ends of the Cylinder with Proper Holes
  G4Tubs* chamber_flange_solid = new G4Tubs("CHAMBER_FLANGE", MgF2_window_diam_/2, (chamber_diam/2. + chamber_thickn),( chamber_thickn), 0.,twopi);
  G4LogicalVolume* chamber_flange_logic =new G4LogicalVolume(chamber_flange_solid,materials::Steel(), "CHAMBER_FLANGE");

  // Now Creating The Chamber with Without Ends
  G4Tubs* chamber_solid =new G4Tubs("CHAMBER", chamber_diam/2., (chamber_diam/2. + chamber_thickn),(chamber_length/2. + chamber_thickn), 0.,twopi);
  G4LogicalVolume* chamber_logic =new G4LogicalVolume(chamber_solid,materials::Steel(), "CHAMBER"); //



  /// MgF2 window ///
  G4Tubs* MgF2_window_solid = new G4Tubs("MgF2_WINDOW", 0., MgF2_window_diam_/2.,
                                              (MgF2_window_thickness_ )/2., 0., twopi);
  G4LogicalVolume* MgF2_window_logic= new G4LogicalVolume(MgF2_window_solid, MgF2, "MgF2_WINDOW");

  // lens
  const G4double lensRcurve (5.698*cm); // radius of curvature of MgF2 Lens
  G4double nind(1.44); // Refractive index of MgF2 at 178 nm at given R of lens
  
  const G4ThreeVector posLensTubeIntersect (0.,0.,11.25*cm); // Choose intersect distance to make a 1cm diam
  
  // Create lens from the intersection of two spheres
  G4Orb* sLensTube = new G4Orb("sLensSphereTube",lensRcurve);
  G4Orb* sLensOrb = new G4Orb("sLensSphere",lensRcurve);
  G4IntersectionSolid* sLens =  new G4IntersectionSolid("sLens",sLensTube,sLensOrb, 0, posLensTubeIntersect);
  
  // Lens logical 
  G4LogicalVolume* lensLogical = new G4LogicalVolume(sLens, MgF2, "Lens");

  //////////////////////////////////////////

  G4double FielCageGap=(160.3+29.55)*mm;
  
  // Placing the gas in the chamber
  G4Tubs* gas_solid =new G4Tubs("GAS", 0., chamber_diam/2., chamber_length/2., 0., twopi);
  gas_logic = new G4LogicalVolume(gas_solid, gxe, "GAS");
  
  //gas_logic->SetUserLimits(new G4UserLimits(50*mm));
  //Defining the Detection Area
  //G4Tubs* Active_solid =new G4Tubs("ACTIVE", 0., Active_diam/2., Active_length/2., 0., twopi);
  //G4LogicalVolume* Active_logic = new G4LogicalVolume(Active_solid, gxe, "ACTIVE");


  // EL Region
  G4Tubs* EL_solid = new G4Tubs("EL_GAP", 0., Active_diam/2.,ElGap_/2 , 0., twopi);
  G4LogicalVolume* EL_logic = new G4LogicalVolume(EL_solid, gxe, "EL_GAP");
  //EL_logic->SetUserLimits(new G4UserLimits(50*mm));

  // FieldCage
  G4Tubs* FieldCage_Solid =new G4Tubs("FIELDCAGE", 0., Active_diam/2.,FielCageGap/2 , 0., twopi);
  G4LogicalVolume* FieldCage_Logic = new G4LogicalVolume(FieldCage_Solid, gxe, "FIELDCAGE");
  //FieldCage_Logic->SetUserLimits(new G4UserLimits(50*mm));


  // Radioactive Source Encloser
  // Source
  G4Tubs* SourceHolChamber_solid =new G4Tubs("SourceHolChamber", SourceEn_holedia/2, (SourceEn_diam/2. + SourceEn_thickn),(SourceEn_length/2. + SourceEn_thickn),0,twopi);
  G4LogicalVolume* SourceHolChamber_logic = new G4LogicalVolume(SourceHolChamber_solid,materials::Steel(), "SourceHolChamber_logic");

  G4Tubs* SourceHolChamberBlock_solid =new G4Tubs("SourceHolChBlock",0,(SourceEn_holedia/2),( SourceEn_thickn/2), 0.,twopi);
  G4LogicalVolume* SourceHolChamberBlock_logic = new G4LogicalVolume(SourceHolChamberBlock_solid,materials::Steel(), "SourceHolChBlock_logic");


  /// Needle Source
  G4double NeedleyepRMin=0;
  G4double NeedleyepRMax=(0.42)*mm;
  G4double NeedleyepDz=( 2/2)*mm;
  G4double NeedleHalfLength=(2.56/2)*cm;
  G4double NeedleTailDiam=(0.6/2)*mm;
  G4double NeedleOffset=1*mm;

  G4Tubs* NeedleEye = new G4Tubs("NeedleEye",NeedleyepRMin,NeedleyepRMax,NeedleyepDz, 0.,twopi);
  G4Tubs* NeedleTail = new G4Tubs("NeedleTail",NeedleyepRMin,NeedleTailDiam,NeedleHalfLength, 0.,twopi);

  G4Tubs *Collimator = new G4Tubs("Collimator",(5.74/2)*mm,(11.5/2)*mm,0.5*cm, 0.,twopi);
  G4Tubs * CollimatorBlock = new G4Tubs("CollimatorBlock",NeedleTailDiam,(5.74/2)*mm,0.2*cm, 0.,twopi);
  
  // Combining them to create the Needle
  G4VSolid * Needle = new G4UnionSolid("Needle",NeedleEye,NeedleTail,0,G4ThreeVector(0,0,NeedleHalfLength));
  G4VSolid * CollimatorWithBlock = new G4UnionSolid("CollimatorWithBlock",Collimator,CollimatorBlock,0,G4ThreeVector(0,0,0.5*cm-0.2*cm));
  //G4VSolid * NeedleWithCollimator=new G4UnionSolid("NeedleWithCollimator",Needle,Collimator,0,G4ThreeVector(0,0,+5*mm));

  //G4LogicalVolume * Needle_Logic=new G4LogicalVolume(Needle,materials::Steel(),"Needle");
  G4LogicalVolume * Needle_Logic = new G4LogicalVolume(Needle,materials::Steel(),"Needle");
  G4LogicalVolume * Coll_Logic = new G4LogicalVolume(CollimatorWithBlock,materials::PEEK(),"CollimatorWithBlock");


  ///
  //Adding the PMTs in here
  pmt1_=new pmt::PmtR7378A();
  pmt2_=new pmt::PmtR7378A();
  pmt1_->SetPMTName("S2");
  pmt2_->SetPMTName("S1");
  pmt1_->Construct();
  pmt2_->Construct();


  std::cout << "Finished Initializing PMTs" << std::endl;

  // Adding Logical Volumes for PMTs
  G4LogicalVolume * pmt1_logic=pmt1_->GetLogicalVolume();
  G4LogicalVolume * pmt2_logic=pmt2_->GetLogicalVolume();


  // PMT1 and PMT3

  // // PMTs
  G4double PMT_offset=0.2*cm;
  G4double PMT_pos=(chamber_length/2)+chamber_thickn+(pmt1_->Length()/2)+MgF2_window_thickness_+PMT_offset;
  G4RotationMatrix* pmt1rotate = new G4RotationMatrix();
  pmt1rotate->rotateY(180.*deg);

  //// PMT Covering Tube ///
  G4double offset=1.65*cm;
  G4double PMT_Tube_Length1=MgF2_window_thickness_+(pmt1_->Length()+0.5*cm)/2 + offset -PMT_offset-0.05*cm ;
  G4double PMT_Tube_Length0=(17*cm+pmt1_->Length())/2 - 3.87*cm-PMT_offset;
  G4double PMT_Tube_Block_Thickness=0.2*cm;
  G4double LongPMTTubeOffset=7.5*cm-3.9*cm;
  G4double PMTTubeDiam=2.54*cm;

  // Tube Away from EL
  G4Tubs * PMT_Tube_solid0=new G4Tubs("PMT_TUBE0",(PMTTubeDiam/2)+0.5*cm,(PMTTubeDiam/2)+0.7*cm,PMT_Tube_Length0,0,twopi);
  G4LogicalVolume * PMT_Tube_Logic0=new G4LogicalVolume(PMT_Tube_solid0,materials::Steel(),PMT_Tube_solid0->GetName());

  G4Tubs * PMT_Block_solid0=new G4Tubs("PMT_TUBE_BLOCK0",0,(PMTTubeDiam/2+0.5*cm),PMT_Tube_Block_Thickness,0,twopi);
  G4LogicalVolume * PMT_Block_Logic0=new G4LogicalVolume(PMT_Block_solid0,materials::Steel(),PMT_Block_solid0->GetName());
  
  // Vacuum for PMT TUBE0
  /// add 2.54*cm to config file
  G4Tubs * InsideThePMT_Tube_solid0=new G4Tubs("PMT_TUBE_VACUUM0",0,(PMTTubeDiam/2+0.5*cm),PMT_Tube_Length0,0,twopi);
  G4LogicalVolume * InsideThePMT_Tube_Logic0=new G4LogicalVolume(InsideThePMT_Tube_solid0,vacuum,InsideThePMT_Tube_solid0->GetName());

  // Tube Close to EL
  G4Tubs * PMT_Tube_solid1=new G4Tubs("PMT_TUBE1",(PMTTubeDiam/2)+0.5*cm,(PMTTubeDiam/2)+0.7*cm,PMT_Tube_Length1,0,twopi);
  G4LogicalVolume * PMT_Tube_Logic1=new G4LogicalVolume(PMT_Tube_solid1,materials::Steel(),PMT_Tube_solid1->GetName());
  G4Tubs * PMT_Block_solid1=new G4Tubs("PMT_TUBE_BLOCK1",0,(PMTTubeDiam/2+0.5*cm),PMT_Tube_Block_Thickness,0,twopi);
  G4LogicalVolume * PMT_Block_Logic=new G4LogicalVolume(PMT_Block_solid1,materials::Steel(),PMT_Block_solid1->GetName());

  // Vacuum for PMT TUBE1

  G4Tubs * InsideThePMT_Tube_solid1=new G4Tubs("PMT_TUBE_VACUUM1",0,(PMTTubeDiam/2+0.5*cm),PMT_Tube_Length1,0,twopi);
  G4LogicalVolume * InsideThePMT_Tube_Logic1=new G4LogicalVolume(InsideThePMT_Tube_solid1,vacuum,InsideThePMT_Tube_solid1->GetName());

  // Place the Volumes

  // LAB
  auto labPhysical = new G4PVPlacement(0, G4ThreeVector(),lab_logic_volume,lab_logic_volume->GetName(),0, false,0, false);

  //Flanges on the Chamber
  G4VPhysicalVolume *Left_Flange_phys = new G4PVPlacement(0,G4ThreeVector(0,0,(chamber_length/2)),chamber_flange_logic,chamber_flange_solid->GetName(),lab_logic_volume,true,0,false);
  G4VPhysicalVolume *Right_Flange_phys = new G4PVPlacement(0,G4ThreeVector(0,0,-(chamber_length/2)),chamber_flange_logic,chamber_flange_solid->GetName(),lab_logic_volume,true,1,false);

  //Chamber
  G4VPhysicalVolume * chamber_phys=  new G4PVPlacement(0,G4ThreeVector(0.,0.,0) ,chamber_logic, chamber_solid->GetName(), lab_logic_volume, false, 0,false);

  // Xenon Gas in Active Area and Non-Active Area
  G4VPhysicalVolume * gas_phys= new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), gas_logic, gas_solid->GetName(),lab_logic_volume, false, 0, false);
  //new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), Active_logic, Active_solid->GetName(),gas_logic, false, 0, false);


  // FieldCage
  G4double FieldCagePos=chamber_length/2-((129)*mm)-FielCageGap/2-ElGap_/2;
  G4double EL_pos=chamber_length/2-FielCageGap/2-((326)*mm)-ElGap_/2;
  G4VPhysicalVolume *FieldCage_Phys;
  FieldCage_Phys=new G4PVPlacement(0,G4ThreeVector(0,0,FieldCagePos/2),FieldCage_Logic,FieldCage_Logic->GetName(),gas_logic, 0,0,false);

  // EL_Gap
  new G4PVPlacement(0,G4ThreeVector(0.,0.,EL_pos/2),EL_logic,EL_solid->GetName(),gas_logic, 0,0, false);


  // MgF2 Windows
  G4double window_posz = chamber_length/2;
  // auto PMT3Mgf2=new G4PVPlacement(0, G4ThreeVector(0., 0., window_posz), MgF2_window_logic,"MgF2_WINDOW1", lab_logic_volume,false, 0, false);
  
  new G4PVPlacement(0, G4ThreeVector(0., 0., window_posz-lensRcurve + chamber_thickn/2.0), lensLogical,"MgF2_WINDOW1", lab_logic_volume,false, 0, false);
  new G4PVPlacement(pmt1rotate, G4ThreeVector(0., 0., -window_posz), MgF2_window_logic,"MgF2_WINDOW2", lab_logic_volume, false, 1, false);


  //PMT Tubes
  G4VPhysicalVolume *PMT_Tube_Phys0=new G4PVPlacement(0,G4ThreeVector(0,0,PMT_pos+LongPMTTubeOffset),PMT_Tube_Logic0,PMT_Tube_Logic0->GetName(),lab_logic_volume,false,0,false);
  G4VPhysicalVolume *PMT_Tube_Phys1=new G4PVPlacement(pmt1rotate,G4ThreeVector(0,0,-(PMT_pos-PMT_offset)-offset),PMT_Tube_Logic1,PMT_Tube_Logic1->GetName(),lab_logic_volume,false,0,false);
  //PMT Tube Vacuum

  G4VPhysicalVolume *PMT_Tube_Vacuum_Phys0=new G4PVPlacement(0,G4ThreeVector(0,0,PMT_pos+LongPMTTubeOffset),InsideThePMT_Tube_Logic0,"PMT_TUBE_VACUUM0",lab_logic_volume,false,0,false);
  G4VPhysicalVolume *PMT_Tube_Vacuum_Phys1=new G4PVPlacement(pmt1rotate,G4ThreeVector(0,0,-(PMT_pos-PMT_offset)-offset),InsideThePMT_Tube_Logic1,"PMT_TUBE_VACUUM1",lab_logic_volume,false,0,false);

  // PMT Tube Block
  new G4PVPlacement(0,G4ThreeVector(0,0,PMT_pos-PMT_offset+PMT_Tube_Length0-PMT_Tube_Block_Thickness/2+LongPMTTubeOffset),PMT_Block_Logic0,PMT_Block_Logic0->GetName(),lab_logic_volume,false,0,false);
  new G4PVPlacement(pmt1rotate,G4ThreeVector(0,0,-(PMT_pos-PMT_offset+PMT_Tube_Length1-PMT_Tube_Block_Thickness/2)-offset),PMT_Block_Logic,PMT_Block_Logic->GetName(),lab_logic_volume,false,1,false);

  // PMTs
  new G4PVPlacement(pmt1rotate,G4ThreeVector (0,0,((PMT3_Pos_)-pmt1_->Length()/2-PMT_Tube_Length1/2-MgF2_window_thickness_/2)),pmt1_logic,pmt1_->GetPMTName(),InsideThePMT_Tube_Logic0,true,0,false);
  new G4PVPlacement(pmt1rotate,G4ThreeVector (0,0,(PMT1_Pos_-pmt1_->Length()/2-MgF2_window_thickness_/2)),pmt2_logic,pmt2_->GetPMTName(),InsideThePMT_Tube_Logic1,true,0,false);



  /// CRAB0 Lens
  //lens = new ::UltraFresnelLens(MgF2_window_diam_,13,MgF2,labPhysical,G4ThreeVector(0., 0., window_posz));



  // Define this volume as an ionization sensitive detector
  //FieldCage_Logic->SetUserLimits(new G4UserLimits(1*mm));
  // IonizationSD* sensdet = new IonizationSD("/CRAB/FIELDCAGE");
  // //Active_logic->SetSensitiveDetector(sensdet);
  // FieldCage_Logic->SetSensitiveDetector(sensdet);
  // G4SDManager::GetSDMpointer()->AddNewDetector(sensdet);

  // Source Holder

  G4VPhysicalVolume * Needle_Phys;
  if(!HideSourceHolder_){
      // Particle Source Holder
      //Rotation Matrix

      // Needle Solid

      G4RotationMatrix* NeedleRotate = new G4RotationMatrix();
      NeedleRotate->rotateY(90.*deg);
      //NeedleRotate->rotateX(+10*deg);
      G4ThreeVector NeedlePos={vtx_[0]-NeedleOffset,vtx_[1],vtx_[2]-FieldCagePos/2};
      G4ThreeVector CollPosition={NeedlePos[0]-5*mm,NeedlePos[1],NeedlePos[2]};

      Needle_Phys= new G4PVPlacement(NeedleRotate,NeedlePos,Needle_Logic,Needle->GetName(),FieldCage_Logic,true,0,false);
      if(!HideCollimator_) {
          new G4PVPlacement(NeedleRotate,CollPosition,Coll_Logic,CollimatorWithBlock->GetName(),FieldCage_Logic,true,0,false);
      }
      G4RotationMatrix* rotateHolder = new G4RotationMatrix();
      rotateHolder->rotateY(90.*deg);

      //new G4PVPlacement(rotateHolder, G4ThreeVector(-SourceEn_offset,0,0), SourceHolChamber_logic, SourceHolChamber_solid->GetName(),gas_logic, false, 0, false);
      //new G4PVPlacement(rotateHolder, G4ThreeVector(-SourceEn_offset-SourceEn_length/2,0,0), SourceHolChamberBlock_logic, SourceHolChamberBlock_solid->GetName(),gas_logic, false, 0, false);

      // NeedleEyePointSample=new CylinderPointSampler2020(NeedleyepRMin,NeedleyepRMax+2*nm,NeedleyepDz,0,twopi, rotateHolder,G4ThreeVector(NeedlePos[0],NeedlePos[1],NeedlePos[2]+FieldCagePos/2));

  }


  /// OpticalSurface
  G4OpticalSurface * OpSteelSurf=new G4OpticalSurface("SteelSurface",unified,polished,dielectric_metal);
  OpSteelSurf->SetMaterialPropertiesTable(opticalprops::STEEL());
  new G4LogicalBorderSurface("SteelSurface_Chamber",gas_phys,chamber_phys,OpSteelSurf);
  new G4LogicalBorderSurface("SteelSurface_LeftFlange",gas_phys,Left_Flange_phys,OpSteelSurf);
  new G4LogicalBorderSurface("SteelSurface_RightFlange",gas_phys,Right_Flange_phys,OpSteelSurf);
  new G4LogicalBorderSurface("SteelSurface_PMT3_Enclosing",PMT_Tube_Vacuum_Phys0,PMT_Tube_Phys0,OpSteelSurf);
  new G4LogicalBorderSurface("SteelSurface_PMT1_Enclosing",PMT_Tube_Vacuum_Phys1,PMT_Tube_Phys1,OpSteelSurf);
  if(!HideSourceHolder_ && !HideCollimator_){
      new G4LogicalBorderSurface("SteelSurface_Needle",FieldCage_Phys,Needle_Phys,OpSteelSurf);
  }


  // Visuals

  AssignVisuals();
  
  // G4Optic
  // this->SetLogicalVolume(lab_logic_volume);
  // this->SetLogicalVolume(chamber_logic);


  //Construct a G4Region, connected to the logical volume in which you want to use the G4FastSimulationModel
  G4Region* regionGas = new G4Region("GasRegion");
  regionGas->AddRootLogicalVolume(gas_logic);
    
  return labPhysical;

}

void DetectorConstruction::ConstructSDandField(){
  G4SDManager* SDManager = G4SDManager::GetSDMpointer();
  G4String GasBoxSDname = "interface/GasBoxSD";
  GasBoxSD* myGasBoxSD = new GasBoxSD(GasBoxSDname);
  SDManager->SetVerboseLevel(1);
  SDManager->AddNewDetector(myGasBoxSD);
  SetSensitiveDetector(gas_logic,myGasBoxSD);
  
  //These commands generate the four gas models and connect it to the GasRegion
  G4Region* region = G4RegionStore::GetInstance()->GetRegion("GasRegion");
  new DegradModel(fGasModelParameters,"DegradModel",region,this,myGasBoxSD);
  new GarfieldVUVPhotonModel(fGasModelParameters,"GarfieldVUVPhotonModel",region,this,myGasBoxSD);

}

void DetectorConstruction::AssignVisuals() {
      // Chamber
      G4LogicalVolumeStore* lvStore = G4LogicalVolumeStore::GetInstance();


      // Lab
      G4LogicalVolume* Lab = lvStore->GetVolume("LAB");
      G4VisAttributes *LabVa=new G4VisAttributes(G4Colour(2,2,2));
      LabVa->SetForceWireframe(false);
      //Chamber
      G4LogicalVolume* Chamber = lvStore->GetVolume("CHAMBER");
      G4VisAttributes *ChamberVa=new G4VisAttributes(G4Colour(1,1,1));
      ChamberVa->SetForceSolid(true);
      Chamber->SetVisAttributes(G4VisAttributes::GetInvisible());



      //GAS
      G4LogicalVolume* Gas = lvStore->GetVolume("GAS");
      G4VisAttributes *GasVa=new G4VisAttributes(G4Colour(2,2,2));
      GasVa->SetForceCloud(true);
      Gas->SetVisAttributes(GasVa);

      //Source Enclosure Related
      G4LogicalVolume* SourceHolder = lvStore->GetVolume("SourceHolChamber_logic");
      G4LogicalVolume* Needle = lvStore->GetVolume("Needle");
      G4LogicalVolume* Collimator = lvStore->GetVolume("CollimatorWithBlock");
      G4VisAttributes *CollimatorVa=new G4VisAttributes(colours::YellowAlpha());
      CollimatorVa->SetForceSolid(true);

      Collimator->SetVisAttributes(CollimatorVa);



      Needle->SetVisAttributes(ChamberVa);

      G4LogicalVolume* SourceHolderBlock = lvStore->GetVolume("SourceHolChBlock_logic");
      G4VisAttributes *SourceHolderVa=new G4VisAttributes(G4Colour(2,2,2));
      SourceHolderVa->SetForceSolid(true);

      // Flange
      G4LogicalVolume* flangeLog = lvStore->GetVolume("CHAMBER_FLANGE");
      G4VisAttributes flangeVis=colours::DarkGreyAlpha();
      flangeVis.SetForceSolid(true);
      flangeLog->SetVisAttributes(ChamberVa);


      //PMT TUBE AND PMT BLOCK
      G4LogicalVolume * PmttubeLog0=lvStore->GetVolume("PMT_TUBE0");
      PmttubeLog0->SetVisAttributes(G4VisAttributes::GetInvisible());
      G4LogicalVolume * PmttubeBlockLog0=lvStore->GetVolume("PMT_TUBE_BLOCK0");
      G4LogicalVolume * PmttubeLog1=lvStore->GetVolume("PMT_TUBE1");
      PmttubeLog1->SetVisAttributes(G4VisAttributes::GetInvisible());
      G4LogicalVolume * PmttubeBlockLog1=lvStore->GetVolume("PMT_TUBE_BLOCK1");
      PmttubeBlockLog0->SetVisAttributes(ChamberVa);
      PmttubeBlockLog1->SetVisAttributes(ChamberVa);
      G4LogicalVolume * PmttubeVacuumLog1=lvStore->GetVolume("PMT_TUBE_VACUUM0");
      G4LogicalVolume * PmttubeVacuumLog2=lvStore->GetVolume("PMT_TUBE_VACUUM1");
      G4VisAttributes PmttubeVacuumVis=colours::Yellow();
      PmttubeVacuumVis.SetForceCloud(true);
      PmttubeVacuumLog1->SetVisAttributes(PmttubeVacuumVis);
      PmttubeVacuumLog2->SetVisAttributes(PmttubeVacuumVis);


      //MgF2Window
      G4LogicalVolume* lensLogical = lvStore->GetVolume("Lens");
      G4VisAttributes  MgF2LensVis=colours::Lilla();
      MgF2LensVis.SetForceSolid(true);
      lensLogical->SetVisAttributes(MgF2LensVis);

      G4LogicalVolume* MgF2WindowLog = lvStore->GetVolume("MgF2_WINDOW");
      G4VisAttributes  MgF2WindowVis=colours::Lilla();
      MgF2WindowVis.SetForceSolid(true);
      MgF2WindowLog->SetVisAttributes(MgF2WindowVis);

      /* // Lens
      G4LogicalVolume * LensLog=lvStore->GetVolume("LensMotherPV");
      G4VisAttributes  LensVis=Red();
      LensVis.SetForceSolid(true);
      LensLog->SetVisAttributes(LensVis);
      */

      // EL-Region
      G4LogicalVolume * ELLogic=lvStore->GetVolume("EL_GAP");
      G4VisAttributes ELVis=colours::BlueAlpha();
      ELVis.SetForceCloud(true);
      ELLogic->SetVisAttributes(ELVis);

      // FieldCage
      G4LogicalVolume * FieldCage=lvStore->GetVolume("FIELDCAGE");
      G4VisAttributes FielCageVis=colours::Red();
      FielCageVis.SetForceCloud(true);
      FieldCage->SetVisAttributes(FielCageVis);


      SourceHolder->SetVisAttributes(SourceHolderVa);
      SourceHolderBlock->SetVisAttributes(SourceHolderVa);
      Lab->SetVisAttributes(G4VisAttributes::GetInvisible());

    }

