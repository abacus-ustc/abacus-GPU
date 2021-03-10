//=========================================================
//AUTHOR : Mohan 
//DATE : 2009-04-22
//=========================================================
#ifndef MAKE_OVERLAP_TABLE_H
#define MAKE_OVERLAP_TABLE_H

#include "src_pw/tools.h"
#include "src_lcao/numerical_orbital.h"
#include "src_lcao/numerical_orbital_lm.h"
#include "src_lcao/numerical_nonlocal.h"
#include "src_lcao/numerical_nonlocal_lm.h"
#include "src_lcao/make_gaunt_table.h"
#include "src_lcao/center2_orb.h"
#include "src_global/sph_bessel_recursive.h"

#include <set>

class Make_Overlap_Table
{
	public:

	Make_Overlap_Table();
	~Make_Overlap_Table();

	void allocate (
		const int &ntype,
		const int &lmax_in,
		const int &kmesh_in,
		const double &Rmax_in,
		const double &dR_in,
		const double &dk_in);

	void init_Table(const int &job);
	void init_Table_Beta(void);
	void Destroy_Table(void);
	void Destroy_Table_Beta(void);

	// Five dimension:
	// (1) 0: normal (S(R)) ; 1: derivative( dS/dR )
	// (2) pairs type number. 
	// (3) pairs chi.
	// (4) Max angular momentum: L.
	// (5) Distance between atoms: R.
	double***** Table_SR;
	double***** Table_TR;
	double***** Table_NR;

	bool destroy_sr;
	bool destroy_tr;
	bool destroy_nr;
	
	//=================================================
	//make table of Spherical bessel
	//Sph_Bes : jlx[kmesh][Rmesh][L]
	//L should be 2*Lmax, which is max L of all type
	//=================================================
	// Peize Lin update 2016-01-26
	void init_Lmax (const int orb_num, const int mode, int &Lmax_used, int &Lmax) const;
	void init_Table_Spherical_Bessel (const int orb_num, const int mode, int &Lmax_used, int &Lmax);

	Sph_Bessel_Recursive::D2* pSB = nullptr;				// Peize Lin add 2017-04-24, and change all jlx in this class

	//==============================================
	// make the index, in order to get the element 
	// from Table_SR and Table_TR quickly.
	//==============================================

	
	//-------------------------
	// OV stands for 'overlap'
	// T stands for atom type.
	// O stands for orbitals.
	//-------------------------
    void init_OV_Tpair(void);
    void init_OV_Opair(void);
	int OV_nTpairs;
    IntArray OV_Tpair;
    IntArray OV_Opair;
    IntArray OV_L2plus1;

	//-------------------------
	// NL stands for 'nonlocal'
	// T stands for atom type.
	// O stands for orbitals.
	//-------------------------
	void init_NL_Tpair(void);
    void init_NL_Opair(void);
	int NL_nTpairs;
	IntArray NL_Tpair;
	IntArray NL_Opair;
	IntArray NL_L2plus1;

	//========================================================
	// Small function
	//========================================================
	static int get_rmesh( const double &R1, const double &R2);

	static double dr;
	int Rmesh;

	private:

	void cal_ST_Phi12_R(
		const int &job,
		const int &l,
		const Numerical_Orbital_Lm &n1,
		const Numerical_Orbital_Lm &n2,
		const int &rmesh,
		double *rs,
		double *drs) const;
	// Peize Lin add 2017-10-13
	void cal_ST_Phi12_R(
		const int &job,
		const int &l,
		const Numerical_Orbital_Lm &n1,
		const Numerical_Orbital_Lm &n2,
		const set<size_t> &radials,				// only calculate ir in radials
		double *rs,
		double *drs) const;

	void cal_VNL_PhiBeta_R(
        const int &l,
        const Numerical_Orbital_Lm &n1,
        const Numerical_Nonlocal_Lm &n2,
        const int &rmesh,
        double *rs,
		double *drs);

	// variables
    int ntype;
	int lmax;
	double Rmax;
	double dk;
	int nlm;
	int kmesh;
	double *kpoint;
	double *r;
	double *rab;
	double *kab;	

	friend class Center2_Orb::Orb11;			// Peize Lin add 2016-01-24
};
#endif
