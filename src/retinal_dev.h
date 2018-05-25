#ifndef MOSAIC_H_
#define MOSAIC_H_

#include "biodynamo.h"
#include "random.h"
#include "substance_initializers.h"
#include "neuroscience/compile_time_param.h"
#include "neuroscience/neurite_element.h"
#include "neuroscience/neuron_soma.h"

namespace bdm {
  // cell type: 0=on; 1=off; -1=not attributed yet
  // std::setprecision (15) <<

  /* TODO

  - other comparison needed. This is not just one mosaic, but two. have to take that into account. RI doesn't necessary reflect the mosaic (ie two mosaic without overlaping would have high R) but just the regularity. check for cells pairs (pair of diff types). cell fate + movement should have a better result than movement or cell fate alone.

  */


  BDM_SIM_OBJECT(MyNeurite, experimental::neuroscience::NeuriteElement) {
    BDM_SIM_OBJECT_HEADER(MyNeuriteExt, 1, has_to_retract_, beyond_threshold_, sleep_mode_, diam_before_retract_);

  public:
    MyNeuriteExt() {}
    MyNeuriteExt(const std::array<double, 3>& position) : Base(position) {}

    void SetHasToRetract(int r) { has_to_retract_[kIdx] = r; }
    bool GetHasToRetract() const { return has_to_retract_[kIdx]; }
    bool* GetHasToRetractPtr() { return has_to_retract_.data(); }

    void SetBeyondThreshold(int r) { beyond_threshold_[kIdx] = r; }
    bool GetBeyondThreshold() const { return beyond_threshold_[kIdx]; }
    bool* GetBeyondThresholdPtr() { return beyond_threshold_.data(); }

    void SetSleepMode(int r) { sleep_mode_[kIdx] = r; }
    bool GetSleepMode() const { return sleep_mode_[kIdx]; }
    bool* GetSleepModePtr() { return sleep_mode_.data(); }

    void SetDiamBeforeRetraction(double d) { diam_before_retract_[kIdx] = d; }
    double GetDiamBeforeRetraction() const { return diam_before_retract_[kIdx]; }
    double* GetDiamBeforeRetractionPtr() { return diam_before_retract_.data(); }

  private:
    vec<bool> has_to_retract_;
    vec<bool> beyond_threshold_;
    vec<bool> sleep_mode_;
    vec<int> diam_before_retract_;
  };

  double diamReducSpeed = 0.00052;
  double branchingFactor = 0.002;

  // inline double getBranchingFactor(NeuriteElement ne) {
  //   return branchingFactor(4.5/ne->GetDiameter()*ne->GetDiameter());
  // }

  //  enum Substances { on_substance_RGC_guide, off_substance_RGC_guide };
  enum Substances { on_diffusion, off_diffusion, on_substance_RGC_guide, off_substance_RGC_guide };

  struct RGC_dendrite_growth : public BaseBiologyModule {
    RGC_dendrite_growth() : BaseBiologyModule(gAllBmEvents) {}

