#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include "../base/global_variables.h"
#include "../base/thread_macros.h"
#include "../communicate/borders.h"
#include "../new_types/complex.h"
#include "../new_types/new_types_definitions.h"
#ifdef USE_THREADS
 #include "../routines/thread.h"
#endif

#include "bgq_macros.h"

#define HOP_HEADER(A)				\
  REORDER_BARRIER();				\
  CACHE_PREFETCH(out+A);			\
  BI_SU3_PREFETCH_NEXT(links[A])

#ifdef BGQ
 #define SITE_COPY(base_out,base_in)		\
  {						\
    void *in=base_in,*out=base_out;		\
    DECLARE_REG_BI_COLOR(reg_temp);		\
    BI_COLOR_PREFETCH_NEXT(in);			\
    REG_LOAD_BI_COLOR(reg_temp,in);		\
    STORE_REG_BI_COLOR(out,reg_temp);		\
  }
#else
 #define SITE_COPY(out,in) BI_COLOR_COPY(out,in)
#endif

THREADABLE_FUNCTION_5ARG(apply_staggered_hopping_matrix_eo_or_oe_bgq_nocomm_nobarrier, bi_oct_su3**,conf, int,istart, int,iend, bi_color*,in, int,eo_or_oe)
{
  GET_THREAD_ID();
  
  bi_color *out=(bi_color*)nissa_send_buf;
  
  NISSA_PARALLEL_LOOP(ibgqlx,istart,iend)
    {
      //take short access to link and output indexing
      int *iout=vireo_hopping_matrix_output_pos[eo_or_oe].inter_fr_in_pos+ibgqlx*8;
      bi_su3 *links=(bi_su3*)(conf[eo_or_oe]+ibgqlx);
      
      //declare
      DECLARE_REG_BI_COLOR(reg_in);
      
      //load in
      REG_LOAD_BI_COLOR(reg_in,in[ibgqlx]);
      
      HOP_HEADER(0); //T backward scatter (forward derivative)
      REG_BI_SU3_PROD_BI_COLOR_LOAD_STORE(out[iout[0]],links[0],reg_in);
      HOP_HEADER(1); //X backward scatter (forward derivative)
      REG_BI_SU3_PROD_BI_COLOR_LOAD_STORE(out[iout[1]],links[1],reg_in);
      HOP_HEADER(2); //Y backward scatter (forward derivative)
      REG_BI_SU3_PROD_BI_COLOR_LOAD_STORE(out[iout[2]],links[2],reg_in);
      HOP_HEADER(3); //Z backward scatter (forward derivative)
      REG_BI_SU3_PROD_BI_COLOR_LOAD_STORE(out[iout[3]],links[3],reg_in);
      
      HOP_HEADER(4); //T forward scatter (backward derivative)
      REG_BI_SU3_DAG_PROD_BI_COLOR_LOAD_STORE(out[iout[4]],links[4],reg_in);
      HOP_HEADER(5);//X forward scatter (backward derivative)
      REG_BI_SU3_DAG_PROD_BI_COLOR_LOAD_STORE(out[iout[5]],links[5],reg_in);
      HOP_HEADER(6); //Y forward scatter (backward derivative)
      REG_BI_SU3_DAG_PROD_BI_COLOR_LOAD_STORE(out[iout[6]],links[6],reg_in);
      HOP_HEADER(7); //Z forward scatter (backward derivative)
      REG_BI_SU3_DAG_PROD_BI_COLOR_LOAD_STORE(out[iout[7]],links[7],reg_in);
    }
}}

