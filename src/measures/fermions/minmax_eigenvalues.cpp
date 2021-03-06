#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "dirac_operators/overlap/dirac_operator_overlap.hpp"
#include "eigenvalues/eigenvalues.hpp"
#include "eigenvalues/eigenvalues_overlap.hpp"
#include "geometry/geometry_mix.hpp"
#include "minmax_eigenvalues.hpp"
#include "new_types/rat_approx.hpp"
#include "operations/remez/remez_algorithm.hpp"

#include "dirac_operators/overlap/dirac_operator_overlap_kernel_portable.hpp"
#include "inverters/overlap/cgm_invert_overlap_kernel2.hpp"

///////////////////////////////////////////////
////      C. BONANNO AND M.CARDINALI       ////
///////////////////////////////////////////////

namespace nissa
{
  namespace minmax
  {
    THREADABLE_FUNCTION_4ARG(matrix_element_with_gamma, double*,out, complex*,buffer, spincolor*,x, int,igamma)
    {
      GET_THREAD_ID();
      
      NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
	{
	  spincolor t;
	  unsafe_dirac_prod_spincolor(t,base_gamma+igamma,x[ivol]);
	  spincolor_scalar_prod(buffer[ivol],x[ivol],t);
	}
      NISSA_PARALLEL_LOOP_END;
      THREAD_BARRIER();
      
      complex_vector_glb_collapse(out,buffer,loc_vol);
    }
    THREADABLE_FUNCTION_END
  }
  
  //Computes the participation ratio
  double participation_ratio(spincolor *v)
  {
    GET_THREAD_ID();
    
    double *l=nissa_malloc("l",loc_vol,double);
    
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
      {
	complex t;
	spincolor_scalar_prod(t,v[ivol],v[ivol]);
	l[ivol]=t[RE];
      }
    NISSA_PARALLEL_LOOP_END;
    THREAD_BARRIER();
    
    double s=double_vector_glb_norm2(l,loc_vol);
    double n2=double_vector_glb_norm2(v,loc_vol);
    
    nissa_free(l);
    
    return sqr(n2)/(glb_vol*s);
  }
  