    template <typename T>
    void Run(T* sim_object) {
      if (sim_object->template IsSoType<MyNeurite>()) {
        auto&& sim_objectNe = sim_object->template ReinterpretCast<MyNeurite>();

        if (!init_) {
          dg_on_RGCguide_ = GetDiffusionGrid(on_substance_RGC_guide);
          dg_off_RGCguide_ = GetDiffusionGrid(off_substance_RGC_guide);
          init_ = true;
        }

        double threshold_1 = 0.8;
        double threshold_2 = 0.6;
        double growthSpeed = 45;
        std::array<double, 3> gradient_RGCguide;
        double concentration;

        auto ne = sim_objectNe.GetSoPtr();
        if (ne->IsTerminal() && !ne->GetSleepMode()) {
          if (!ne->GetHasToRetract()) {

            auto& positionNeurite = ne->GetPosition();

            double oldDirectionWeight = 0.6; // cylinder axis
            double gradientWeight = 0.4;
            double randomnessWeight = 0.8;

            // initialise the correct substance as guide depending on cell type
            if (ne->GetNeurriteNeuronSoma()->GetCellType()==0) {
              dg_on_RGCguide_->GetGradient(positionNeurite, &gradient_RGCguide);
              concentration = dg_on_RGCguide_->GetConcentration(positionNeurite);
            }
            if (ne->GetNeurriteNeuronSoma()->GetCellType()==1) {
              dg_off_RGCguide_->GetGradient(positionNeurite, &gradient_RGCguide);
              concentration = dg_off_RGCguide_->GetConcentration(positionNeurite);
            }

            if (ne->GetDiameter() > 0.6 && gTRandom.Uniform(0, 1) < 4.5/ne->GetDiameter()*ne->GetDiameter()) {
              ne->SetDiameter(ne->GetDiameter()-0.0016);
              ne->Bifurcate();
              //TODO: add biologyModule to ne2
              // auto&& ne2 = ne->Bifurcate();
              // ne2.AddBiologyModule(RGC_dendrite_growth);
            }

            std::array<double, 3> random_axis={gTRandom.Uniform(-1, 1), gTRandom.Uniform(-1, 1), gTRandom.Uniform(-1, 1)};
            auto oldDirection = Math::ScalarMult(oldDirectionWeight, ne->GetSpringAxis());
            auto randomDirection = Math::ScalarMult(gradientWeight, Math::Normalize(gradient_RGCguide));
            auto gradDirection = Math::ScalarMult(randomnessWeight, random_axis);
            std::array<double, 3> newStepDirection =  Math::Add(Math::Add(oldDirection,randomDirection), gradDirection);

            ne->SetDiameter(ne->GetDiameter()-diamReducSpeed);
            ne->ElongateTerminalEnd(growthSpeed, newStepDirection);

            //              direction = Matrix::Normalize(Matrix::Add(Matrix::ScalarMult(5,direction),newStepDirection));

            // homo-type interaction
            // update external counters for neighbor neurites
            int ownType = 0; int otherType = 0;
            auto countNeighbours = [ne,&ownType,&otherType](auto&& neighbor, SoHandle neighbor_handle) {
              // if neighbor is a NeuriteElement
              if (neighbor.IsSoType(ne)) {
                auto&& neighbor_rc =
                neighbor.template ReinterpretCast<MyNeurite>();
                auto n_soptr = neighbor_rc.GetSoPtr();
                // if it is a direct relative
                if (n_soptr.GetNeurriteNeuronSoma() != ne->GetNeurriteNeuronSoma() && n_soptr.GetNeurriteNeuronSoma().GetCellType() == ne->GetNeurriteNeuronSoma().GetCellType()) {
                  ownType++;
                }
                else {
                  otherType++;
                }
              }
            };

            auto& grid = Grid<>::GetInstance();
            grid.ForEachNeighborWithinRadius(countNeighbours, *ne, ne->GetSoHandle(), 0.25);
            if (ownType > otherType) {
              ne->RetractTerminalEnd(10);
              // ne->RunDiscretization();
              // ne.SetDiameter(ne.GetDiameter()+0.01);
            }

            if (concentration < threshold_2 && ne->GetDiameter() < 0.9) {
              ne->SetHasToRetract(true);
              ne->SetBeyondThreshold(true);
              ne->SetDiamBeforeRetraction(ne->GetDiameter());
            }
          } // end not has to retract

          else { // if has to retract
            ne->RetractTerminalEnd(40);

            // what to do in case of retraction due to interaction
            if (!ne->GetBeyondThreshold() && ne->GetDiamBeforeRetraction()>ne->GetDiameter()+0.15){
              ne->SetHasToRetract(false);
              ne->SetSleepMode(true);
              // TODO: remove biologyModule for this neurite
              //             ne.removeLocalBiologyModule();
            }

            // if retraction due to threshold
            if (ne->GetBeyondThreshold()) {
              // initialise the correct substance as guide depending on cell type
              auto& positionNeurite = ne->GetPosition();
              if (ne->GetNeurriteNeuronSoma()->GetCellType()==0) {
                dg_on_RGCguide_->GetGradient(positionNeurite, &gradient_RGCguide);
                concentration = dg_on_RGCguide_->GetConcentration(positionNeurite);
              }
              if (ne->GetNeurriteNeuronSoma()->GetCellType()==1) {
                dg_off_RGCguide_->GetGradient(positionNeurite, &gradient_RGCguide);
                concentration = dg_off_RGCguide_->GetConcentration(positionNeurite);
              }
              if (concentration>threshold_1){
                ne->SetBeyondThreshold(false);
                ne->SetHasToRetract(false);
                ne->ElongateTerminalEnd(1, {gTRandom.Uniform(-1, 1), gTRandom.Uniform(-1, 1), gTRandom.Uniform(-1, 1)});
              }
            }
          } // end has to retract

        } // end if neurite is terminal

        //      } // end for neurite in cell
      }
    } // end run