//swap border between VN and, if virtual parallelized dir is really parallelized, fill send buffers
THREADABLE_FUNCTION_0ARG(bgq_staggered_hopping_matrix_eo_or_oe_vdir_VN_comm_and_buff_fill)
{
  GET_THREAD_ID();
  
  //vdir buffers might have been filled in another order
  THREAD_BARRIER();
  
  //form two thread team
  FORM_TWO_THREAD_TEAMS();
  
  //short access
  int v=nissa_vnode_paral_dir;
  const int fact=2;
  bi_color *bgq_hopping_matrix_output_data=(bi_color*)nissa_send_buf+bord_volh/fact; //half vol bisp = vol sp 
  bi_color *bgq_hopping_matrix_output_vdir_buffer=bgq_hopping_matrix_output_data+8*loc_volh/fact;
  
  ///////////////////////// bw scattered v derivative (fw derivative)  ////////////////////////
  
  if(is_in_first_team)
    {
      if(paral_dir[v])
	{
	  //split bw v border: VN 0 goes to bw out border (first half)
	  NISSA_CHUNK_LOOP(base_isrc,0,vbord_vol/4/fact,thread_in_team_id,nthreads_in_team)
	    {
	      //the source starts at the middle of result border buffer
	      int isrc=2*base_isrc+0*vbord_vol/2/fact; //we match 2 sites
	      //non-local shuffling: must enter bw buffer for direction v
	      int idst_buf=(0*bord_volh/2+bord_offset[v]/2)/fact+base_isrc;
	      //load the first
	      DECLARE_REG_BI_COLOR(in0);
	      REG_LOAD_BI_COLOR(in0,bgq_hopping_matrix_output_vdir_buffer[isrc]);
	      //load the second
	      DECLARE_REG_BI_COLOR(in1);
	      REG_LOAD_BI_COLOR(in1,bgq_hopping_matrix_output_vdir_buffer[isrc+1]);
	      //merge the two and save
	      DECLARE_REG_BI_COLOR(to_buf);
	      REG_BI_COLOR_V0_MERGE(to_buf,in0,in1);
	      STORE_REG_BI_COLOR(((bi_color*)nissa_send_buf)[idst_buf],to_buf);
	    }
	}
      else
	//we have only to transpose between VN, and no real communication happens
	NISSA_CHUNK_LOOP(isrc,0,vbord_vol/2/fact,thread_in_team_id,nthreads_in_team)
	  {
	    int idst=8*virlx_of_loclx[loclx_neighdw[isrc+vnode_lx_offset][v]]+0+v;
	    BI_COLOR_TRANSPOSE(bgq_hopping_matrix_output_data[idst],bgq_hopping_matrix_output_vdir_buffer[isrc]);
	  }
    }
  
  ///////////////////////// fw scattered v derivative (bw derivative)  ////////////////////////
  
  if(is_in_second_team)
    {
      if(paral_dir[v])
	{
	  //split fw v border: VN 1 goes to fw out border (second half)
	  NISSA_CHUNK_LOOP(base_isrc,0,vbord_vol/4/fact,thread_in_team_id,nthreads_in_team)
	    {
	      //the source starts at the middle of result border buffer
	      int isrc=2*base_isrc+1*vbord_vol/2/fact;
	      //non-local shuffling: must enter fw buffer (starting at bord_volh/2 because its bi) for direction 0
	      int idst_buf=(1*bord_volh/2+bord_offset[v]/2)/fact+base_isrc;
	      //load the first
	      DECLARE_REG_BI_COLOR(in0);
	      REG_LOAD_BI_COLOR(in0,bgq_hopping_matrix_output_vdir_buffer[isrc]);
	      //load the second
	      DECLARE_REG_BI_COLOR(in1);
	      REG_LOAD_BI_COLOR(in1,bgq_hopping_matrix_output_vdir_buffer[isrc+1]);
	      //merge the two and save
	      DECLARE_REG_BI_COLOR(to_buf);
	      REG_BI_COLOR_V1_MERGE(to_buf,in0,in1);
	      STORE_REG_BI_COLOR(((bi_color*)nissa_send_buf)[idst_buf],to_buf);
	    }
	}
      else
	//we have only to transpose between VN
	NISSA_CHUNK_LOOP(base_isrc,0,vbord_vol/2/fact,thread_in_team_id,nthreads_in_team)
	  {
	    //the source starts at the middle of result border buffer
	    int isrc=base_isrc+vbord_vol/2/fact;
	    int idst=8*(vnode_lx_offset+base_isrc)+4+v;
	    BI_COLOR_TRANSPOSE(bgq_hopping_matrix_output_data[idst],bgq_hopping_matrix_output_vdir_buffer[isrc]);
	  }
    }
}}

//perform communications between VN and start all the communications between nodes
THREADABLE_FUNCTION_0ARG(start_staggered_hopping_matrix_eo_or_oe_bgq_communications)
{
  //shuffle data between virtual nodes and fill vdir out buffer
  bgq_staggered_hopping_matrix_eo_or_oe_vdir_VN_comm_and_buff_fill();
  
  //after the barrier, all buffers are filled and communications can start
  THREAD_BARRIER();
  
  //start communications of scattered data to other nodes
  comm_start(eo_color_comm);
}}

