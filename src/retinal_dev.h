#ifndef RETINAL_DEV_
#define RETINAL_DEV_

#include "biodynamo.h"
#include "neuroscience/neuroscience.h"
#include "substance_initializers.h"

namespace bdm {
// cell type: 0=on; 1=off; -1=not differentiated yet
// std::cout << setprecision (15) <<

// TODO: add external biology modules

/* TODO
- other comparison needed. This is not just one mosaic, but two. have to take
that into account. RI doesn't necessary reflect the mosaic (ie two mosaic
without overlaping would have high R) but just the regularity. check for cells
pairs (pair of diff types). cell fate + movement should have a better result
than movement or cell fate alone.
*/

using namespace std;
//using experimental::neuroscience::SplitNeuriteElementEvent;

enum Substances {
  on_diffusion,
  off_diffusion,
  on_substance_RGC_guide,
  off_substance_RGC_guide
};

// Define my custom cell MyCell extending NeuronSoma
BDM_SIM_OBJECT(MyCell, experimental::neuroscience::NeuronSoma) {
  BDM_SIM_OBJECT_HEADER(MyCellExt, 1, cell_type_, internal_clock_, labelSWC_);

 public:
  MyCellExt() {}

  MyCellExt(const array<double, 3>& position) : Base(position) {}

  /// Default event constructor
  template <typename TEvent, typename TOther>
  MyCellExt(const TEvent& event, TOther* other, uint64_t new_oid = 0) {
    // TODO(jean) implement
  }

  /// Default event handler (exising biology module won't be modified on
  /// any event)
  template <typename TEvent, typename... TOthers>
  void EventHandler(const TEvent&, TOthers*...) {
    // TODO(jean) implement
  }

  void SetCellType(int t) { cell_type_[kIdx] = t; }
  int GetCellType() const { return cell_type_[kIdx]; }
  // This function is used by ParaView for coloring the cells by their type
  int* GetCellTypePtr() { return cell_type_.data(); }

  void SetInternalClock(int t) { internal_clock_[kIdx] = t; }
  int GetInternalClock() const { return internal_clock_[kIdx]; }

  inline void SetLabel(int label) { labelSWC_[kIdx] = label; }
  inline int GetLabel() { return labelSWC_[kIdx]; }
  inline void IncreaseLabel() { labelSWC_[kIdx] = labelSWC_[kIdx] + 1; }

 private:
  vec<int> cell_type_;
  vec<int> internal_clock_;
  vec<int> labelSWC_;
};

// Define my custom neurite MyNeurite, which extends NeuriteElement
BDM_SIM_OBJECT(MyNeurite, experimental::neuroscience::NeuriteElement) {
  BDM_SIM_OBJECT_HEADER(MyNeuriteExt, 1, has_to_retract_, beyond_threshold_,
                        sleep_mode_, diam_before_retract_, subtype_, its_soma_);

 public:
  MyNeuriteExt() {}
  MyNeuriteExt(const array<double, 3>& position) : Base(position) {}

  /// Default event constructor
  template <typename TEvent, typename TOther>
  MyNeuriteExt(const TEvent& event, TOther* other, uint64_t new_oid = 0) {
    subtype_[kIdx] = other->subtype_[other->kIdx];
  }

  template <typename TOther>
  MyNeuriteExt(const experimental::neuroscience::NewNeuriteExtensionEvent& event, TOther* other, uint64_t new_oid = 0) {}

  // event handler for SplitNeuriteElementEvent
  template <typename TEvent, typename... TOthers>
  void EventHandler(const TEvent&, TOthers*...) {}

  void SetHasToRetract(int r) { has_to_retract_[kIdx] = r; }
  bool GetHasToRetract() const { return has_to_retract_[kIdx]; }

  void SetBeyondThreshold(int r) { beyond_threshold_[kIdx] = r; }
  bool GetBeyondThreshold() const { return beyond_threshold_[kIdx]; }

  void SetSleepMode(int r) { sleep_mode_[kIdx] = r; }
  bool GetSleepMode() const { return sleep_mode_[kIdx]; }

  void SetDiamBeforeRetraction(double d) { diam_before_retract_[kIdx] = d; }
  double GetDiamBeforeRetraction() const { return diam_before_retract_[kIdx]; }

  void SetSubtype(int st) { subtype_[kIdx] = st; }
  int GetSubtype() { return subtype_[kIdx]; }
  // ParaView
  int* GetSubtypePtr() { return subtype_.data(); }

  void SetMySoma(MyCell soma) { its_soma_[kIdx] = &soma; }
  MyCell GetMySoma() { return *its_soma_[kIdx]; }

 private:
  vec<bool> has_to_retract_;
  vec<bool> beyond_threshold_;
  vec<bool> sleep_mode_;
  vec<int> diam_before_retract_;
  vec<int> subtype_;
  vec<MyCell*> its_soma_;
};

// Define dendrites behavior for RGC dendritic growth
struct RGC_dendrite_growth_BM : public BaseBiologyModule {
  RGC_dendrite_growth_BM() : BaseBiologyModule(gAllEventIds) {}