  private:
    bool init_ = false;
    DiffusionGrid* dg_on_RGCguide_ = nullptr;
    DiffusionGrid* dg_off_RGCguide_ = nullptr;
    ClassDefNV(RGC_dendrite_growth, 1);

  }; // end biologyModule RGC_dendrite_growth

  // 0. Define my custom cell, which extends Cell by adding an extra
  // data member cell_type.
  // TODO: MyCell has to extend NeuronSoma, not Cell
  BDM_SIM_OBJECT(MyCell, experimental::neuroscience::NeuronSoma) {
    //  BDM_SIM_OBJECT(MyCell, Cell) {
    BDM_SIM_OBJECT_HEADER(MyCellExt, 1, cell_type_, internal_clock_);

  public:
    MyCellExt() {}

    MyCellExt(const std::array<double, 3>& position) : Base(position) {}

    void SetCellType(int t) { cell_type_[kIdx] = t; }
    int GetCellType() const { return cell_type_[kIdx]; }
    // This function is used by ParaView for coloring the cells by their type
    int* GetCellTypePtr() { return cell_type_.data(); }

    void SetInternalClock(int t) { internal_clock_[kIdx] = t; }
    int GetInternalClock() const { return internal_clock_[kIdx]; }
    int* GetInternalClockPtr() { return internal_clock_.data(); }


  private:
    vec<int> cell_type_;
    vec<int> internal_clock_;
  };

  //TODO: add external biology modules: bioM_*
  //  enum Substances { on_diffusion, off_diffusion };
  //  enum Substances { on_diffusion, off_diffusion, on_substance_RGC_guide, off_substance_RGC_guide };

  // 1a. Define behavior:
  struct Chemotaxis : public BaseBiologyModule {
    Chemotaxis() : BaseBiologyModule(gAllBmEvents) {}

