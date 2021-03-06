#ifndef _CG_128_INVERT_TMCLOVQ2_BGQ_HPP
#define _CG_128_INVERT_TMCLOVQ2_BGQ_HPP

#include "new_types/su3.hpp"

namespace nissa
{
  void inv_tmclovQ2_cg_128_bgq(vir_spincolor *sol,vir_spincolor *guess,vir_oct_su3 *conf,double kappa,vir_clover_term_t *Cl,double mass,int niter,double external_solver_residue,vir_spincolor *external_source);
  void inv_tmclovQ2_m2_cg_128_bgq(vir_spincolor *sol,vir_spincolor *guess,vir_oct_su3 *conf,double kappa,vir_clover_term_t *Cl,double m2,int niter,double external_solver_residue,vir_spincolor *external_source);
}

#endif