  /// Default event constructor
  template <typename TEvent, typename TBm>
  RGC_dendrite_growth_BM(const TEvent& event, TBm* other,
                           uint64_t new_oid = 0) {
  }

  /// Default event handler (exising biology module won't be modified on
  /// any event)
  template <typename TEvent, typename... TBms>
  void EventHandler(const TEvent&, TBms*...) {
  }

  template <typename T, typename TSimulation = Simulation<>>
  void Run(T* ne) {
        auto* sim = TSimulation::GetActive();
        auto* random = sim->GetRandom();
    //    auto* param = sim->GetParam();
        auto* rm = sim->GetResourceManager();

          if (ne->IsTerminal() && ne->GetDiameter() >= 0.5) {
            if (!init_) {
              dg_on_RGCguide_ =
              rm->GetDiffusionGrid("on_substance_RGC_guide");
              dg_off_RGCguide_ =
              rm->GetDiffusionGrid("off_substance_RGC_guide");
              init_ = true;
            }
            array<double, 3> gradient_RGCguide;
            double concentration = 0;
            // initialise the correct substance as guide depending on cell type
            if (ne->GetSubtype()/100 == 0) {
              dg_on_RGCguide_->GetGradient(ne->GetPosition(),
              &gradient_RGCguide);
              concentration =
              dg_on_RGCguide_->GetConcentration(ne->GetPosition());
            }
            if (ne->GetSubtype()/100 == 1) {
              dg_off_RGCguide_->GetGradient(ne->GetPosition(),
              &gradient_RGCguide);
              concentration =
              dg_off_RGCguide_->GetConcentration(ne->GetPosition());
            }
            if (ne->GetSubtype()/100 == 2) {
              double conc_on =
              dg_on_RGCguide_->GetConcentration(ne->GetPosition());
              double conc_off =
              dg_off_RGCguide_->GetConcentration(ne->GetPosition());
              if (conc_on > conc_off) {
                concentration = conc_on;
                dg_on_RGCguide_->GetGradient(ne->GetPosition(),
                &gradient_RGCguide);
              } else {
                concentration = conc_off;
                dg_off_RGCguide_->GetGradient(ne->GetPosition(),
                &gradient_RGCguide);
              }
            }

            // if neurite doesn't have to retract
            if (!ne->GetHasToRetract()) {
              double gradientWeight = 0.2;
              double randomnessWeight = 0.2;
              double oldDirectionWeight = 1.6;
              array<double, 3> random_axis = {random->Uniform(-1, 1),
                                              random->Uniform(-1, 1),
                                              random->Uniform(-1, 1)};
              auto oldDirection =
                  Math::ScalarMult(oldDirectionWeight, ne->GetSpringAxis());
              auto gradDirection = Math::ScalarMult(
                  gradientWeight, Math::Normalize(gradient_RGCguide));
              auto randomDirection =
                  Math::ScalarMult(randomnessWeight, random_axis);
              array<double, 3> newStepDirection = Math::Add(
                  Math::Add(oldDirection, randomDirection), gradDirection);

              ne->ElongateTerminalEnd(25, newStepDirection);
              ne->SetDiameter(ne->GetDiameter()-0.0008);

              if (concentration > 0.04
                && random->Uniform() < 0.0095*ne->GetDiameter()) {
                ne->SetDiameter(ne->GetDiameter()-0.005);
                ne->Bifurcate();
              }

              // homo-type interaction
              int ownType = 0;
              int otherType = 0;
              // lambda updating counters for neighbor neurites
              auto countNeighbours = [&](auto&& neighbor, SoHandle
              neighbor_handle) {
                // if neighbor is a NeuriteElement
                if (neighbor->template IsSoType<MyNeurite>()) {
                  auto&& neighbor_rc = neighbor->template
                    ReinterpretCast<MyNeurite>();
                  auto n_soptr = neighbor_rc->GetSoPtr();
                  // if not a direct relative but same cell type
                  if (n_soptr->GetNeuronSomaOfNeurite() !=
                      ne->GetNeuronSomaOfNeurite() &&
                      n_soptr->GetSubtype()/100 == ne->GetSubtype()/100) {
                    ownType++;
                  }
                  else if (n_soptr->GetMySoma() != ne->GetMySoma()
                    && n_soptr->GetSubtype()/100 != ne->GetSubtype()/100) {
                    otherType++;
                  }
                }
              }; // end lambda

              auto* grid = sim->GetGrid();
              grid->ForEachNeighborWithinRadius(
                countNeighbours, *ne, ne->GetSoHandle(), 4);

              if (ownType > otherType) {
                ne->SetHasToRetract(true);
                ne->SetDiamBeforeRetraction(ne->GetDiameter());
              }

            } // if ! has to retract

            // if neurite has to retract
            else {
              ne->RetractTerminalEnd(40);
            }

          } // if is terminal
  }  // end run

