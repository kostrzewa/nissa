#ifndef _METADYNAMICS_HPP
#define _METADYNAMICS_HPP

#include <math.h>
#include <string>
#include <vector>

#include "new_types_definitions.hpp"
#include "base/debug.hpp"

namespace nissa
{
  struct meta_pars_t
  {
    int after;
    int each;
    double coeff;
    double width;
    double barr;
    double force_out;
    double well_tempering;
    double bend;
    
    int ngrid;
    storable_vector_t<double> grid;
    
    void update(int isweep,double Q);
    double compute_pot_der(double x);
    double compute_pot(double x);
    void save(const char *path);
    void load(const char *path);
    void draw_force(const char *force_path);
    void init();
    void read_pars();
  };
}

#endif