// Template to invert using c.g.
// macro to be defined:
//  -apply_operator
//  -cg_invert
//  -cg_parameters_proto
//  -cg_inner_parameters_call
//  -size_of_bulk, size_of_bord
//  -basetype
//  -ndoubles_per_site

extern double cg_inv_over_time;
extern int ncg_inv;

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <omp.h>

void cg_invert(basetype *sol,basetype *guess,cg_parameters_proto,int niter,int rniter,double residue,basetype *source)
{
  int riter=0;
  basetype *s=nissa_malloc("s",size_of_bulk,basetype);
  basetype *p=nissa_malloc("p",size_of_bulk+size_of_bord,basetype);
  basetype *r=nissa_malloc("r",size_of_bulk,basetype);

  //macro to be defined externally, allocating all the required additional vectors
  cg_additional_vectors_allocation();
  
  if(guess==NULL) vector_reset(sol);
  else vector_copy(sol,guess);
  
  ncg_inv++;
  cg_inv_over_time-=take_time();
  
  const int each=10;
  
  //external loop, used if the internal exceed the maximal number of iterations
  double source_norm,lambda;
  do
    {
      //calculate p0=r0=DD*sol_0 and delta_0=(p0,p0), performing global reduction and broadcast to all nodes
      apply_operator(s,cg_inner_parameters_call,sol);
      
      double_vector_subt_double_vector_prod_double((double*)r,(double*)source,(double*)s,1,size_of_bulk*ndoubles_per_site);
      double_vector_copy((double*)p,(double*)r,size_of_bulk*ndoubles_per_site);
      source_norm=double_vector_glb_scalar_prod((double*)source,(double*)source,size_of_bulk*ndoubles_per_site);
      double delta=double_vector_glb_scalar_prod((double*)r,(double*)r,size_of_bulk*ndoubles_per_site);
      
      if(riter==0) verbosity_lv2_master_printf("Source norm: %lg\n",source_norm);
      if(source_norm==0 || isnan(source_norm)) crash("invalid norm: %lg",source_norm);
      
      verbosity_lv2_master_printf("iter 0 relative residue: %lg\n",delta/source_norm);
      
      int final_iter;
      
#pragma omp parallel
      {
	//main loop
	int iter=0;
	double alpha,omega,gammag,internal_lambda,internal_delta=delta;
	do
	  {	  
	    //(r_k,r_k)/(p_k*DD*p_k)
#pragma omp single
	    cg_inv_over_time+=take_time();
	    apply_operator(s,cg_inner_parameters_call,p);
#pragma omp single
	    cg_inv_over_time-=take_time();
	    
	    alpha=double_vector_glb_scalar_prod((double*)s,(double*)p,size_of_bulk*ndoubles_per_site);
	    omega=internal_delta/alpha;
	    
	    //sol_(k+1)=x_k+omega*p_k
	    double_vector_summ_double_vector_prod_double((double*)sol,(double*)sol,(double*)p,omega,size_of_bulk*ndoubles_per_site);
	    //r_(k+1)=x_k-omega*p_k
	    double_vector_summ_double_vector_prod_double((double*)r,(double*)r,(double*)s,-omega,size_of_bulk*ndoubles_per_site);
	    //(r_(k+1),r_(k+1))
	    internal_lambda=double_vector_glb_scalar_prod((double*)r,(double*)r,size_of_bulk*ndoubles_per_site);
	    
	    //(r_(k+1),r_(k+1))/(r_k,r_k)
	    gammag=internal_lambda/internal_delta;
	    internal_delta=internal_lambda;
	    
	    //p_(k+1)=r_(k+1)+gammag*p_k
	    double_vector_summ_double_vector_prod_double((double*)p,(double*)r,(double*)p,gammag,size_of_bulk*ndoubles_per_site);
	    
	    final_iter=(++iter);
#pragma omp single
	    if(iter%each==0) verbosity_lv2_master_printf("iter %d relative residue: %lg\n",iter,internal_lambda/source_norm);
	  }
	while(internal_lambda>(residue*source_norm) && iter<niter);
      }
      
      //last calculation of residual, in the case iter>niter
      apply_operator(s,cg_inner_parameters_call,sol);
      double_vector_subt_double_vector_prod_double((double*)r,(double*)source,(double*)s,1,size_of_bulk*ndoubles_per_site);
      lambda=double_vector_glb_scalar_prod((double*)r,(double*)r,size_of_bulk*ndoubles_per_site);
      
      verbosity_lv1_master_printf("\nfinal relative residue (after %d iters): %lg where %lg was required\n",final_iter,lambda/source_norm,residue);
      
      riter++;
    }
  while(lambda>(residue*source_norm) && riter<rniter);
  
  cg_inv_over_time+=take_time();
  
  nissa_free(s);
  nissa_free(p);
  nissa_free(r);
  
  //macro to be defined externally
  cg_additional_vectors_free();
}

#undef basetype
#undef ndoubles_per_site
#undef size_of_bulk
#undef size_of_bord

#undef apply_operator
#undef cg_operator_parameters
#undef cg_invert
#undef cg_additional_vectors_free
#undef cg_additional_vectors_allocation