 private:
  bool init_ = false;
  DiffusionGrid* dg_on_RGCguide_ = nullptr;
  DiffusionGrid* dg_off_RGCguide_ = nullptr;
  ClassDefNV(RGC_dendrite_growth_BM, 1);
}; // end RGC_dendrite_growth_BM

// Define cell behavior for mosaic formation
struct RGC_mosaic_BM : public BaseBiologyModule {
  RGC_mosaic_BM() : BaseBiologyModule(gAllEventIds) {}

  /// Default event constructor
  template <typename TEvent, typename TBm>
  RGC_mosaic_BM(const TEvent& event, TBm* other, uint64_t new_oid = 0) {
  }

  /// Default event handler (exising biology module won't be modified on
  /// any event)
  template <typename TEvent, typename... TBms>
  void EventHandler(const TEvent&, TBms*...) {
  }

  template <typename T, typename TSimulation = Simulation<>>
  void Run(T* cell) {
    auto* sim = TSimulation::GetActive();
    auto* random = sim->GetRandom();

    bool withCellDeath = true;  // run cell death mechanism
    bool withMovement = true;   // run tangential migration

    // if not initialised, initialise substance diffusions
    if (!init_) {
      auto* rm = sim->GetResourceManager();
      dg_0_ = rm->GetDiffusionGrid("on_diffusion");
      dg_1_ = rm->GetDiffusionGrid("off_diffusion");
      dg_2_ = rm->GetDiffusionGrid("on_off_diffusion");
      init_ = true;
    }

    auto& position = cell->GetPosition();
    array<double, 3> gradient_0_;
    array<double, 3> gradient_1_;
    array<double, 3> gradient_2_;
    array<double, 3> diff_gradient;
    array<double, 3> gradient_z;
    double concentration = 0;
    int cellClock = cell->GetInternalClock();

    // if cell is type 0, concentration and gradient are substance 0
    if (cell->GetCellType() == 0) {
      dg_0_->GetGradient(position, &gradient_0_);
      gradient_z = Math::ScalarMult(0.2, gradient_0_);
      gradient_z[0] = 0;
      gradient_z[1] = 0;
      diff_gradient = Math::ScalarMult(-0.1, gradient_0_);
      diff_gradient[2] = 0;
      concentration = dg_0_->GetConcentration(position);
    }
    // if cell is type 1, concentration and gradient are substance 1
    if (cell->GetCellType() == 1) {
      dg_1_->GetGradient(position, &gradient_1_);
      gradient_z = Math::ScalarMult(0.2, gradient_1_);
      gradient_z[0] = 0;
      gradient_z[1] = 0;
      diff_gradient = Math::ScalarMult(-0.1, gradient_1_);
      diff_gradient[2] = 0;
      concentration = dg_1_->GetConcentration(position);
    }
    // if cell is type 2, concentration and gradient are substance 2
    if (cell->GetCellType() == 2) {
      dg_2_->GetGradient(position, &gradient_2_);
      gradient_z = Math::ScalarMult(0.2, gradient_2_);
      gradient_z[0] = 0;
      gradient_z[1] = 0;
      diff_gradient = Math::ScalarMult(-0.1, gradient_2_);
      diff_gradient[2] = 0;
      concentration = dg_2_->GetConcentration(position);
    }

    if (cellClock < 1400) {
      // add small random movements
      cell->UpdatePosition(
          {random->Uniform(-0.1, 0.1), random->Uniform(-0.1, 0.1), 0});
      // cell growth
      if (cell->GetDiameter() < 14 && random->Uniform(0, 1) < 0.01) {
        cell->ChangeVolume(5500);
      }
    }

    if (cellClock == 1600 && cell->GetCellType() != -1) {
      // dendrite per cell: average=4.5; std=1.2
      int thisSubType = cell->GetCellType()*100 + (int)random->Uniform(0, 20);
      for (int i = 0; i <= (int)random->Uniform(2, 7); i++) {
        auto&& ne = cell->ExtendNewNeurite({0, 0, 1});
        ne->GetSoPtr()->AddBiologyModule(RGC_dendrite_growth_BM());
        ne->GetSoPtr()->SetHasToRetract(false);
        ne->GetSoPtr()->SetSleepMode(false);
        ne->GetSoPtr()->SetBeyondThreshold(false);
        ne->GetSoPtr()->SetSubtype(thisSubType);
        ne->GetSoPtr()->SetMySoma(cell);
      }
    }

    /* -- cell movement -- */
    if (withMovement && cellClock >= 200 && cellClock < 1600) {
      // cell movement based on homotype substance gradient
      // 0. with cell death - 0. without
      if (concentration >= 1.28427) {  // 1.28428 - no artifact - ~4.5
        cell->UpdatePosition(diff_gradient);
      }
    }  // end tangential migration

    /* -- cell death -- */
    if (withCellDeath && cellClock >= 200 && cellClock < 1600) {
      // add vertical migration as the multi layer colapse in just on layer
      cell->UpdatePosition(gradient_z);
      // cell death depending on homotype substance concentration
      // with cell fate: 1.29208800011691 - 1.2920880001169 - 1.29
      // with cell movement:
      // with cell fate and cell movement: 1.285 - random < 0.01
      if (concentration > 1.285 && random->Uniform(0, 1) < 0.01) {
        cell->RemoveFromSimulation();
      }

      // cell death for homotype cells in contact
      auto killNeighbor = [&](auto&& neighbor, SoHandle neighbor_handle) {
        if (neighbor->template IsSoType<MyCell>()) {
          auto&& neighbor_rc = neighbor->template ReinterpretCast<MyCell>();
          auto n_soptr = neighbor_rc->GetSoPtr();
          if (cell->GetCellType() == n_soptr->GetCellType() &&
              random->Uniform(0, 1) < 0.2) {
            n_soptr->RemoveFromSimulation();
          }
        }
      };
      auto* grid = sim->GetGrid();
      grid->ForEachNeighborWithinRadius(
          killNeighbor, *cell, cell->GetSoHandle(), cell->GetDiameter() * 1.5);
    }  // end cell death

    /* -- cell fate -- */
    // cell type attribution depending on concentrations
    // if cell in undifferentiated; random so don't change simultaneously
    if (cell->GetCellType() == -1 && random->Uniform(0, 1) < 0.05) {
      dg_0_->GetGradient(position, &gradient_0_);
      double concentration_0 = dg_0_->GetConcentration(position);
      dg_1_->GetGradient(position, &gradient_1_);
      double concentration_1 = dg_1_->GetConcentration(position);
      dg_2_->GetGradient(position, &gradient_2_);
      double concentration_2 = dg_2_->GetConcentration(position);

      map<int, double> concentrationMap;
      concentrationMap[0] = concentration_0;
      concentrationMap[1] = concentration_1;
      concentrationMap[2] = concentration_2;

      vector<int> possibleCellType;
      int nbOfZero = 0;
      double smallestValue = 1e10;
      int smallestConcentrationType = -1;

      for (auto it = concentrationMap.begin(); it != concentrationMap.end();
           ++it) {
        if (it->second == 0) {
          possibleCellType.push_back(it->first);
          smallestConcentrationType = it->first;
          nbOfZero++;
        }
        if (it->second < smallestValue) {
          smallestValue = it->second;
          smallestConcentrationType = it->first;
        }
      }

      if (nbOfZero < 2) {
        cell->SetCellType(smallestConcentrationType);
      } else {
        cell->SetCellType(
            possibleCellType[random->Uniform(0, possibleCellType.size())]);
      }
    }  // end cell type = -1

    /* -- internal clock -- */
    // probability to increase internal clock
    if (random->Uniform(0, 1) < 0.96) {
      // update cell internal clock
      cell->SetInternalClock(cell->GetInternalClock() + 1);
    }  // end update cell internal clock

  }  // end Run()