    template <typename T>
    void Run(T* sim_object) {
      if (sim_object->template IsSoType<MyCell>()) {
        auto&& cell = sim_object->template ReinterpretCast<MyCell>();

        bool withCellDeath = true;
        bool withMovement = true;

        // if not initialised, initialise substance diffusions
        if (!init_) {
          dg_0_ = GetDiffusionGrid(on_diffusion);
          dg_1_ = GetDiffusionGrid(off_diffusion);
          init_ = true;
        }

        auto& position = cell->GetPosition();
        std::array<double, 3> diff_gradient;
        std::array<double, 3> gradient_z;
        double concentration=0;
        int cellClock=cell->GetInternalClock();

        // if cell is type 1, concentration and gradient are the one of substance 1
        if (cell->GetCellType() == 1) {
          dg_1_->GetGradient(position, &gradient_1_);
          gradient_z = Math::ScalarMult(0.08, gradient_1_);
          gradient_z[0] = 0; gradient_z[1]=0;
          diff_gradient = Math::ScalarMult(-0.1, gradient_1_);
          diff_gradient[2]=0;
          concentration = dg_1_->GetConcentration(position);
        }
        // else if cell is type 2, concentration and gradient are the one of substance 2
        if (cell->GetCellType() == 0) {
          dg_0_->GetGradient(position, &gradient_0_);
          gradient_z = Math::ScalarMult(0.08, gradient_0_);
          gradient_z[0] = 0; gradient_z[1]=0;
          diff_gradient = Math::ScalarMult(-0.1, gradient_0_);
          diff_gradient[2]=0;
          concentration = dg_0_->GetConcentration(position);
        }

        /* -- cell movement -- */
        if (withMovement && cellClock >= 200) { // && cellClock >= 1600 // 500
          // cell movement based on homotype substance gradient
          if(concentration>=0.10475) { // 0.1049 for high density - 0.104725 for normal density // 0.10465 for cell death with layer collapse - 0.1047 is too high (no collapse)
            cell->UpdatePosition(diff_gradient);
            cell->UpdatePosition(gradient_z);
            cell->SetPosition(cell->GetPosition());
          }
          // random movement
          //        cell->UpdatePosition({gTRandom.Uniform(-1, 1), gTRandom.Uniform(-1, 1), 0});
        }

        /* -- cell death -- */
        //      if (withCellDeath && cellClock >= 200 && cellClock < 800) {
        if (withCellDeath && cellClock >= 200) { // && cellClock >= 500 && cellClock < 1600
          if (!withMovement && cellClock < 400 && concentration>=0.104725) {
            cell->UpdatePosition({gTRandom.Uniform(-0.2, 0.2), gTRandom.Uniform(-0.2, 0.2), 0});
            cell->SetPosition(cell->GetPosition());
          }
          // cell death depending on homotype substance concentration
          if (concentration >= 0.11 && gTRandom.Uniform(0, 1) < 0.1) { // die if concentration is too high; proba so all cells don't die simultaneously ; 0.1047 for already existing mosaic - 0.1048 for random position - 0.1047x for inbetween
          cell->RemoveFromSimulation();
        }
        // add vertical migration as the multi layer colapse in just on layer
        if(concentration>=0.1045) { // 104
          cell->UpdatePosition(gradient_z);
          //          cell->UpdatePosition(diff_gradient);
          cell->SetPosition(cell->GetPosition());
        }
        // randomly kill ~60% cells (over 250 steps)
        //      if (gTRandom.Uniform(0, 1) < 0.004) {
        //        cell->RemoveFromSimulation();
        //      }
      }

      /* -- cell fate -- */
      // cell type attribution depending on concentrations
      if (cell->GetCellType() == -1) { // if cell type is not on or off
        dg_1_->GetGradient(position, &gradient_1_);
        double concentration_1 = dg_1_->GetConcentration(position);
        dg_0_->GetGradient(position, &gradient_0_);
        double concentration_0 = dg_0_->GetConcentration(position);

        if (concentration_1 == 0 && concentration_0 == 0) { // if no substances
          if (gTRandom.Uniform(0, 1) < 0.0001) { // so all cell types doesn't create all randomly at step 1
          cell->SetCellType((int) gTRandom.Uniform(0, 2)); // random attribution of cell type
        }
      }

      //        if (concentration_1 > concentration_0 && concentration_1 > 0.000001) { // if off > on // 0.000001
      if (concentration_1 > concentration_0*4 && concentration_1 > 0.00001) {
        if (gTRandom.Uniform(0, 1) < 0.1) { // so all cell doesn't choose their type in the same time
        cell->SetCellType(0); // cell become on
      }
    }

    //        if ( concentration_0 > concentration_1 && concentration_0 > 0.000001) { // if on > off // 0.000001
    if ( concentration_0 > concentration_1*4 && concentration_0 > 0.00001) {
      if (gTRandom.Uniform(0, 1) < 0.1) { // so cells don't choose their type in the same time
      cell->SetCellType(1); // cell become off
    }
  }
} // end cell type = -1

if (gTRandom.Uniform(0, 1) < 0.96) {
  cell->SetInternalClock(cell->GetInternalClock()+1); // update cell internal clock
}

} // end of if neuron soma
} // end Run()

private:
  bool init_ = false;
  DiffusionGrid* dg_0_ = nullptr;
  DiffusionGrid* dg_1_ = nullptr;
  std::array<double, 3> gradient_0_;
  std::array<double, 3> gradient_1_;
  ClassDefNV(Chemotaxis, 1);
}; // end biologyModule Chemotaxis

// 1b. Define secretion behavior:
struct SubstanceSecretion : public BaseBiologyModule {
  // Daughter cells inherit this biology module
  SubstanceSecretion() : BaseBiologyModule(gAllBmEvents) {}