//finish the communications and put in place the communicated data
THREADABLE_FUNCTION_1ARG(finish_staggered_hopping_matrix_eo_or_oe_bgq_communications, int,eo_or_oe)
{
  GET_THREAD_ID();
  FORM_TWO_THREAD_TEAMS();
  
  //short access
  int v=nissa_vnode_paral_dir;
  const int fact=2;
  int *def_pos=vireo_hopping_matrix_output_pos[eo_or_oe].inter_fr_recv_pos;
  
  //wait communications end
  comm_wait(eo_color_comm);
  
  //vdir bw border (bw derivative): data goes to VN 0
  if(is_in_first_team)
    {
      //inside incoming borders vdir is ordered naturally, while in the output data it comes first
      bi_color *base_out=(bi_color*)nissa_send_buf+(bord_volh+0*8*bord_dir_vol[v])/fact;
      bi_color *base_vdir_in=(bi_color*)nissa_send_buf+(bord_volh+8*loc_volh+1*vbord_vol/2)/fact;
      bi_color *base_bord_in=(bi_color*)nissa_recv_buf+bord_offset[v]/2/fact;
      
      NISSA_CHUNK_LOOP(isrc,0,bord_dir_vol[v]/2/fact,thread_in_team_id,nthreads_in_team)
	{
	  //VN=0 must be filled with border
	  DECLARE_REG_BI_COLOR(in0);
	  REG_LOAD_BI_COLOR(in0,base_bord_in[isrc]);
	  //VN=1 with buf0
	  DECLARE_REG_BI_COLOR(in1);
	  REG_LOAD_BI_COLOR(in1,base_vdir_in[2*isrc]);
	  //merge and save
	  DECLARE_REG_BI_COLOR(to_dest);
	  REG_BI_COLOR_V0_MERGE(to_dest,in0,in1);
	  STORE_REG_BI_COLOR(base_out[2*isrc*8+4+v],to_dest);
	  
	  //VN=1 with buf1
	  REG_LOAD_BI_COLOR(in1,base_vdir_in[2*isrc+1]);
	  //merge and save
	  REG_BI_COLOR_V10_MERGE(to_dest,in0,in1);
	  STORE_REG_BI_COLOR(base_out[(2*isrc+1)*8+4+v],to_dest);
	}
    }
  
  //other 3 bw borders
  if(is_in_first_team)
    for(int imu=0;imu<3;imu++)
      {
	int mu=perp_dir[v][imu];
	bi_color *base_out=(bi_color*)nissa_send_buf+bord_volh/fact;
	bi_color *base_in=(bi_color*)nissa_recv_buf;
	NISSA_CHUNK_LOOP(isrc,bord_offset[mu]/2/fact,(bord_offset[mu]/2+bord_dir_vol[mu]/2)/fact,
			 thread_in_team_id,nthreads_in_team)
	  SITE_COPY(base_out[def_pos[isrc]],base_in[isrc]);
      }
  
  //v fw border (fw derivative): data goes to VN 1
  if(is_in_second_team)
    {
      //inside incoming borders vdir is ordered naturally, while in the output data it comes first
      bi_color *base_out=(bi_color*)nissa_send_buf+(bord_volh+1*8*bord_dir_vol[v])/fact;
      bi_color *base_vdir_in=(bi_color*)nissa_send_buf+(bord_volh+8*loc_volh+0*vbord_vol/2)/fact;
      bi_color *base_bord_in=(bi_color*)nissa_recv_buf+(bord_vol/4+bord_offset[v]/2)/fact;
      
      NISSA_CHUNK_LOOP(isrc,0,bord_dir_vol[v]/2/fact,thread_in_team_id,nthreads_in_team)
	{
	  //VN=0 with buf1
	  DECLARE_REG_BI_COLOR(in0);
	  REG_LOAD_BI_COLOR(in0,base_vdir_in[2*isrc]);
	  //VN=1 must be filled with border 0
	  DECLARE_REG_BI_COLOR(in1);
	  REG_LOAD_BI_COLOR(in1,base_bord_in[isrc]);
	  //merge and save
	  DECLARE_REG_BI_COLOR(to_dest);
	  REG_BI_COLOR_V10_MERGE(to_dest,in0,in1);
	  STORE_REG_BI_COLOR(base_out[2*isrc*8+0+v],to_dest);
	  
	  //VN=0 with buf1
	  REG_LOAD_BI_COLOR(in0,base_vdir_in[2*isrc+1]);
	  //merge and save
	  REG_BI_COLOR_V1_MERGE(to_dest,in0,in1);
	  STORE_REG_BI_COLOR(base_out[(2*isrc+1)*8+0+v],to_dest);
	}
    }
  
  //other 3 fw borders
  if(is_in_second_team)
    for(int imu=0;imu<3;imu++)
      {
	int mu=perp_dir[v][imu];
	bi_color *base_out=(bi_color*)nissa_send_buf+bord_volh/fact;
	bi_color *base_in=(bi_color*)nissa_recv_buf;
	NISSA_CHUNK_LOOP(isrc,(bord_vol/4+bord_offset[mu]/2)/fact,(bord_vol/4+bord_offset[mu]/2+bord_dir_vol[mu]/2)/fact,
			 thread_in_team_id,nthreads_in_team)
	  SITE_COPY(base_out[def_pos[isrc]],base_in[isrc]);
      }
}}