 private:
  bool init_ = false;
  DiffusionGrid* dg_0_ = nullptr;
  DiffusionGrid* dg_1_ = nullptr;
  DiffusionGrid* dg_2_ = nullptr;
  ClassDefNV(RGC_mosaic_BM, 1);
};  // end biologyModule RGC_mosaic_BM

// Define secretion behavior:
struct Substance_secretion_BM : public BaseBiologyModule {
  // Daughter cells inherit this biology module
  Substance_secretion_BM() : BaseBiologyModule(gAllEventIds) {}

  /// Default event constructor
  template <typename TEvent, typename TBm>
  Substance_secretion_BM(const TEvent& event, TBm* other, uint64_t new_oid = 0) {
  }

  /// Default event handler (exising biology module won't be modified on
  /// any event)
  template <typename TEvent, typename... TBms>
  void EventHandler(const TEvent&, TBms*...) {
  }

  template <typename T, typename TSimulation = Simulation<>>
  void Run(T* cell) {
    auto* sim = TSimulation::GetActive();

    if (!init_) {
      auto* rm = sim->GetResourceManager();
      dg_0_ = rm->GetDiffusionGrid("on_diffusion");
      dg_1_ = rm->GetDiffusionGrid("off_diffusion");
      dg_2_ = rm->GetDiffusionGrid("on_off_diffusion");
      init_ = true;
    }
    auto& secretion_position = cell->GetPosition();
    // if on cell, secrete on cells substance
    if (cell->GetCellType() == 0) {
      dg_0_->IncreaseConcentrationBy(secretion_position, 1);
    }
    // is off cell, secrete off cells substance
    else if (cell->GetCellType() == 1) {
      dg_1_->IncreaseConcentrationBy(secretion_position, 1);
    }
    // is on-off cell, secrete on-off cells substance
    else if (cell->GetCellType() == 2) {
      dg_2_->IncreaseConcentrationBy(secretion_position, 1);
    }
  }

