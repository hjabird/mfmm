/******************************************************************************
 *
 * ExaFMM
 * A high-performance fast multipole method library with C++ and
 * python interfaces.
 *
 * Originally Tingyu Wang, Rio Yokota and Lorena A. Barba
 * Modified by HJA Bird
 *
 ******************************************************************************/
#if NON_ADAPTIVE
#include "build_non_adaptive_tree.h"
#else
#include "ExaFMM/build_tree.h"
#endif
#include "ExaFMM/build_list.h"
#include "ExaFMM/dataset.h"
#include "ExaFMM/laplace.h"

using namespace ExaFMM;

int main(int argc, char **argv) {
  Args args(argc, argv);
  print_divider("Parameters");
  args.print();

  omp_set_num_threads(args.threads);

  using real_t = double;
  print_divider("Time");
  Bodies<real_t> sources =
      init_sources<real_t>(args.numBodies, args.distribution, 0);
  Bodies<real_t> targets =
      init_targets<real_t>(args.numBodies, args.distribution, 5);

  start("Total");
  LaplaceFmm fmm(args.P, args.numCrit);
#if NON_ADAPTIVE
  fmm.m_depth = args.maxlevel;
#endif

  start("Build Tree");
  auto [boxCentre, boxHalfSide] =
      get_bounds<LaplaceFmm::potential_t>(sources, targets);
  fmm.m_x0 = boxCentre;
  fmm.m_r0 = boxHalfSide;
  adaptive_tree tree(sources, targets, fmm);
  stop("Build Tree");

  start("Build Lists");
  build_list(tree.nodes(), fmm);
  stop("Build Lists");

  start("Precomputation");
  fmm.precompute();
  stop("Precomputation");

  start("M2L Setup");
  fmm.M2L_setup(tree.nonleaves());
  stop("M2L Setup");
  fmm.upward_pass(tree.nodes(), tree.leaves());
  fmm.downward_pass(tree.nodes(), tree.leaves());
  stop("Total");

#if DEBUG /* check downward check potential at leaf level*/
  for (auto dn_check : leafs[0]->m_downEquiv) {
    std::cout << dn_check << std::endl;
  }
#endif
  stop("Total");
  print("Evaluation Gflop", (float)m_flop / 1e9);

  bool sample = (args.numBodies >= 10000);
  std::vector<real_t> err = fmm.verify(tree.leaves(), sample);
  print_divider("Error");
  print("Potential Error L2", err[0]);
  print("Gradient Error L2", err[1]);

  print_divider("Tree");
  print("Root Center x", fmm.m_x0[0]);
  print("Root Center y", fmm.m_x0[1]);
  print("Root Center z", fmm.m_x0[2]);
  print("Root Radius R", fmm.m_r0);
  print("Tree Depth", fmm.m_depth);
  print("Leaf Nodes", tree.leaves().size());

  return 0;
}
