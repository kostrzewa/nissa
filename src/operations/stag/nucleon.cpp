#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "linalgs/linalgs.hpp"
#include "new_types/su3.hpp"
#include "routines/mpi_routines.hpp"
#include "operations/gauge_fixing.hpp"

#include "stag.hpp"

#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

#include "nucleon.hpp"

namespace nissa
{
  THREADABLE_FUNCTION_5ARG(measure_nucleon_corr, quad_su3**,conf, theory_pars_t,theory_pars, nucleon_corr_meas_pars_t,meas_pars, int,iconf, int,conf_created)
  {
    GET_THREAD_ID();
    const int eps_i[6][3]={{0,1,2},{1,2,0},{2,0,1},{0,2,1},{2,1,0},{1,0,2}};
    const int eps_s[6]={+1,+1,+1,-1,-1,-1};
    
    CRASH_IF_NOT_3COL();
    FILE *file=open_file(meas_pars.path,conf_created?"w":"a");
    
    int nflavs=theory_pars.nflavs;
    
    //allocate source
    su3 *source[2]={nissa_malloc("source_e",loc_volh+bord_volh,su3),nissa_malloc("source_o",loc_volh+bord_volh,su3)};
    color *temp_source[2]={nissa_malloc("temp_source_e",loc_volh+bord_volh,color),nissa_malloc("temp_source_o",loc_volh+bord_volh,color)};
    color *temp_sol[2]={nissa_malloc("temp_sol_e",loc_volh+bord_volh,color),nissa_malloc("temp_sol_o",loc_volh+bord_volh,color)};
    
    //allocate propagators
    su3 *prop[nflavs][2];
    for(int iflav=0;iflav<nflavs;iflav++)
	for(int EO=0;EO<2;EO++)
	  prop[iflav][EO]=nissa_malloc("prop",loc_volh+bord_volh,su3);
    //perform_random_gauge_transform(conf,conf);
    //allocate local and global contraction
    int ncompl=glb_size[0]*nflavs*(nflavs+1)*(nflavs+2)/6;
    complex *glb_contr=nissa_malloc("glb_contr",ncompl,complex);
    complex *loc_contr=new complex[ncompl];
    vector_reset(glb_contr);
    memset(loc_contr,0,sizeof(complex)*ncompl);
    
    //loop over the hits
    int nhits=meas_pars.nhits;
    for(int hit=0;hit<nhits;hit++)
      {
	verbosity_lv2_master_printf("Evaluating nucleon correlator, hit %d/%d\n",hit+1,nhits);
	
	//generate the source on an even site
	coords source_coord;
	generate_random_coord(source_coord);
	for(int mu=0;mu<4;mu++) source_coord[mu]=0;//(source_coord[mu]/2)*2;
	master_printf("Coord[0]: %d\n",source_coord[0]);
	generate_delta_eo_source(source,source_coord);
	
	//compute M^-1
	for(int ic=0;ic<NCOL;ic++)
	  {
	    get_color_from_su3(temp_source,source,ic);
	    for(int iflav=0;iflav<nflavs;iflav++)
	      {
		mult_Minv(temp_sol,conf,&theory_pars,iflav,meas_pars.residue,temp_source);
		
		//put the anti-periodic condition on the propagator
		for(int eo=0;eo<2;eo++)
		  NISSA_PARALLEL_LOOP(ieo,0,loc_volh)
		    {
		      //color_prod_double(temp_sol[eo][ieo],temp_sol[eo][ieo],(glb_coord_of_loclx[loclx_of_loceo[eo][ieo]][0]>=source_coord[0])?+1:-1);
		      put_color_into_su3(prop[iflav][eo][ieo],temp_sol[eo][ieo],ic);
		  }
	      }
	  }
	
	//contract
	int icombo=0;
	int ifl[NCOL];
	for(ifl[0]=0;ifl[0]<nflavs;ifl[0]++)
	  for(ifl[1]=0;ifl[1]<=ifl[0];ifl[1]++)
	    for(ifl[2]=0;ifl[2]<=ifl[1];ifl[2]++)
	      {
		for(int eo=0;eo<1;eo++) //nb!
		  NISSA_PARALLEL_LOOP(ieo,0,loc_volh)
		    {
		      //find t
		      int ilx=loclx_of_loceo[eo][ieo];
		      int t=(glb_coord_of_loclx[ilx][0]+glb_size[0]-source_coord[0])%glb_size[0];
		      
		      for(int soeps=0;soeps<6;soeps++)
			for(int sieps=0;sieps<6;sieps++)
			  {
			    complex temp;
			    const int *soc=eps_i[soeps];
			    const int *sic=eps_i[sieps];
			    unsafe_complex_prod(temp,prop[ifl[0]][eo][ieo][sic[0]][soc[0]],prop[ifl[1]][eo][ieo][sic[1]][soc[1]]);
			    complex_prodassign_double(temp,eps_s[soeps]*eps_s[sieps]);
			    complex_summ_the_prod(loc_contr[icombo*glb_size[0]+t],temp,prop[ifl[2]][eo][ieo][sic[2]][soc[2]]);
			  }
		    }
		icombo++;
	      }
      }
    
    //reduce
    glb_threads_reduce_double_vect((double*)loc_contr,2*ncompl);
    if(IS_MASTER_THREAD) glb_nodes_reduce_complex_vect(glb_contr,loc_contr,ncompl);
    
    //print
    double norm=nhits;
    int icombo=0;
    for(int iflav=0;iflav<nflavs;iflav++)
      for(int jflav=0;jflav<=iflav;jflav++)
	for(int kflav=0;kflav<=jflav;kflav++)
	  {
	    master_fprintf(file," # conf %d ; flv1 = %d , m1 = %lg ; flv2 = %d , m2 = %lg ; flv3 = %d , m3 = %lg\n",
			   iconf,iflav,theory_pars.quark_content[iflav].mass,jflav,theory_pars.quark_content[jflav].mass,kflav,theory_pars.quark_content[kflav].mass);
	  
	  for(int t=0;t<glb_size[0];t++)
	    master_fprintf(file,"%d %+016.16lg\n",t,glb_contr[icombo*glb_size[0]+t][RE]/norm);
	  icombo++;
	}
    
    //free everything
    for(int EO=0;EO<2;EO++)
      {    
	for(int iflav=0;iflav<nflavs;iflav++)
	  nissa_free(prop[iflav][EO]);
	nissa_free(source[EO]);
	nissa_free(temp_sol[EO]);
	nissa_free(temp_source[EO]);
      }
    delete[] loc_contr;
    nissa_free(glb_contr);
    
    close_file(file);
  }
  THREADABLE_FUNCTION_END
}