 private:
  bool init_ = false;
  DiffusionGrid* dg_0_ = nullptr;
  DiffusionGrid* dg_1_ = nullptr;
  DiffusionGrid* dg_2_ = nullptr;
  ClassDefNV(Substance_secretion_BM, 1);
};  // end biologyModule Substance_secretion_BM

// define compile time parameter
BDM_CTPARAM(experimental::neuroscience) {
  BDM_CTPARAM_HEADER(experimental::neuroscience);

  using SimObjectTypes = CTList<MyCell, MyNeurite>;
  using NeuronSoma = MyCell;
  using NeuriteElement = MyNeurite;

  BDM_CTPARAM_FOR(bdm, MyCell) {
    using BiologyModules = CTList<RGC_mosaic_BM, Substance_secretion_BM>;
  };

  BDM_CTPARAM_FOR(bdm, MyNeurite) {
    using BiologyModules =
        CTList<RGC_dendrite_growth_BM>;
  };
};

// define my cell creator
template <typename TSimulation = Simulation<>>
static void CellCreator(double min, double max, int num_cells, int cellType) {
  auto* sim = TSimulation::GetActive();
  auto* rm = sim->GetResourceManager();
  auto* random = sim->GetRandom();

  auto* container = rm->template Get<MyCell>();
  container->reserve(num_cells);

  for (int i = 0; i < num_cells; i++) {
    double x = random->Uniform(min + 100, max - 100);
    double y = random->Uniform(min + 100, max - 100);
    double z = random->Uniform(min + 20, 40);  // 24
    std::array<double, 3> position = {x, y, z};

    auto&& cell = rm->template New<MyCell>(position);
    cell.SetDiameter(random->Uniform(7, 8));  // random diameter
    cell.SetCellType(cellType);
    cell.SetInternalClock(0);
    cell.AddBiologyModule(Substance_secretion_BM());
    cell.AddBiologyModule(RGC_mosaic_BM());
  }

  container->Commit();
}  // end CellCreator

// position exporteur
template <typename TSimulation = Simulation<>>
inline void position_exporteur(int i) {
  int seed = TSimulation::GetActive()->GetRandom()->GetSeed();
  auto* param = TSimulation::GetActive()->GetParam();
  ofstream positionFileOn;
  ofstream positionFileOff;
  ofstream positionFileOnOff;
  ofstream positionFileAll;
  stringstream sstmOn;
  stringstream sstmOff;
  stringstream sstmOnOff;
  stringstream sstmAll;
  sstmOn << Concat(param->output_dir_, "/cells_position/on_t") << i << "_seed"
         << seed << ".txt";
  sstmOff << Concat(param->output_dir_, "/cells_position/off_t") << i << "_seed"
          << seed << ".txt";
  sstmOnOff << Concat(param->output_dir_, "/cells_position/onOff_t") << i
            << "_seed" << seed << ".txt";
  sstmAll << Concat(param->output_dir_, "/cells_position/all_t") << i << "_seed"
          << seed << ".txt";

  string fileNameOn = sstmOn.str();
  string fileNameOff = sstmOff.str();
  string fileNameOnOff = sstmOnOff.str();
  string fileNameAll = sstmAll.str();
  positionFileOn.open(fileNameOn);
  positionFileOff.open(fileNameOff);
  positionFileOnOff.open(fileNameOnOff);
  positionFileAll.open(fileNameAll);

  auto* sim = TSimulation::GetActive();
  auto* rm = sim->GetResourceManager();
  auto my_cells = rm->template Get<MyCell>();
  int numberOfCells = my_cells->size();

  for (int cellNum = 0; cellNum < numberOfCells; cellNum++) {
    auto thisCellType = (*my_cells)[cellNum].GetCellType();
    auto position = (*my_cells)[cellNum].GetPosition();
    if (thisCellType == 0) {
      positionFileOn << position[0] << " " << position[1] << " " << position[2]
                     << "\n";
      positionFileAll << position[0] << " " << position[1] << " " << position[2]
                      << " on\n";
    } else if (thisCellType == 1) {
      positionFileOff << position[0] << " " << position[1] << " " << position[2]
                      << "\n";
      positionFileAll << position[0] << " " << position[1] << " " << position[2]
                      << " off\n";
    } else if (thisCellType == 2) {
      positionFileOnOff << position[0] << " " << position[1] << " "
                        << position[2] << "\n";
      positionFileAll << position[0] << " " << position[1] << " " << position[2]
                      << " onoff\n";
    } else {
      positionFileAll << position[0] << " " << position[1] << " " << position[2]
                      << " nd\n";
    }
  }
  positionFileOn.close();
  positionFileOff.close();
  positionFileOnOff.close();
  positionFileAll.close();
}  // end position_exporteur

template <typename TSimulation = Simulation<>>
inline void morpho_exporteur() {
  auto* sim = TSimulation::GetActive();
  auto* rm = sim->GetResourceManager();
  auto* param = sim->GetParam();
  int seed = sim->GetRandom()->GetSeed();
  auto my_cells = rm->template Get<MyCell>();

  for (unsigned int cellNum = 0; cellNum < my_cells->size(); cellNum++) {
    auto&& cell = (*my_cells)[cellNum];
    int thisCellType = cell.GetCellType();
    auto cellPosition = cell.GetPosition();
    ofstream swcFile;
    string swcFileName = Concat(param->output_dir_, "/swc_files/cell", cellNum,
                                "_type", thisCellType, "_seed", seed, ".swc")
                             .c_str();
    swcFile.open(swcFileName);

    cell->SetLabel(1);

    // swcFile << labelSWC_ << " 1 " << cellPosition[0] << " "
    //         << cellPosition[1]  << " " << cellPosition[2] << " "
    //         << cell->GetDiameter()/2 << " -1";
    swcFile << cell->GetLabel() << " 1 0 0 0 " << cell->GetDiameter() / 2
            << " -1";

    for (auto& ne : cell->GetDaughters()) {
      swcFile << swc_neurites(ne, 1, cellPosition);
    }  // end for neurite in cell
    swcFile.close();
  }  // end for cell in simulation
}  // end morpho_exporteur

template <typename T>
inline string swc_neurites(const T ne, int labelParent,
                           array<double, 3> somaPosition) {
  array<double, 3> nePosition = ne->GetPosition();
  nePosition[0] = nePosition[0] - somaPosition[0];
  nePosition[1] = nePosition[1] - somaPosition[1];
  nePosition[2] = nePosition[2] - somaPosition[2];
  string temps;

  ne->GetNeuronSomaOfNeurite()->IncreaseLabel();
  // set explicitly the value of GetLabel() other wise it is not properly set
  int currentLabel = ne->GetNeuronSomaOfNeurite()->GetLabel();

  // if branching point
  if (ne->GetDaughterRight() != nullptr) {
    // FIXME: segment indice should be 5, no 3. If set here,
    // it's not the actual branching point, but the following segment
    // need to run correction.py to correct file
    temps =
        Concat(temps, "\n", currentLabel, " 3 ", nePosition[0], " ",
               nePosition[1], " ", nePosition[2], " ", ne->GetDiameter() / 2,
               " ", labelParent,
               swc_neurites(ne->GetDaughterRight(), currentLabel, somaPosition))
            .c_str();
    ne->GetNeuronSomaOfNeurite()->IncreaseLabel();
  }
  // if is straigh dendrite
  // need to update currentLabel
  currentLabel = ne->GetNeuronSomaOfNeurite()->GetLabel();
  if (ne->GetDaughterLeft() != nullptr) {
    temps =
        Concat(temps, "\n", currentLabel, " 3 ", nePosition[0], " ",
               nePosition[1], " ", nePosition[2], " ", ne->GetDiameter() / 2,
               " ", labelParent,
               swc_neurites(ne->GetDaughterLeft(), currentLabel, somaPosition))
            .c_str();
  }
  // if ending point
  if (ne->GetDaughterLeft() == nullptr && ne->GetDaughterRight() == nullptr) {
    temps = Concat(temps, "\n", currentLabel, " 6 ", nePosition[0], " ",
                   nePosition[1], " ", nePosition[2], " ",
                   ne->GetDiameter() / 2, " ", labelParent)
                .c_str();
  }

  return temps;
}  // end swc_neurites

// RI computation
inline double computeRI(vector<array<double, 3>> coordList) {
  vector<double> shortestDistList;
  for (unsigned int i = 0; i < coordList.size();
       i++) {  // for each cell of same type in the simulation
    array<double, 3> cellPosition = coordList[i];

    double shortestDist = 1e10;
    // for each other cell of same type in the simulation
    for (unsigned int j = 0; j < coordList.size(); j++) {
      array<double, 3> otherCellPosition = coordList[j];

      // get the distance between those 2 cells (x-y plan only)
      double tempsDistance =
          sqrt(pow(cellPosition[0] - otherCellPosition[0], 2) +
               pow(cellPosition[1] - otherCellPosition[1], 2));
      // if cell is closer and is not itself
      if (tempsDistance < shortestDist && tempsDistance != 0) {
        shortestDist = tempsDistance;  // updade closest neighbour distance
      }
    }
    shortestDistList.push_back(
        shortestDist);  // save the shortest distance to neighbour
  }
  // compute mean
  double temps_sum = 0;
  for (unsigned int i = 0; i < shortestDistList.size(); i++) {
    temps_sum += shortestDistList[i];
  }
  double aveShortestDist = temps_sum / (double)shortestDistList.size();
  // compute std
  double temp = 0;
  for (unsigned int i = 0; i < shortestDistList.size(); i++) {
    double val = shortestDistList[i];
    double squrDiffToMean = pow(val - aveShortestDist, 2);
    temp += squrDiffToMean;
  }
  double meanOfDiffs = temp / (double)(shortestDistList.size());
  double std = sqrt(meanOfDiffs);

  return aveShortestDist / std;  // return RI
}  // end computeRI

// get cells coordinate of same cell_type_ to call computeRI
template <typename TSimulation = Simulation<>>
inline double getRI(int desiredCellType) {
  auto* sim = TSimulation::GetActive();
  auto* rm = sim->GetResourceManager();
  auto my_cells = rm->template Get<MyCell>();  // get cell list
  vector<array<double, 3>> coordList;          // list of coordinate
  int numberOfCells = my_cells->size();
  // for each cell in simulation
  for (int cellNum = 0; cellNum < numberOfCells; cellNum++) {
    auto thisCellType = (*my_cells)[cellNum].GetCellType();
    if (thisCellType == desiredCellType) {  // if cell is of the desired type
      auto position = (*my_cells)[cellNum].GetPosition();  // get its position
      coordList.push_back(position);  // put cell coord in the list
    }
  }
  return computeRI(coordList);  // return RI for desired cell type
}  // end getRI

/* -------- simulate -------- */
template <typename TSimulation = Simulation<>>
inline int Simulate(int argc, const char** argv) {
  // number of simulation steps
  int maxStep = 3000;
  // Create an artificial bounds for the simulation space
  int cubeDim = 250;
  int num_cells = 1100;
  double cellDensity = (double)num_cells * 1e6 / (cubeDim * cubeDim);
  cout << "cell density: " << cellDensity << " cells per cm2" << endl;

  // set write output param
  // if you want to write file for RI and cell position
  bool writeRI = false;
  bool writePositionExport = false;
  bool writeSWC = true;
  // create cell position files every outputFrequence steps
  int outputFrequence = 100;

  auto set_param = [&](auto* param) {
    // cell are created with +100 to min and -100 to max
    param->bound_space_ = true;
    param->min_bound_ = 0;
    param->max_bound_ = cubeDim + 200;
    // set min and max length for neurite segments
    param->neurite_min_length_ = 1.0;
    param->neurite_max_length_ = 2.0;
  };

  Simulation<> simulation(argc, argv, set_param);
  auto* rm = simulation.GetResourceManager();
  auto* random = simulation.GetRandom();
  auto* scheduler = simulation.GetScheduler();
  auto* param = simulation.GetParam();

  int mySeed = rand() % 10000;
  mySeed = 9784;  // 9784
  random->SetSeed(mySeed);
  cout << "modelling with seed " << mySeed << endl;

  // min position, max position, number of cells , cell type
  CellCreator(param->min_bound_, param->max_bound_, 0, 0);
  cout << "on cells created" << endl;
  CellCreator(param->min_bound_, param->max_bound_, 0, 1);
  cout << "off cells created" << endl;
  CellCreator(param->min_bound_, param->max_bound_, 0, 2);
  cout << "on-off cells created" << endl;
  CellCreator(param->min_bound_, param->max_bound_, num_cells, -1);
  cout << "undifferentiated cells created" << endl;

  // 3. Define substances
  // Order: substance_name, diffusion_coefficient, decay_constant, resolution
  // if diffusion_coefficient is low, diffusion distance is short
  // if decay_constant is high, diffusion distance is short
  // resolution is number of point in one domaine dimension
  ModelInitializer::DefineSubstance(0, "on_diffusion", 1, 0.5,
                                    param->max_bound_ / 10);
  ModelInitializer::DefineSubstance(1, "off_diffusion", 1, 0.5,
                                    param->max_bound_ / 10);
  ModelInitializer::DefineSubstance(2, "on_off_diffusion", 1, 0.5,
                                    param->max_bound_ / 10);

  // define substances for RGC dendrite attraction
  ModelInitializer::DefineSubstance(3, "on_substance_RGC_guide", 0, 0,
                                    param->max_bound_ / 2);
  ModelInitializer::DefineSubstance(4, "off_substance_RGC_guide", 0, 0,
                                    param->max_bound_ / 2);
  // create substance gaussian distribution for RGC dendrite attraction
  // average peak distance for ON cells: 15.959 with std of 5.297;
  ModelInitializer::InitializeSubstance(3, "on_substance_RGC_guide",
                                        GaussianBand(45, 6, Axis::kZAxis));
  // average peak distance for OFF cells: 40.405 with std of 8.39;
  ModelInitializer::InitializeSubstance(4, "off_substance_RGC_guide",
                                        GaussianBand(69, 8, Axis::kZAxis));
  cout << "substances initialised" << endl;

  // prepare export
  ofstream outputFile;
  // delete previous simulation export
  if (writePositionExport &&
      system(
          Concat("mkdir -p ", param->output_dir_, "/cells_position").c_str())) {
    cout << "error in " << param->output_dir_
         << "/cells_position folder creation" << endl;
  }
  // create RI export file
  if (writeRI) {
    outputFile.open("output/cells_position/RI_" + to_string(mySeed) + ".txt");
  }

  // 4. Run simulation for maxStep timesteps
  for (int i = 0; i <= maxStep; i++) {
    scheduler->Simulate(1);

    if (i % 10 == 0) {  // write RI in file
      double RIon = getRI(0);
      double RIoff = getRI(1);
      double RIonOff = getRI(2);
      if (writeRI) {
        outputFile << RIon << " " << RIoff << " " << RIonOff << "\n";
      }

      // print
      if (i % 100 == 0) {
        // get cell list size
        rm = simulation.GetResourceManager();
        auto my_cells = rm->template Get<MyCell>();
        int numberOfCells = my_cells->size();
        // TODO: vector for unknow number of cell type
        int numberOfCells0 = 0;
        int numberOfCells1 = 0;
        int numberOfCells2 = 0;
        int numberOfDendrites = 0;

        for (int cellNum = 0; cellNum < numberOfCells;
             cellNum++) {  // for each cell in simulation
          numberOfDendrites += (*my_cells)[cellNum].GetDaughters().size();
          auto thisCellType = (*my_cells)[cellNum].GetCellType();
          if (thisCellType == 0) {
            numberOfCells0++;
          } else if (thisCellType == 1) {
            numberOfCells1++;
          } else if (thisCellType == 2) {
            numberOfCells2++;
          }
        }
        cout << "-- step " << i << " out of " << maxStep << " --\n"
             << numberOfCells << " cells in simulation: "
             << (1 - ((double)numberOfCells / num_cells)) * 100
             << "% of cell death\n"
             << numberOfCells0 << " cells are type 0 (on) ; " << numberOfCells1
             << " cells are type 1 (off) ; " << numberOfCells2
             << " cells are type 2 (on-off) ; "
             << (double)(numberOfCells0 + numberOfCells1 + numberOfCells2) /
                    numberOfCells * 100
             << "% got type\n"
             << numberOfDendrites << " apical dendrites in simulation: "
             << numberOfDendrites / numberOfCells << " dendrites per cell\n"
             << "RI on: " << RIon << " ; RI off: " << RIoff
             << " ; RI on-off: " << RIonOff
             << " ; mean: " << (RIon + RIoff + RIonOff) / 3 << endl;
      }  // end every 100 simu steps
    }    // end every 10 simu steps

    if (writePositionExport && i % outputFrequence == 0) {
      // export cell position
      position_exporteur(i);
    }

  }  // end for Simulate

  outputFile.close();

  // remove previous swc files and export neurons morphology
  if (writeSWC &&
      !system(Concat("mkdir -p ", param->output_dir_, "/swc_files").c_str())) {
    if (system(Concat("rm -r ", param->output_dir_, "/swc_files/*").c_str())) {
      cout << "could not delete previous swc files" << endl;
    };
    morpho_exporteur();
  }

  return 0;
} // end Simulate

}  // end namespace bdm

#endif  // RETINAL_DEV_