  template <typename T>
  void Run(T* sim_object) {
    if (sim_object->template IsSoType<MyCell>()) {
      auto&& cell = sim_object->template ReinterpretCast<MyCell>();

      if (!init_) {
        dg_0_ = GetDiffusionGrid(on_diffusion);
        dg_1_ = GetDiffusionGrid(off_diffusion);
        init_ = true;
      }
      auto& secretion_position = cell->GetPosition();

      if (cell->GetCellType() == 1) { // if off cell, secrete off cells substance
        dg_1_->IncreaseConcentrationBy(secretion_position, 0.1); // 0.1
      }
      else if (cell->GetCellType() == 0) { // is on cell, secrete on cells substance
        dg_0_->IncreaseConcentrationBy(secretion_position, 0.1);
      }
    }
  }

private:
  bool init_ = false;
  DiffusionGrid* dg_0_ = nullptr;
  DiffusionGrid* dg_1_ = nullptr;
  ClassDefNV(SubstanceSecretion, 1);
}; // end biologyModule SubstanceSecretion


// 2. Define compile time parameter
template <typename Backend>
struct CompileTimeParam : public DefaultCompileTimeParam<Backend> {
  using BiologyModules = Variant<Chemotaxis, SubstanceSecretion, RGC_dendrite_growth>;
  using AtomicTypes = VariadicTypedef<MyCell, MyNeurite>;
  using NeuronSoma = MyCell;
  using NeuriteElement = MyNeurite;
};

// my cell creator
template <typename Function, typename TResourceManager = ResourceManager<>>
static void CellCreator(double min, double max, int num_cells, Function cell_builder) {
  auto rm = TResourceManager::Get();

  // Determine simulation object type which is returned by the cell_builder
  using FunctionReturnType = decltype(cell_builder({0, 0, 0}));

  auto container = rm->template Get<FunctionReturnType>();
  container->reserve(num_cells);

  for (int i = 0; i < num_cells; i++) {
    double x = gTRandom.Uniform(min+20, max-20);
    double y = gTRandom.Uniform(min+20, max-20);
    double z = gTRandom.Uniform(min+20, 40); //24
    auto new_simulation_object = cell_builder({x, y, z});
    container->push_back(new_simulation_object);
  }
  container->Commit();
} // end CellCreator

// position exporteur
template <typename TResourceManager = ResourceManager<>>
inline void position_exporteur(int i) {
  ofstream positionFileOn;
  ofstream positionFileOff;
  ofstream positionFileAll;
  std::stringstream sstmOn;
  std::stringstream sstmOff;
  std::stringstream sstmAll;
  sstmOn << "on_t" << i << ".txt";
  sstmOff << "off_t" << i << ".txt";
  sstmAll << "all_t" << i << ".txt";

  string fileNameOn = sstmOn.str();
  string fileNameOff = sstmOff.str();
  string fileNameAll = sstmAll.str();
  positionFileOn.open(fileNameOn); // TODO: include seed number to file name
  positionFileOff.open(fileNameOff);
  positionFileAll.open(fileNameAll);

  auto rm = TResourceManager::Get();
  auto my_cells=rm->template Get<MyCell>();
  int numberOfCells = my_cells->size();

  for (int cellNum=0; cellNum< numberOfCells; cellNum++) {
    auto thisCellType = (*my_cells)[cellNum].GetCellType();
    auto position = (*my_cells)[cellNum].GetPosition();
    if (thisCellType == 0) {
      positionFileOn << position[0] << " " << position[1] << "\n";
      positionFileAll << position[0] << " " << position[1] << " " << position[2] << " on\n";
    }
    else {
      positionFileOff << position[0] << " " << position[1] << "\n";
      positionFileAll << position[0] << " " << position[1] << " " << position[2] << " off\n";
    }
  }
  positionFileOn.close();
  positionFileOff.close();
  positionFileAll.close();
} // end position_exporteur

// RI computation
inline double computeRI(std::vector<array<double, 3>> coordList) {
  std::vector<double> shortestDistList;
  for (unsigned int i=0; i<coordList.size(); i++) { // for each cell of same type in the simulation
    array<double, 3> cellPosition = coordList[i];

    double shortestDist=9999;
    for (unsigned int j=0; j<coordList.size(); j++) { // for each other cell of same type in the simulation
      array<double, 3> otherCellPosition = coordList[j];

      double tempsDistance=sqrt(pow(cellPosition[0]-otherCellPosition[0], 2) + pow(cellPosition[1]-otherCellPosition[1], 2)); // get the distance between those 2 cells
      if (tempsDistance<shortestDist && tempsDistance!=0){ // if cell is not itself
        shortestDist=tempsDistance; // updade closest neighbour distance
      }
    }
    shortestDistList.push_back(shortestDist); // save the shortest distance to neighbour
  }
  //compute mean
  double temps_sum=0;
  for (unsigned int i=0; i<shortestDistList.size(); i++) {
    temps_sum+=shortestDistList[i];
  }
  double aveShortestDist=temps_sum/ (double) shortestDistList.size();
  //compute std
  double temp=0;
  for (unsigned int i = 0; i < shortestDistList.size(); i++){
    double val = shortestDistList[i];
    double squrDiffToMean = pow(val - aveShortestDist, 2);
    temp += squrDiffToMean;
  }
  double meanOfDiffs = temp / (double) (shortestDistList.size());
  double std=sqrt(meanOfDiffs);

  return aveShortestDist/std; // return RI
} // end computeRI

template <typename TResourceManager = ResourceManager<>>
inline  double getRI(int desiredCellType) {
  auto rm = TResourceManager::Get();
  auto my_cells=rm->template Get<MyCell>(); // get cell list
  std::vector<array<double, 3>> coordList; // list of coordinate
  int numberOfCells = my_cells->size();

  for (int cellNum=0; cellNum< numberOfCells; cellNum++) { // for each cell in simulation
    auto thisCellType = (*my_cells)[cellNum].GetCellType();
    if (thisCellType == desiredCellType) { // if cell is of the desired type
      auto position = (*my_cells)[cellNum].GetPosition(); // get its position
      coordList.push_back(position); // put cell coord in the list
    }
  }
  //    cout << coordList.size() << " cells of type " << desiredCellType << endl;
  return computeRI(coordList); // return RI for desired cell type
} //; end getRI

/* -------- simulate -------- */
template <typename TResourceManager = ResourceManager<>>
inline int Simulate(int argc, const char** argv) {
  InitializeBiodynamo(argc, argv);
  // 3. Define initial model

  // Create an artificial bounds for the simulation space
  int cubeDim= 500; //100 //500
  int num_cells = 4400; //55 //1250 // 2500
  double cellDensity = (double) num_cells*1000000/(cubeDim*cubeDim);
  cout << "cell density: " << cellDensity << " cells per cm2" << endl;

  Param::bound_space_ = true;
  Param::min_bound_ = 0;
  Param::max_bound_ = cubeDim+40; // cell are created with +20 to min and -20 to max. so physical cube has to be cubeDim+40
  Param::run_mechanical_interactions_ = true;

  int mySeed=rand()%10000;
  mySeed=9784; // 9784
  gTRandom.SetSeed(mySeed);
  cout << "modelling with seed " << mySeed << endl;

  // Construct num_cells/2 cells of on cells (type 0)
  auto construct_on = [](const std::array<double, 3>& position) {
    MyCell cell(position);
    cell.SetDiameter(gTRandom.Uniform(8, 9)); // random diameter between 8 and 9
    cell.SetCellType(0);
    cell.SetInternalClock(0);
    cell.AddBiologyModule(SubstanceSecretion());
    cell.AddBiologyModule(Chemotaxis());
    return cell;
  };
  CellCreator(Param::min_bound_, Param::max_bound_, 0, construct_on); // num_cells/2
  //TODO: get actual number of on cells to check if cell creation is okay
  cout << "on cells created" << endl;

  // Construct num_cells/2 cells of off cells (type 1)
  auto construct_off = [](const std::array<double, 3>& position) {
    MyCell cell(position);
    cell.SetDiameter(gTRandom.Uniform(8, 9)); // random diameter between 8 and 9
    cell.SetCellType(1);
    cell.SetInternalClock(0);
    cell.AddBiologyModule(SubstanceSecretion());
    cell.AddBiologyModule(Chemotaxis());
    return cell;
  };
  CellCreator(Param::min_bound_, Param::max_bound_, 0, construct_off); // num_cells/2
  //TODO: get actual number of off cells to check if cell creation is okay
  cout << "off cells created" << endl;

  // construct neutral cells (type -1)
  auto construct_nonType = [](const std::array<double, 3>& position) {
    MyCell cell(position);
    cell.SetDiameter(gTRandom.Uniform(8, 9));
    cell.SetCellType(-1);
    cell.SetInternalClock(0);
    cell.AddBiologyModule(SubstanceSecretion());
    cell.AddBiologyModule(Chemotaxis());
    return cell;
  };
  CellCreator(Param::min_bound_, Param::max_bound_, num_cells, construct_nonType); // num_cells
  cout << "neutral cells created" << endl;

  // 3. Define the substances that cells may secrete
  // Order: substance_name, diffusion_coefficient, decay_constant, resolution
  ModelInitializer::DefineSubstance(on_diffusion, "on_diffusion", 1, 0.5, 2); // 1, 0.5, 2
  ModelInitializer::DefineSubstance(off_diffusion, "off_diffusion", 1, 0.5, 2); // 1, 0.5, 2

  // define substances for RGC dendrite attraction
  ModelInitializer::DefineSubstance(on_substance_RGC_guide, "on_substance_RGC_guide", 0.5,0.1,2);
  ModelInitializer::DefineSubstance(off_substance_RGC_guide, "off_substance_RGC_guide", 0.5,0.1,2);
  // create substance gaussian distribution for RGC dendrite attraction
  ModelInitializer::InitializeSubstance(on_substance_RGC_guide, "on_substance_RGC_guide", GaussianBand(20, 6, Axis::kZAxis));
  ModelInitializer::InitializeSubstance(off_substance_RGC_guide, "off_substance_RGC_guide", GaussianBand(44, 8, Axis::kZAxis));

  // set visualization param
  Param::live_visualization_ = false;
  Param::export_visualization_ = true;
  Param::visualization_export_interval_ = 1;
  Param::visualize_sim_objects_["MyCell"] = std::set<std::string>{"diameter_", "cell_type_"};

  // set some param
  int maxStep=1201; // number of simulation steps // 1201 // 2501
  bool writeOutput = true; // if you want to write file for RI and cell position
  int outputFrequence = 100; // create cell position files every outputFrequence steps
  ofstream outputFile;

  if (writeOutput) {
    outputFile.open("RI_" + std::to_string(mySeed) + ".txt");
  }

  // 4. Run simulation for maxStep timesteps
  Scheduler<> scheduler;

  auto rm =TResourceManager::Get();
  auto my_cells=rm->template Get<MyCell>();
  int numberOfCells = my_cells->size();
  int numberOfCells0=0; int numberOfCells1=0;

  for (int i=0; i<maxStep; i++) {
    scheduler.Simulate(1);

    if (i%10==0){ // write RI in file
      double RIon=getRI(0);
      double RIoff=getRI(1);
      //        cout << "RI on: " << RIon << " ; RI off: " << RIoff << endl;
      if (writeOutput) {
        outputFile << RIon << " " << RIoff << "\n";
      }

      if (i%100==0){ // print
        // get cell list size
        rm =TResourceManager::Get();
        my_cells = rm->template Get<MyCell>();
        numberOfCells = my_cells->size();
        numberOfCells0=0; numberOfCells1=0;

        for (int cellNum=0; cellNum< numberOfCells; cellNum++) { // for each cell in simulation
          auto thisCellType = (*my_cells)[cellNum].GetCellType();
          if (thisCellType == 0) {
            numberOfCells0++;
          }
          else if (thisCellType == 1){
            numberOfCells1++;
          }
        }
        cout << numberOfCells << " cells in simulation: " << (1-((double) numberOfCells/num_cells))*100 << "% of cell death\n" << numberOfCells0 << " cells are type 0 (on) ; " << numberOfCells1 << " cells are type 1 (off)"<< endl;
        cout << "RI on: " << RIon << " ; RI off: " << RIoff << " ; mean: " << (RIon+RIoff)/2 << endl;
      }
    }

    if (i%outputFrequence == 0 && writeOutput) { // export cell position
      position_exporteur(i);
    }
  }
  outputFile.close();

  return 0;
}

} // namespace bdm

#endif // MOSAIC_H_