  //measure minmax_eigenvalues
  void measure_minmax_eigenvalues(quad_su3 **conf_eo,theory_pars_t &theory_pars,minmax_eigenvalues_meas_pars_t &meas_pars,int iconf,int conf_created)
  {
    double eig_time=-take_time();
    
    //Parameters of the eigensolver
    FILE *fout=open_file(meas_pars.path,conf_created?"w":"a");
    master_fprintf(fout," # iconf: %d\n",iconf);
    
    //zero smooth time of the conf
    quad_su3 *conf_lx=nissa_malloc("conf_lx",loc_vol+bord_vol,quad_su3);
    paste_eo_parts_into_lx_vector(conf_lx,conf_eo);
    
    //parameters of the measure
    bool min_max=meas_pars.min_max;
    int neigs=meas_pars.neigs;
    double residue=meas_pars.residue;
    int wspace_size=meas_pars.wspace_size;
    double maxerr=sqrt(residue);
    
    //allocate
    complex *eigval=nissa_malloc("eigval",neigs,complex);
    double *eig_res=nissa_malloc("eig_res",neigs,double);
    spincolor **eigvec=nissa_malloc("eigvec",neigs,spincolor*);
    spincolor *temp=nissa_malloc("temp",loc_vol,spincolor);
    complex *buffer=nissa_malloc("buffer",loc_vol,complex);
    for(int ieig=0;ieig<neigs;ieig++)
      eigvec[ieig]=nissa_malloc("eig",loc_vol+bord_vol,spincolor);
    
    //loop on smooth
    int nsmooth=0;
    bool finished;
    do
      {
	verbosity_lv1_master_printf("Measuring minmax_eigenvalues for nsmooth %d/%d\n",nsmooth,meas_pars.smooth_pars.nsmooth());
	
	//plaquette for the current nsmooth
	double plaq=global_plaquette_lx_conf(conf_lx);
	master_fprintf(fout,"  # nsmooth: %d , plaq: %.16lg\n",nsmooth,plaq);
	
	//loop on the quarks
	for(int iquark=0;iquark<theory_pars.nflavs();iquark++)
	  {
	    master_fprintf(fout,"   # iquark: %d\n",iquark);
	    
	    verbosity_lv1_master_printf("Measuring minmax_eigenvalues for quark %d/%d\n",iquark+1,(int)theory_pars.nflavs());
	    
	    double mass=theory_pars.quarks[iquark].mass;
	    double mass_overlap=theory_pars.quarks[iquark].mass_overlap;
	    if(theory_pars.quarks[0].discretiz!=ferm_discretiz::OVERLAP) crash("Implemented only for overlap");
	    
	    //Generate the approximation
	    rat_approx_t appr;
	    generate_rat_approx_for_overlap(conf_lx,&appr,mass_overlap,maxerr);
	    appr.master_fprintf_expr(stdout);
	    verify_rat_approx_for_overlap(conf_lx,appr,mass_overlap,residue);
	    
	    //Find the eigenvalues
	    find_eigenvalues_overlap(eigvec,eigval,neigs,min_max,conf_lx,appr,residue,mass_overlap,mass,wspace_size);
	    
	    //computes the participation ratio and chirality, recompute the eigenvalues and compute the residue
	    for(int ieig=0;ieig<neigs;ieig++)
	      {
		master_fprintf(fout,"   # ieig: %d\n",ieig);
		master_fprintf(fout,"     eigval: ( %.16lg , %.16lg )\n",eigval[ieig][RE],eigval[ieig][IM]);
		
		//eigenvalue check
		complex eigval_check;
		apply_overlap(temp,conf_lx,&appr,residue,mass_overlap,mass,eigvec[ieig]);
		double eigvec_norm2=double_vector_glb_norm2(eigvec[ieig],loc_vol);
		complex_vector_glb_scalar_prod(eigval_check,(complex*)(eigvec[ieig]),(complex*)temp,sizeof(spincolor)/sizeof(complex)*loc_vol);
		complex_prodassign_double(eigval_check,1.0/eigvec_norm2);
		master_fprintf(fout,"     eigval_check: ( %.16lg , %.16lg)\n",eigval_check[RE],eigval_check[IM]);
		
		//residue
		complex_vector_subtassign_complex_vector_prod_complex((complex*)temp,(complex*)(eigvec[ieig]),eigval_check,sizeof(spincolor)/sizeof(complex)*loc_vol);
		eig_res[ieig]=sqrt(double_vector_glb_norm2(temp,loc_vol)/eigvec_norm2);
		master_fprintf(fout,"     residue: %.16lg\n",residue);
		
		//participation ratio
		double pr=participation_ratio(eigvec[ieig]);
		master_fprintf(fout,"     partic_rat: %.16lg\n",pr);
		
		//chirality
		complex chir;
		minmax::matrix_element_with_gamma(chir,buffer,eigvec[ieig],5);
		master_fprintf(fout,"     chirality: %.16lg %.16lg\n",chir[RE],chir[IM]);
		
		master_printf("\n");
	      }
	  }
	
	//proceeds with smoothing
	finished=smooth_lx_conf_until_next_meas(conf_lx,meas_pars.smooth_pars,nsmooth);
      }
    while(not finished);
    
    //close the file
    close_file(fout);
    
    //print elapsed time
    eig_time+=take_time();
    master_printf("Eigenvalues computation time: %lg\n", eig_time);
    
    //free
    nissa_free(conf_lx);
    nissa_free(eigval);
    nissa_free(eig_res);
    nissa_free(temp);
    nissa_free(buffer);
    for(int ieig=0;ieig<neigs;ieig++)
      nissa_free(eigvec[ieig]);
    nissa_free(eigvec);
  }
  
  //print
  std::string minmax_eigenvalues_meas_pars_t::get_str(bool full)
  {
    std::ostringstream os;
    
    os<<"MeasMinMaxEigenval\n";
    os<<base_fermionic_meas_t::get_str(full);
    if(neigs!=def_neigs() or full) os<<" Neigs\t\t=\t"<<neigs<<"\n";
    if(wspace_size!=def_wspace_size() or full) os<<" WSpaceSize\t\t=\t"<<wspace_size<<"\n";
    if(min_max!=def_min_max() or full) os<<" MinMax\t\t=\t"<<min_max<<"\n";
    os<<smooth_pars.get_str(full);
    
    return os.str();
  }
}
