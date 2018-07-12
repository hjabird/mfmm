#include <omp.h>
#include "args.h"
#include "build_tree.h"
#include "dataset.h"
#include "interaction_list.h"
#include "laplace.h"
#include "precompute.h"

using namespace exafmm_t;
using namespace exafmm;
RealVec plummer(int);
RealVec nonuniform(int);

int main(int argc, char **argv) {
  Args args(argc, argv);
  omp_set_num_threads(args.threads);
  size_t N = args.numBodies;
  NCRIT = args.ncrit;
  MULTIPOLE_ORDER = args.P;
  NSURF = 6*(MULTIPOLE_ORDER-1)*(MULTIPOLE_ORDER-1) + 2;
  Profile::Enable(true);
  RealVec src_coord, src_value;

  Bodies bodies = cube(args.numBodies, 0);
  std::vector<Node*> leafs, nonleafs;
  Nodes nodes = buildTree(bodies, leafs, nonleafs);
  MAXLEVEL = 0;
  for(size_t i=0; i<leafs.size(); i++) {
    MAXLEVEL = std::max(MAXLEVEL, leafs[i]->depth);
  }

  // fill in pt_coord, pt_src, correct coord for compatibility
  // remove this later
  for(int i=0; i<nodes.size(); i++) {
    for(int d=0; d<3; d++) {
      nodes[i].coord[d] = nodes[i].X[d] - nodes[i].R;
    }
    if(nodes[i].IsLeaf()) {
      for(Body* B=nodes[i].body; B<nodes[i].body+nodes[i].numBodies; B++) {
        nodes[i].pt_coord.push_back(B->X[0]);
        nodes[i].pt_coord.push_back(B->X[1]);
        nodes[i].pt_coord.push_back(B->X[2]);
        nodes[i].pt_src.push_back(B->q);
      }
    }
  }
  Profile::Tic("Total", true);
  initRelCoord();    // initialize relative coords
  Profile::Tic("Precomputation", true);
  Precompute();
  Profile::Toc();
  setColleagues(nodes);
  buildList(nodes);
  M2LSetup(M2Ldata, nonleafs);
  upwardPass(nodes, leafs);
  downwardPass(nodes, leafs);
  Profile::Toc();
  RealVec error = verify(leafs);
  std::cout << std::setw(20) << std::left << "Leaf Nodes" << " : "<< leafs.size() << std::endl;
  std::cout << std::setw(20) << std::left << "Tree Depth" << " : "<< leafs.back()->depth << std::endl;
  std::cout << std::setw(20) << std::left << "Potn Error" << " : " << std::scientific << error[0] << std::endl;
  std::cout << std::setw(20) << std::left << "Grad Error" << " : " << std::scientific << error[1] << std::endl;
  Profile::print();
  return 0;
}
