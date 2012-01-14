#include "nissa.h"

int main(int narg,char **arg)
{
  //basic mpi initialization
  init_nissa();
  
  if(rank_tot>1)
    {
      fprintf(stderr,"Cannot run in parallel\n");
      MPI_Abort(MPI_COMM_WORLD,1);
    }
  
  if(narg<5 && rank==0)
    {
      fprintf(stderr,"Use: %s L T file_in file_out\n",arg[0]);
      MPI_Abort(MPI_COMM_WORLD,1);
    }

  int L=atoi(arg[1]);
  int T=atoi(arg[2]);

  //Init the MPI grid 
  init_grid(T,L);

  ///////////////////////////////////////////

  quad_su3 *conf=allocate_quad_su3(loc_vol,"conf");
  read_ildg_gauge_conf(conf,arg[3]);  
  
  FILE *fout=fopen(arg[4],"wb");

  fwrite(glb_size,sizeof(int),4,fout);

  double plaq=global_plaquette_lx_conf(conf);
  fwrite(&plaq,sizeof(double),1,fout);
  
  for(size_t t=0;t<T;t++)
    for(size_t z=0;z<L;z++)
      for(size_t y=0;y<L;y++)
        for(size_t x=0;x<L;x++)
	  if((x+y+z+t)%2)
	    {
	      int iw=loclx_of_coord_list(t,x,y,z);
	      for(int mu=0;mu<4;mu++)
		{
		  int iz=loclx_neighdw[iw][mu];
		  fwrite(conf[iw][mu],sizeof(quad_su3),1,fout);
		  fwrite(conf[iz][mu],sizeof(quad_su3),1,fout);
		}
	    }
  
  fclose(fout);
  
  free(conf);
  
  ///////////////////////////////////////////

  close_nissa();

  return 0;
}
