#include "nissa.hpp"

using namespace nissa;

void in_main(int narg,char **arg)
{
  if(nranks>1) crash("cannot run in parallel");
  
  if(narg<5) crash("use: %s L T file_in file_out",arg[0]);
  
  int L=atoi(arg[1]);
  int T=atoi(arg[2]);
  
  //Init the MPI grid
  init_grid(T,L);
  
  ///////////////////////////////////////////
  
  spincolor *in[8];
  for(int i=0;i<8;i++)
    {
      in[i]=nissa_malloc("in",loc_vol,spincolor);
      for(int j=0;j<loc_vol*4*3*2;j++) ((double*)(in[i]))[j]=9;
    }
  int i=0;
  ILDG_File fin=ILDG_File_open_for_read(arg[3]);
  do
    {
      ILDG_header head=ILDG_File_get_next_record_header(fin);
      master_printf("%s %lld\n",head.type,head.data_length);
      
      if(strcasecmp(head.type,"scidac-binary-data")==0) read_real_vector(in[i++],fin,head);
      else
	{
	  char *mess=(char*)malloc(head.data_length+1);
	  ILDG_File_read_all(mess,fin,head.data_length);
	  mess[head.data_length]='\0';
	  master_printf("%s\n================================================\n",mess);
	  free(mess);
	}
    }
  while(i<8);
  
  ILDG_File_close(fin);
  
  //print
  FILE *fout=open_file(arg[4],"w");
  for(int iflav=0;iflav<2;iflav++)
    for(int ivol=0;ivol<loc_vol;ivol++)
      for(int id_so=0;id_so<4;id_so++)
	for(int id_si=0;id_si<4;id_si++)
	  for(int ic_si=0;ic_si<3;ic_si++)
	    fprintf(fout,"%d  % 2d % 2d % 2d % 2d  %d %d %d  %+016.016le %+016.016le\n",
		    iflav,
		    glb_coord_of_loclx[ivol][0],
		    glb_coord_of_loclx[ivol][1],
		    glb_coord_of_loclx[ivol][2],
		    glb_coord_of_loclx[ivol][3],
		    id_so,
		    id_si,
		    ic_si,
		    in[id_so*2+iflav][ivol][id_si][ic_si][RE],
		    in[id_so*2+iflav][ivol][id_si][ic_si][IM]);
  
  close_file(fout);
  
  //read_real_vector(in,arg[3],"scidac-binary-data");
  
  for(int i=0;i<8;i++) nissa_free(in[i]);
  
  ///////////////////////////////////////////
}

int main(int narg,char **arg)
{
  init_nissa_threaded(narg,arg,in_main);
  
  close_nissa();
  
  return 0;
}