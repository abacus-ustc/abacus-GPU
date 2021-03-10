//==========================================================
// AUTHOR : Lixin He, mohan
// DATE : 2008-11-21
//==========================================================

#include "tools.h"
#include "global.h"
#include "energy.h"
#include "algorithms.h"
#include "mymath.h"
//#include "../src_develop/src_siao/selinv.h"
#include "../src_lcao/Mulliken_Charge.h"
#include "../src_lcao/lcao_nnr.h"
//#include "../src_lcao/Mulliken_Charge.h"
#include  "../src_lcao/use_overlap_matrix.h"    
#include "../src_lcao/sltk_atom_arrange.h"//qifeng-2019-01-21
#include "../src_lcao/local_orbital_charge.h"

#include "../src_pw/global.h"
#include "../src_pw/wavefunc.h"
#include "../src_lcao/lcao_matrix.h"


#include "../src_lcao/global_fp.h"
#include "../src_lcao/wfc_dm_2d.h"
#include "../src_global/lapack_connector.h"
#include "../src_global/scalapack_connector.h"
#include "../src_global/matrix.h"
#include "../src_global/complexmatrix.h"
#include <vector>
#include <mpi.h>
#include<sys/time.h>
#include "../src_pw/tools.h"
int energy::mxr = 50;

energy::energy()
{
    // max, used in ewald , mohan add 2009-06-05
    // the maximum number of R vectors included in r
    // the square of the electron charge (Ry atomic units)
    this->etot   = 0;          // the total energy of the solid
	this->etot_harris = 0;	   // total energy of harris functional
    this->eband  = 0;          // the band energy
    this->deband = 0;          // correction for variational energy
	this->deband_harris = 0;   // harris energy
    this->ehart  = 0;          // the hartree energy
    this->etxc   = 0;          // the exchange and correlation energy
    this->vtxc   = 0;          // another exchange-correlation energy
    this->etxcc  = 0;          // the nlcc exchange and correlation
	this->exx    = 0;          // the exact exchange energy.

    this->ewld   = 0;          // the ewald energy
    this->demet  = 0;          // correction for metals
    this->ef     = 0;          // the fermi energy
}

energy::~energy()
{
}

#include "efield.h"

#ifndef __EPM
void energy::calculate_harris(const int &flag)
{
//	TITLE("energy","calculate_harris");
	
	if(flag==1)
	{
		this->deband_harris = this->delta_e();
	}
	else if(flag==2)
	{
		this->etot_harris = eband + deband_harris + (etxc - etxcc) + ewld 
		+ ehart + demet + exx + Efield::etotefield;
	}
	
	return;
}
#endif

void energy::calculate_etot(void)
{
	TITLE("energy","calculate_etot");
	//cout << "\n demet in etot = " << demet << endl;
	this->etot = eband + deband + (etxc - etxcc) + ewld +
	ehart + demet + descf + exx + Efield::etotefield;

	if(vdwd2.vdwD2) 									//Peize Lin add 2014-04-03, update 2019-04-26
	{
		this->etot += vdwd2.energy_result;
	}
	if(vdwd3.vdwD3) 									//jiyy add 2019-05-18
	{
		this->etot += vdwd3.energy_result;
	}																			  
	
/*
	cout << resetiosflags(ios::scientific) << endl;
	cout << setprecision(6) << endl;
	cout << " eband=" << eband << endl;
	cout << " deband=" << deband << endl;
	cout << " etxc-etxcc=" << etxc-etxcc << endl;
	cout << " ewld=" << ewld << endl;
	cout << " ehart=" << ehart << endl;
	cout << " demet=" << demet << endl;
	cout << " descf=" << descf << endl;
	cout << " exx=" << exx << endl;
	cout << " efiled=" << Efield::etotefield << endl;
	*/
}

void energy::print_etot(const bool converged, const int &istep, const int &iter_in, 
const double &dr2, const double &duration, const double &ethr, const double &avg_iter,
bool print)
{
	TITLE("energy","print_etot");
	this->iter = iter_in;

	ofs_running << setprecision(12);
	ofs_running << setiosflags(ios::left);

	ofs_running << "\n Density error is " << dr2 << endl;

 if(OUT_LEVEL != "m") //xiaohui add "OUT_LEVEL", 2015-09-16
 {
	//if(!LOCAL_BASIS)OUT(ofs_running,"Error Threshold",ethr); xiaohui modify 2013-09-02
	if(BASIS_TYPE=="pw")OUT(ofs_running,"Error Threshold",ethr); //xiaohui add 2013-09-02

	if( this->printe>0 && ( (iter+1) % this->printe == 0 || converged || iter == NITER) )	
	{
		ofs_running << "\n " << setw(12) << "Energy" << setw(30) << "Rydberg" << setw(30) << "eV" << endl;
		this->print_format("E_KohnSham",etot);
		this->print_format("E_Harris",etot_harris);
		this->print_format("E_band",eband);
		this->print_format("E_one_elec",eband+deband);
		this->print_format("E_Hartree",ehart);
		this->print_format("E_xc",etxc-etxcc);
		this->print_format("E_Ewald",ewld);
		this->print_format("E_demet",demet); //mohan add 2011-12-02
		this->print_format("E_descf",descf);
		this->print_format("E_efield",Efield::etotefield);
		if(vdwd2.vdwD2)					//Peize Lin add 2014-04, update 2019-04-26
		{
			this->print_format("E_vdwD2",vdwd2.energy_result);
		}
        if(vdwd3.vdwD3)					//jiyy add 2019-05
		{
			this->print_format("E_vdwD3",vdwd3.energy_result);
		}
//		this->print_format("e_exx",exx);
	}
	else
	{
		ofs_running << "\n " << setw(12) << "Energy" << setw(30) << "Rydberg" << setw(30) << "eV" << endl;
		this->print_format("E_KohnSham",etot);
		this->print_format("E_Harris",etot_harris);
	}

	if(TWO_EFERMI)
	{
		this->print_format("E_Fermi_up",ef_up);
		this->print_format("E_Fermi_dw",ef_dw);
	}
	else
	{
		this->print_format("E_Fermi",this->ef);
	}
 }//xiaohui add "OUT_LEVEL", 2015-09-16

        if (iter_in == 1)   // pengfei Li added 2015-1-31
        {
            this->etot_old = this->etot;
        }
	
	// for test, output eigenvalues.
	/*
	for(int ik=0; ik<kv.nks; ik++)
	{
		for(int ib=0; ib<NBANDS; ib++)
		{
			if(ib%8==0)
			{
				ofs_running << endl;
			}
			ofs_running << setw(15) << wf.et(ik,ib) * Ry_to_ev;
		}
	}
	*/

	// mohan update 2011-02-26
	stringstream ss;
	//xiaohui modify 2013-09-02
	//if(LINEAR_SCALING == 2)
	//{
	//	if(DIAGO_TYPE=="canonical")
	//	{
	//		ss << "CA" << iter << "\0";
	//	}
	//	else if(DIAGO_TYPE=="trace_correcting")
	//	{
	//		ss << "TC" << iter << "\0";

	//	}
	//	else if(DIAGO_TYPE=="trace_resetting")
	//	{
	//		ss << "TR" << iter << "\0";
	//	}
	//}
	//else
	//{
	//	if(DIAGO_TYPE=="cg")
	//	{
	//		ss << "CG" << iter << "\0";
	//	}
	//	else if(DIAGO_TYPE=="lapack")
	//	{
	//		ss << "LA" << iter << "\0";
	//	}
	//	else if(DIAGO_TYPE=="hpseps")
	//	{
	//		ss << "HP" << iter << "\0";
	//	}
	//	//else if(DIAGO_TYPE=="selinv")
	//	//{
	//		// selinv series version
	//	//	ss << "SI" << iter << "\0";
	//	//}
	//	else if(DIAGO_TYPE=="dav")
	//	{
	//		ss << "DA" << iter << "\0";
	//	}
	//	else
	//	{
	//		WARNING_QUIT("Energy","print_etot");
	//	}
	//} xiaohui modify 2013-09-02

	//xiaohui add 2013-09-02
	if(KS_SOLVER=="cg")
	{
		ss << "CG" << iter << "\0";
	}
	else if (KS_SOLVER=="lapack")
	{
		ss << "LA" << iter << "\0";
	}
    else if(KS_SOLVER=="hpseps")
	{
		ss << "HP" << iter << "\0";
	}
    else if(KS_SOLVER=="genelpa")
    {
        ss << "GE" << iter << "\0";
    }
	else if(KS_SOLVER=="dav")
	{
		ss << "DA" << iter << "\0";
	}
	else
	{
		WARNING_QUIT("Energy","print_etot");
	} //xiaohui add 2013-09-02

	bool scientific=true;
	int prec = 6;


	if(!print) return;

	if(OUT_LEVEL=="ie" || OUT_LEVEL=="m") //xiaohui add 'm' option, 2015-09-16
	{
		cout << " " << setw(7) << ss.str();
		//cout << setiosflags(ios::fixed);
		//cout << setiosflags(ios::showpos);
		if(scientific)
		{
			cout << setiosflags(ios::scientific);
		}

		if(COLOUR)
		{
			if(MY_RANK==0)
			{
				//printf( "\e[36m%-15f\e[0m", en.etot);	
				printf( "[36m%-15f[0m", en.etot);	
				if(NSPIN==2)
				{
					cout << setprecision(2);
					cout<<setw(10)<<mag.tot_magnetization;
					cout<<setw(10)<<mag.abs_magnetization;
				}
				else if(NONCOLIN && (DOMAG||DOMAG_Z))
				{
					cout << setprecision(2);
					cout<<setw(10)<<mag.tot_magnetization_nc[0]<<setw(10)<<mag.tot_magnetization_nc[1]<<setw(10)<<mag.tot_magnetization_nc[2];
					cout<<setw(10)<<mag.abs_magnetization;
				}
				if(dr2>1.0)
				{
					// 31 is red
					//printf( "\e[31m%-14e\e[0m", dr2);
					printf( "[31m%-14e[0m", dr2);
				}
				else
				{
					// 32 is green
					//printf( "\e[32m%-14e\e[0m", dr2);
					printf( "[32m%-14e[0m", dr2);
				}
				// 34 is blue
				//printf( "\e[36m%-15f\e[0m", en.etot*Ry_to_eV);	
				printf( "[36m%-15f[0m", en.etot*Ry_to_eV);	
				cout << setprecision(3);
	//			cout << setw(11) << en.eband;
	//			cout << setw(11) << en.ehart;
	//			cout << setw(11) << en.etxc - en.etxcc;
				cout << resetiosflags(ios::scientific);
				//if(DIAGO_TYPE=="cg") xiaohui modify 2013-09-02
				if(KS_SOLVER=="cg") //xiaohui add 2013-09-02
				{
					cout << setw(11) << avg_iter;
				}
				//xiaohui modified 2013-03-23
				//else if(DIAGO_TYPE=="selinv")
				//{
					// because Selinv::iter starts from 0.
				//	cout << setw(11) << Selinv::iter;
				//}
				cout << setw(11) << duration;
				cout << endl;
			}
		}
		else
		{
			cout << setprecision(prec);
			//cout << setw(15) << en.etot;
			if(NSPIN==2)
			{
				cout << setprecision(2);
				cout<<setw(10)<<mag.tot_magnetization;
				cout<<setw(10)<<mag.abs_magnetization;
			}
			cout << setprecision(6);
			cout << setw(15) << en.etot*Ry_to_eV;
                        cout << setw(15) << (en.etot - en.etot_old) *Ry_to_eV;  //pengfei Li added 2015-1-31
                        cout << setprecision(3);
                        cout << setw(11) << dr2;
			cout << setprecision(3);
	//		cout << setw(11) << en.eband;
	//		cout << setw(11) << en.ehart;
	//		cout << setw(11) << en.etxc - en.etxcc;
			//if(DIAGO_TYPE=="cg") xiaohui modify 2013-09-02
			if(KS_SOLVER=="cg") //xiaohui add 2013-09-02
			{
				cout << setw(11) << avg_iter;
			}
			//xiaohui modified 2013-03-23
			//else if(DIAGO_TYPE=="selinv")
			//{
				// because Selinv::iter starts from 0.
			//	cout << setw(11) << Selinv::iter+1;
			//}
			cout << setw(11) << duration;
			cout << endl;
		}

	}
	else
	{
	//	if(iter_in % 4 == 0) cout << endl;
	//	cout << setprecision(6) << setw(15) << en.etot;
	}

	/*
	cout << setprecision(9);

	if(converged)
	{
		ofs_running << "\n end of self-consistenr calculation ";

		cout 
		<< "\n" << setw(20) << "  oooO      Oooo   |" << setw(15) << etot
		<< "\n" << setw(20) << " (####)    (####)  |" << setw(15) << dr2
		<< "\n" << setw(20) << " \\##(      )##/   |" << setw(15) << duration
		<< "\n" << setw(20) << "  \\_)      (_/    |"
		<< endl;
		
	}
	else
	{
		if(iter%2==0)
		{
			cout 
			<< "\n" << setw(20) << "  oooO             |" << setw(15) << etot
			<< "\n" << setw(20) << " (####)            |" << setw(15) << dr2
			<< "\n" << setw(20) << " \\##(             |" << setw(15) << duration
			<< "\n" << setw(20) << "  \\_)             |"
			<< endl;
		}
		else
		{
			cout 
			<< "\n" << setw(20) << "            Oooo   |" << setw(15) << etot
			<< "\n" << setw(20) << "           (####)  |" << setw(15) << dr2
			<< "\n" << setw(20) << "            )##/   |" << setw(15) << duration
			<< "\n" << setw(20) << "            (_/    |"
			<< endl;
		}
	}
	*/
        this->etot_old = this->etot;
	return;
}

void energy::print_format(const string &name, const double &value)
{
	ofs_running << setiosflags(ios::showpos);
	stringstream name2;
	name2 << name;
	ofs_running << " " << setw(12) << name2.str() << setw(30) <<  value 
	<< setw(30) << value * Ry_to_eV << endl;
	ofs_running << resetiosflags(ios::showpos);
	return;
}

// from ewald.f90
double energy::ewald(void)
{
    TITLE("energy","ewald");
    timer::tick("energy","ewald");

//----------------------------------------------------------
// Calculates Ewald energy with both G- and R-space terms.
// Determines optimal alpha. Should hopefully work for any structure.
//----------------------------------------------------------
    //int ng=0;
    int nr=0;
    int na=0;
    int nb=0;
    //int nt=0;
    int nrm=0;

    double ewaldg=0.0;
    double ewaldr=0.0;
    double ewalds=0.0;

    Vector3<double> dtau ;
    Vector3<double> *r;
    double *r2;
    double rmax=0.0;
    double rr=0.0;
    double upperbound=0.0;
    double fact=0;
    // total ionic charge in the cell
    // ewald energy computed in reciprocal space
    // ewald energy computed in real space
    // the difference tau_s - tau_s'
    // alpha term in ewald sum
    // input of the rgen routine ( not used here )
    // the square modulus of R_j-tau_s-tau_s'
    // the maximum radius to consider real space sum
    // buffer variable
    // used to optimize alpha

	if(test_energy)OUT(ofs_running,"mxr",mxr);
    r  = new Vector3<double>[mxr];
    r2 = new double[mxr];
    int* irr = new int[mxr];

    // (1) calculate total ionic charge
    double charge = 0.0;
    for (int it = 0;it < ucell.ntype;it++)
    {
        charge += ucell.atoms[it].na * ucell.atoms[it].zv;//mohan modify 2007-11-7
    }
    if(test_energy)OUT(ofs_running,"Total ionic charge",charge);

	// (2) calculate the converged value: alpha
    this->alpha = 2.90;
    do
    {
        alpha -= 0.10;
        // choose alpha in order to have convergence in the sum over G
        // upperbound is a safe upper bound for the error in the sum over G

        if (alpha <= 0.0)
        {
            WARNING_QUIT("ewald","Can't find optimal alpha.");
        }
        upperbound = 2.0 * charge * charge * sqrt(2.0 * alpha / TWO_PI) *
                     erfc(sqrt(ucell.tpiba2 * pw.ggchg / 4.0 / alpha));
    }
    while (upperbound > 1.0e-7);
    if(test_energy)OUT(ofs_running,"alpha",alpha);
	if(test_energy)OUT(ofs_running,"Upper bound",upperbound);

    // G-space sum here.
    // Determine if this processor contains G=0 and set the constant term
    if (pw.gstart == 1)
    {
        ewaldg = - charge * charge / alpha / 4.0;
    }
    else
    {
        ewaldg = 0.0;
    }


	// in plane wave basis, only use k=0 point is not 
	// called "gamma_only", only if the wave functions
	// are stored as double type, the gamma_only = true. 
	// I don't know why "gamma_only" in plane wave 
	// makes the fact below is 2, that's a little complicated
	// to understand. I think that may because only half
	// the G vectors are used. Unfortunately implement the 
	// function hasn't in my plan list yet.
	//
	// but that's not the term "gamma_only" I want to use in LCAO,  
	fact = 1.0;

    //ofs_running << "\n pw.gstart = " << pw.gstart << endl;

    for (int ig=pw.gstart; ig<pw.ngmc; ig++)
    {
        complex<double> rhon = ZERO;
        for (int it=0; it<ucell.ntype; it++)
        {
            rhon += static_cast<double>( ucell.atoms[it].zv ) * conj( pw.strucFac(it, ig));
        }
        if(pw.gg[ig] >= 1.0e-12) //LiuXh 20180515
        {
            ewaldg += fact * abs(rhon) * abs(rhon)
                      * exp(- pw.gg[ig] * ucell.tpiba2 / alpha / 4.0 ) / pw.gg[ig] / ucell.tpiba2;
        }
    }


    ewaldg = FOUR_PI / ucell.omega * ewaldg;

//	cout << "\n ewaldg = " << ewaldg;

    //  Here add the other constant term
	if(pw.gstart == 1)
	{
    	for (int it = 0; it < ucell.ntype;it++)
    	{
        	ewaldg = ewaldg - ucell.atoms[it].na * ucell.atoms[it].zv * ucell.atoms[it].zv * sqrt(8.0 / TWO_PI * alpha);
		}
    }//mohan modify 2007-11-7, 2010-07-26

    // R-space sum here (only done for the processor that contains G=0)
    ewaldr = 0.0;
    if (pw.gstart == 1)
    {
        rmax = 4.0 / sqrt(alpha) / ucell.lat0;
		if(test_energy)OUT(ofs_running,"rmax(unit lat0)",rmax);
        // with this choice terms up to ZiZj*erfc(4) are counted (erfc(4)=2x10^-8
        int nt1=0;
        int nt2=0;

        for (nt1 = 0; nt1 < ucell.ntype; nt1++)
        {
            for (nt2 = 0; nt2 < ucell.ntype; nt2++)
            {
                for (na = 0; na < ucell.atoms[nt1].na; na++)
                {
                    for (nb = 0; nb < ucell.atoms[nt2].na; nb++)
                    {
                        //calculate tau[na]-tau[nb]
                        dtau = ucell.atoms[nt1].tau[na] - ucell.atoms[nt2].tau[nb];
                        //generates nearest-neighbors shells
                        this->rgen(dtau, rmax, irr, ucell.latvec, ucell.G, r, r2, nrm);
                        //pw.at-->ucell.latvec, pw.bg-->pw.G
                        // and sum to the real space part

                        if (test_energy>1)
                        {
                            OUT("dtau.x",dtau.x);
                            OUT("dtau.y",dtau.y);
                            OUT("dtau.z",dtau.z);
                            OUT("nrm",nrm);
                        }
                        for (nr = 0;nr < nrm;nr++)
                        {
                            rr = sqrt(r2 [nr]) * ucell.lat0;
                            ewaldr = ewaldr + ucell.atoms[nt1].zv * ucell.atoms[nt2].zv *
                                     erfc(sqrt(alpha) * rr) / rr;

                        } // enddo
                        if (test_energy>1) OUT("ewaldr",ewaldr);
                    } // enddo
                } // enddo
            } // nt2
        }//nt1
    } // endif

    ewalds = 0.50 * e2 * (ewaldg + ewaldr);

	// mohan fix bug 2010-07-26
    Parallel_Reduce::reduce_double_pool( ewalds );

    if (test_energy>1)
    {
        OUT("ewaldg",ewaldg);
        OUT("ewaldr",ewaldr);
        OUT("ewalds",ewalds);
    }

    delete[] irr;
    delete[] r;
    delete[] r2;
    timer::tick("energy","ewald");
    return ewalds;
} // end function ewald

//-----------------------------------------------------------------------
void energy::rgen(
    const Vector3<double> &dtau,
    const double &rmax,
    int *irr,
    const Matrix3 &latvec,
    const Matrix3 &G,
    Vector3<double> *r,
    double *r2,
    int &nrm) const
{
    //-------------------------------------------------------------------
    // generates neighbours shells (in units of alat) with length
    // less than rmax,and returns them in order of increasing length.
    // r=i*a1+j*a2+k*a3-dtau,
    // where a1,a2,a3 are the vectors defining the lattice

    // first the dummy variables
    // int nrm, mxr;
    // output: the number of vectors in the spher
    // input: the maximum number of vectors

    // real r (3, mxr), r2 (mxr), at (3, 3), bg (3, 3), dtau (3),rmax;
    // output: coordinates of vectors R+tau_s-tau
    // output: square modulus of vectors R+tau_s-
    // input: direct lattice vectors
    // input: reciprocal lattice vectors
    // input: the vector tau_s-tau_s'
    // input: the radius of the sphere in real sp
    //    and here the local variables

    int nm1=0;
    int nm2=0;
    int nm3=0;
    int i=0;
    int j=0;
    int k=0;
    // index on R vectors for order
    //  maximum values for trial vectors
    //  counters on trial vectors
    // counter on polarizations
    // counter on R vectors
    // index of swapping
    // used for swapping

    Vector3<double> t;
    Vector3<double> t1;
    double tt=0.0;
    double bg1[3]={0,0,0};
    // buffer contains the actual r
    // buffer cotains the modulus of actual r
    // used for swapping
    // function to find the norm of a vector
    // external dnrm2, see myfunc

    nrm = 0;

    if (rmax == 0.0)
    {
        return;
    }

    bg1[0] = G.e11;
    bg1[1] = G.e12;
    bg1[2] = G.e13;

    nm1 = (int)(dnrm2(3, bg1, 1) * rmax + 2);

    bg1[0] = G.e21;
    bg1[1] = G.e22;
    bg1[2] = G.e23;

    nm2 = (int)(dnrm2(3, bg1, 1) * rmax + 2);

    bg1[0] = G.e31;
    bg1[1] = G.e32;
    bg1[2] = G.e33;

    nm3 = (int)(dnrm2(3, bg1, 1) * rmax + 2);

    if (test_energy>1)
    {
        OUT("nm1",nm1);
        OUT("nm2",nm2);
        OUT("nm3",nm3);
    }

    for (i = -nm1; i <= nm1; i++) // mohan fix bug, add '='. 2009-02-27
    {
        for (j = -nm2; j <= nm2; j++)
        {
            for (k = -nm3; k <= nm3; k++)
            {
                Vector3<double> t1(i,j,k);
//				out.printV3(t1);
                t = t1 * latvec; // bug ! first '*latvec', second '-dtau'.
                t = t - dtau; // bug ! t = t - dtau, not t1 = t1 -tau;

//				out.printV3(t);	// mohan fix 2bugs here, 2009-2-27
//				out.printM3("latvec",latvec);

                tt = t.x * t.x + t.y * t.y + t.z * t.z;

                if (tt <= rmax * rmax && abs(tt) > 1.e-10)
                {
                    if (nrm > mxr)
                    {
                        cerr << "\n rgen, too many r-vectors," << nrm;
                    }
                    r[nrm] = t;
                    r2[nrm] = tt;
                    nrm++;
                } // endif
            } // enddo
        } // enddo
    } // enddo

    //   reorder the vectors in order of increasing magnitude
    //   initialize the index inside sorting routine
    irr[0] = 0;
    if (nrm > 1)
    {
        heapsort(nrm, r2, irr);
    }

	// mohan fix bug 2011-06-07
	for(int i=0; i<nrm; i++)
	{
		back:
		const int index = irr[i];
		// large index before 'i' should be eliminated.
		// according to change elements again and again.
		if( index < i )
		{
			double swap_x = r[index].x;	
			double swap_y = r[index].y;	
			double swap_z = r[index].z;
			
			r[index].x = r[ irr[index] ].x;	
			r[index].y = r[ irr[index] ].y;			
			r[index].z = r[ irr[index] ].z;			
				
			r[ irr[index] ].x = swap_x;
			r[ irr[index] ].y = swap_y;
			r[ irr[index] ].z = swap_z;


			const int iswap = irr[i];
			irr[i] = irr[index];
			irr[index] = iswap;

			goto back;
		}	
	}

    return;
} //end subroutine rgen

#ifndef __EPM
// from ddelta_e.f90
double energy::delta_e(void)
{
    // out potentials from potential mixing
    // total energy and band energy corrections
	double deband0 = 0.0;

    double deband_aux = 0.0;

    for (int ir=0; ir<pw.nrxx; ir++) deband_aux -= chr.rho[0][ir] * pot.vr(0, ir);

    if (NSPIN == 2)
    {
        for (int ir=0; ir<pw.nrxx; ir++)
        {
            deband_aux -= chr.rho[1][ir] * pot.vr(1, ir);
        }

    }
    else if(NSPIN == 4)
    {
        for (int ir=0; ir<pw.nrxx; ir++)
        {
            deband_aux -= chr.rho[1][ir] * pot.vr(1, ir);
            deband_aux -= chr.rho[2][ir] * pot.vr(2, ir);
            deband_aux -= chr.rho[3][ir] * pot.vr(3, ir);
        }
    }

#ifdef __MPI
    MPI_Allreduce(&deband_aux,&deband0,1,MPI_DOUBLE,MPI_SUM,POOL_WORLD);
#else
    deband0 = deband_aux;
#endif

    deband0 *= ucell.omega / pw.ncxyz;
    return deband0;
} // end subroutine delta_e
#endif


#ifdef __EPM

#else
void energy::delta_escf(void)
{
	TITLE("energy","delta_escf");
    this->descf = 0.0;

	// now rho1 is "mixed" charge density
	// and rho1_save is "output" charge density
	// because in "deband" the energy is calculated from "output" charge density,
	// so here is the correction.
    for (int ir=0; ir<pw.nrxx; ir++) 
	{
		this->descf -= ( chr.rho[0][ir]- chr.rho_save[0][ir] ) * pot.vr(0,ir);
	}

    if (NSPIN==2)
    {
        for (int ir=0; ir<pw.nrxx; ir++)
        {
            this->descf -= ( chr.rho[1][ir] - chr.rho_save[1][ir] ) * pot.vr(1, ir);
        }
    }
    if (NSPIN==4)
    {
        for(int ir=0; ir<pw.nrxx; ir++)
        {
            this->descf -= ( chr.rho[1][ir] - chr.rho_save[1][ir] ) * pot.vr(1, ir);
            this->descf -= ( chr.rho[2][ir] - chr.rho_save[2][ir] ) * pot.vr(2, ir);
            this->descf -= ( chr.rho[3][ir] - chr.rho_save[3][ir] ) * pot.vr(3, ir);
        }
    }

    Parallel_Reduce::reduce_double_pool( descf );

    this->descf *= ucell.omega / pw.ncxyz;
    return;
}
#endif

void energy::perform_dos(void)
{
    TITLE("energy","perform_dos");

    if(out_dos !=0 || out_band !=0)
    {
        ofs_running << "\n\n\n\n";
        ofs_running << " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        ofs_running << " |                                                                    |" << endl;
        ofs_running << " | Post-processing of data:                                           |" << endl;
        ofs_running << " | DOS (density of states) and bands will be output here.             |" << endl;
        ofs_running << " | If atomic orbitals are used, Mulliken charge analysis can be done. |" << endl;
        ofs_running << " | Also the .bxsf file containing fermi surface information can be    |" << endl;
        ofs_running << " | done here.                                                         |" << endl;
        ofs_running << " |                                                                    |" << endl;
        ofs_running << " <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
        ofs_running << "\n\n\n\n";
    }

    if(MY_RANK==0)
    {
        if(CALCULATION=="scf" || CALCULATION=="md" || CALCULATION=="relax")
        //if(CALCULATION=="scf" || CALCULATION=="md")
        {
            stringstream ss;
            ss << global_out_dir << "istate.info" ;
            ofstream ofsi( ss.str().c_str() );
            /*for(int ib=0; ib<NBANDS; ++ib)
            {
                ofsi << "0 " << ib+1;
                for(int is=0; is<NSPIN; ++is)
                {
                    ofsi << " " << wf.ekb[is][ib];
                }
                ofsi << endl;
            }*/
            // pengfei 2015-4-1
            if(NSPIN == 1||NSPIN == 4)
            { 
                for (int ik = 0;ik < kv.nks;ik++)
                {                       
                    ofsi<<"BAND"<<setw(25)<<"Energy(ev)"<<setw(25)<<"Occupation"<<setw(25)<<"Kpoint = "<<ik+1<<setw(25)<<"("<<kv.kvec_d[ik].x<<" "<<kv.kvec_d[ik].y<<" "<<kv.kvec_d[ik].z<<")"<<endl;
                    for(int ib=0;ib<NBANDS;ib++)
                    {
                        ofsi<<ib+1<<setw(25)<<wf.ekb[ik][ib]* Ry_to_eV<<setw(25)<<wf.wg(ik,ib)<<endl;
                    }
                    ofsi <<endl; 
                    ofsi <<endl;                              
                }
            }
            else
            {
                for (int ik = 0;ik < kv.nks/2;ik++)
                {
                    ofsi<<"BAND"<<setw(25)<<"Spin up Energy(ev)"<<setw(25)<<"Occupation"<<setw(25)<<"Spin down Energy(ev)"<<setw(25)<<"Occupation"<<setw(25)<<"Kpoint = "
                        <<ik+1<<setw(25)<<"("<<kv.kvec_d[ik].x<<" "<<kv.kvec_d[ik].y<<" "<<kv.kvec_d[ik].z<<")"<<endl;
                    for(int ib=0;ib<NBANDS;ib++)
                    {
                        ofsi<<ib+1<<setw(25)<<wf.ekb[ik][ib]* Ry_to_eV<<setw(25)<<wf.wg(ik,ib)<<setw(25)<<wf.ekb[(ik+kv.nks/2)][ib]* Ry_to_eV<<setw(25)<<wf.wg(ik+kv.nks/2,ib)<<endl;
                    }
                    ofsi <<endl;
                    ofsi <<endl;
                }
            }
            ofsi.close();
        }
    }
    // mulliken charge analysis
    if(mulliken == 1)
    {
        Mulliken_Charge   MC;
        MC.stdout_mulliken();			
    }//qifeng add 2019/9/10

    int nspin0=1;
    if(NSPIN==2) nspin0=2;

    if(this->out_dos)
    {
        // find the maximal and minimal band energy.
        double emax = wf.ekb[0][0];
        double emin = wf.ekb[0][0];
        for(int ik=0; ik<kv.nks; ++ik)
        {
            for(int ib=0; ib<NBANDS; ++ib)
            {
                emax = std::max( emax, wf.ekb[ik][ib] );
                emin = std::min( emin, wf.ekb[ik][ib] );
            }
        }

#ifdef __MPI
        Parallel_Reduce::gather_max_double_all(emax);
        Parallel_Reduce::gather_min_double_all(emin);
#endif

        emax *= Ry_to_eV;
        emin *= Ry_to_eV;

        // OUT(ofs_running,"minimal energy is (eV)", emin);
        // OUT(ofs_running,"maximal energy is (eV)", emax);
        // output the PDOS file.////qifeng-2019-01-21
        // atom_arrange::set_sr_NL();
        // atom_arrange::search( SEARCH_RADIUS );//qifeng-2019-01-21
        const double de_ev = this->dos_edelta_ev;
 
        const int npoints = static_cast<int>(std::floor ( ( emax - emin ) / de_ev ));
        int NUM=NLOCAL*(npoints+1);
        Wfc_Dm_2d D;
        D.init();
        if(GAMMA_ONLY_LOCAL)
        {
            for(int in=0;in<NSPIN;in++)
            {
                D.wfc_gamma[in]=LOC.wfc_dm_2d.wfc_gamma[in];
            }
        }
        else 
        {
            for(int in=0;in<kv.nks;in++)
            {
                D.wfc_k[in] = LOC.wfc_dm_2d.wfc_k[in];
            }
        }
 
        const int np=npoints+1;
        matrix*  pdosk = new matrix[nspin0];

        for(int is=0; is<nspin0; ++is)
        {
            pdosk[is].create(NLOCAL,np,true);
        }
        matrix*  pdos = new matrix[nspin0];
        for(int is=0; is<nspin0; ++is)
        {
            pdos[is].create(NLOCAL,np,true);
        }

        double a = bcoeff;
        double b =  sqrt(3.1415926)*a;                                         
               
        complex<double> *waveg = new complex<double> [NLOCAL];

        double* Gauss = new double [np];

        for(int is=0; is<nspin0; ++is)
        {
            if(GAMMA_ONLY_LOCAL)
	    {
                std::vector<matrix> Mulk;
                Mulk.resize(1);
                Mulk[0].create(ParaO.ncol,ParaO.nrow);

                matrix Dwf = D.wfc_gamma[is];
                //for (int i=0; i<NBANDS; ++i)		  
                for (int i=1; i<NBANDS-1; ++i)		  
                {
                    ZEROS(waveg, NLOCAL);
                    ZEROS(Gauss,np);
                    //for (int n=0; n<npoints+1; ++n)		  
                    for (int n=1; n<npoints+1; ++n)		  
                    {
                        double en=emin+n * de_ev;
                        double en0=wf.ekb[0][i]*Ry_to_eV;
                        double de = en-en0;
                        double de2 = de * de;
                        Gauss[n] = kv.wk[0]*exp(-de2/a/a)/b;
                    }

                    const int NB= i+1;
                    const double one_float=1.0, zero_float=0.0;
                    const int one_int=1;
                    const char T_char='T';		
                    pdgemv_(
                        &T_char,
                        &NLOCAL,&NLOCAL,
                        &one_float,
                        LM.Sloc, &one_int, &one_int, ParaO.desc,
                        Dwf.c, &one_int, &NB, ParaO.desc, &one_int,
                        &zero_float,
                        Mulk[0].c, &one_int, &NB, ParaO.desc,
                        &one_int);

                    for (int j=0; j<NLOCAL; ++j)
                    {
                        if ( ParaO.in_this_processor(j,i) )
                        {
                            const int ir = ParaO.trace_loc_row[j];
                            const int ic = ParaO.trace_loc_col[i];
                            waveg[j] = Mulk[0](ic,ir)*D.wfc_gamma[is](ic,ir);
                            const double x = waveg[j].real();
                            LapackConnector::axpy(np , x,Gauss, 1,pdosk[is].c+j*pdosk[is].nc,1);
                        }
                    }
                }//ib
            }//if
            else
            {
                atom_arrange::set_sr_NL();
                atom_arrange::search( SEARCH_RADIUS );//qifeng-2019-01-21

                hm.hon.set_orb_tables();
                LM.allocate_HS_R(LNNR.nnr);
                LM.zeros_HSR('S', LNNR.nnr);
                UHM.UOM.calculate_S_no();
                UHM.UOM.build_ST_new('S', false);
                std::vector<ComplexMatrix> Mulk;
                Mulk.resize(1);
                Mulk[0].create(ParaO.ncol,ParaO.nrow);

                for(int ik=0;ik<kv.nks;ik++)
                {
                    if(is == kv.isk[ik])
                    {
                        LM.allocate_HS_k(ParaO.nloc);
                        LM.zeros_HSk('S');
                        LNNR.folding_fixedH(ik);

                        ComplexMatrix Dwfc = conj(D.wfc_k[ik]);

                        for (int i=0; i<NBANDS; ++i)
                        {
                            ZEROS(waveg, NLOCAL);
                            ZEROS(Gauss,np);
                            for (int n=0; n<npoints+1; ++n)		  
                            {
                                double en=emin+n * de_ev;
                                double en0=wf.ekb[ik][i]*Ry_to_eV;
                                double de = en-en0;
                                double de2 = de * de;
                                Gauss[n] = kv.wk[ik]*exp(-de2/a/a)/b;
                            }
                            const int NB= i+1;
                            const double one_float=1.0, zero_float=0.0;
                            const int one_int=1;
                            //const int two_int=2;
                            const char T_char='T'; // N_char='N',U_char='U'

                            pzgemv_(
                                &T_char,
                                &NLOCAL,&NLOCAL,
                                &one_float,
                                LM.Sloc2, &one_int, &one_int, ParaO.desc,
                                Dwfc.c, &one_int, &NB, ParaO.desc, &one_int,
                                &zero_float,
                                Mulk[0].c, &one_int, &NB, ParaO.desc,
                                &one_int);

                            for (int j=0; j<NLOCAL; ++j)
                            {
                                if ( ParaO.in_this_processor(j,i) )
                                {
                                    const int ir = ParaO.trace_loc_row[j];
                                    const int ic = ParaO.trace_loc_col[i];

                                    waveg[j] = Mulk[0](ic,ir)*D.wfc_k[ik](ic,ir);
                                    const double x = waveg[j].real();
                                    LapackConnector::axpy(np , x,Gauss, 1,pdosk[is].c+j*pdosk[is].nc,1);
                                }
                            }
                        }//ib
                    }//if
                }//ik
#ifdef __MPI
                atom_arrange::delete_vector( SEARCH_RADIUS );
#endif
                hm.hon.clear_after_ions();
            }//else

            MPI_Reduce(pdosk[is].c, pdos[is].c , NUM , MPI_DOUBLE , MPI_SUM, 0, MPI_COMM_WORLD);
        }//is                                              
        delete[] pdosk;                                               
        delete[] waveg;
        if(MY_RANK == 0)
        {
            {stringstream ps;
                ps << global_out_dir << "TDOS";
                ofstream out(ps.str().c_str());
                if (NSPIN==1)
                {
                    //for (int n=0; n<npoints+1; ++n)
                    for (int n=1; n<npoints+1; ++n)
                    {
                        double y=0.0;
                        double en=emin + n * de_ev;
                        for (int i=0; i<NLOCAL; i++)
                        {
                            y +=  pdos[0](i,n);
                        }
                        out <<setw(20)<< en <<setw(30)<< y << endl;
                    }
                }
                else if (NSPIN==2)
                {
                    for (int n=0; n<npoints+1; ++n)
                    {
                        double y=0.0;
                        double z=0.0;
                        double en=emin + n * de_ev;
                        for (int i=0; i<NLOCAL; i++)
                        {
                            y +=  pdos[0](i,n);
                            z +=  pdos[1](i,n);
                        }

                        out <<setw(20)<< en <<setw(30)<< y << setw(30)<< z<< endl;
                    }
                }
                out.close();}
 
            string Name_Angular[5][11];
            /* decomposed Mulliken charge */

            Name_Angular[0][0] = "s          ";
            Name_Angular[1][0] = "px         ";
            Name_Angular[1][1] = "py         ";
            Name_Angular[1][2] = "pz         ";
            Name_Angular[2][0] = "d3z^2-r^2  ";
            Name_Angular[2][1] = "dx^2-y^2   ";
            Name_Angular[2][2] = "dxy        ";
            Name_Angular[2][3] = "dxz        ";
            Name_Angular[2][4] = "dyz        ";
            Name_Angular[3][0] = "f5z^2-3r^2 ";
            Name_Angular[3][1] = "f5xz^2-xr^2";
            Name_Angular[3][2] = "f5yz^2-yr^2";
            Name_Angular[3][3] = "fzx^2-zy^2 ";
            Name_Angular[3][4] = "fxyz       ";
            Name_Angular[3][5] = "fx^3-3*xy^2";
            Name_Angular[3][6] = "f3yx^2-y^3 ";
            Name_Angular[4][0] = "g1         ";
            Name_Angular[4][1] = "g2         ";
            Name_Angular[4][2] = "g3         ";
            Name_Angular[4][3] = "g4         ";
            Name_Angular[4][4] = "g5         ";
            Name_Angular[4][5] = "g6         ";
            Name_Angular[4][6] = "g7         ";
            Name_Angular[4][7] = "g8         ";
            Name_Angular[4][8] = "g9         ";

            {stringstream as;
            as << global_out_dir << "PDOS";
            ofstream out(as.str().c_str());

            out << "<"<<"pdos"<<">" <<endl;
            out << "<"<<"nspin"<<">" << NSPIN<< "<"<<"/"<<"nspin"<<">"<< endl;
            out << "<"<<"norbitals"<<">" <<setw(2) <<NLOCAL<< "<"<<"/"<<"norbitals"<<">"<< endl;
            out << "<"<<"energy"<<"_"<<"values units"<<"="<<"\""<<"eV"<<"\""<<">"<<endl;

            for (int n=0; n<npoints+1; ++n)
            {
                double y=0.0;
                double en=emin + n * de_ev;
                out <<setw(20)<< en << endl;
            }
            out << "<"<<"/"<<"energy"<<"_"<<"values"<<">" <<endl;
            for (int i=0; i<ucell.nat; i++)
            {
                int a = ucell.iat2ia[i];
                int t = ucell.iat2it[i];
                Atom* atom1 = &ucell.atoms[t];
                for(int j=0; j<atom1->nw; ++j)
                {
                    const int L1 = atom1->iw2l[j];
                    const int N1 = atom1->iw2n[j];
                    const int m1 = atom1->iw2m[j];
                    const int w = ucell.itiaiw2iwt(t, a, j);

                    //out << "<"<<"/"<<"energy"<<"_"<<"values"<<">" <<endl;
                    out << "<"<<"orbital" <<endl;
                    out <<setw(6)<< "index"<<"="<<"\""<<setw(40) <<w+1<<"\""<<endl;
                    out <<setw(5)<< "atom"<<"_"<<"index"<<"="<<"\""<<setw(40) <<i+1<<"\""<<endl;
                    out <<setw(8)<< "species"<<"="<<"\""<<ucell.atoms[t].label<<"\""<<endl;
                    out<<setw(2)<< "l"<<"="<<"\""<<setw(40)<<L1<<"\""<<endl;
                    out <<setw(2)<< "m"<<"="<<"\""<<setw(40)<<m1<<"\""<<endl;
                    out <<setw(2)<< "z"<<"="<<"\""<<setw(40)<<N1+1<<"\""<<endl;
                    out << ">" <<endl;
                    out << "<"<<"data"<<">" <<endl;

                    if (NSPIN==1)
                    {
                        for (int n=0; n<npoints+1; ++n)
                        {
                            out <<setw(13)<< pdos[0](w,n)<<endl;
                        }//n
                    }
                    else if (NSPIN==2)
                    {
                        for (int n=0; n<npoints+1; ++n)
                        {
                            out <<setw(20)<< pdos[0](w,n)<< setw(30)<< pdos[1](w,n)<<endl;
                        }//n
                    }

                    out << "<"<<"/"<<"data"<<">" <<endl;
                }//j
            }//i

            out << "<"<<"/"<<"orbital"<<">" <<endl;
            out << "<"<<"/"<<"pdos"<<">" <<endl;
            out.close();}
            
            {stringstream os;
            os<<global_out_dir<<"Orbital";
            ofstream out(os.str().c_str());
            out<< setw(5)<<"io"<< setw(8) <<"spec" <<setw(5)<<"l"<<setw(5)<<"m"<<setw(5)<<"z"<<setw(5)<<"sym"<<endl;

            for (int i=0; i<ucell.nat; i++)
            {
                int t = ucell.iat2it[i];
                Atom* atom1 = &ucell.atoms[t];  
                for(int j=0; j<atom1->nw; ++j)
                {
                    const int L1 = atom1->iw2l[j];
                    const int N1 = atom1->iw2n[j];
                    const int m1 = atom1->iw2m[j];
                    out <<setw(5) << i << setw(8) << ucell.atoms[t].label <<setw(5)<<L1<<setw(5) <<m1<<setw(5)<<N1+1<<setw(15)<< Name_Angular[L1][m1] << endl;
                }
            }
            out <<endl<<endl;
            out <<setw(5)<< "io"<<setw(2)<<"="<<setw(2)<<"Orbital index in supercell"<<endl;
            out <<setw(5)<< "spec"<<setw(2)<<"="<<setw(2)<<"Atomic species label"<<endl;
            out <<setw(5)<< "l"<<setw(2)<<"="<<setw(2)<<"Angular mumentum quantum number"<<endl;
            out <<setw(5)<< "m"<<setw(2)<<"="<<setw(2)<<"Magnetic quantum number"<<endl;
            out <<setw(5)<< "z"<<setw(2)<<"="<<setw(2)<<"Zeta index of orbital"<<endl;
            out <<setw(5)<< "sym"<<setw(2)<<"="<<setw(2)<<"Symmetry name of real orbital"<<endl;
            out.close();}
        }
        delete[] pdos;

        // output the DOS file.
        for(int is=0; is<nspin0; ++is)
        {
            stringstream ss;
            ss << global_out_dir << "DOS" << is+1;

            Dos::calculate_dos(
                is,
                kv.isk,
                ss.str(), 
                this->dos_edelta_ev, 
                emax, 
                emin, 
                kv.nks, kv.nkstot, kv.wk, wf.wg, NBANDS, wf.ekb );
            ifstream in(ss.str().c_str());
            if(!in)
            {
                //cout<<"\n Can't find file : "<< name << endl;
                //return 0;
            }

            //----------------------------------------------------------
            // FOUND LOCAL VARIABLES :
            // NAME : number(number of DOS points)
            // NAME : nk(number of k point used)
            // NAME : energy(energy range,from emin_ev to emax_ev)
            // NAME : dos(old,count k points in the energy range)
            // NAME : dos2(new,count k points in the energy range)
            //----------------------------------------------------------
            int number=0;
            int nk=0;
            in >> number;
            in >> nk;
            double *energy = new double[number];
            double *dos = new double[number];
            double *dos2 = new double[number];
            for(int i=0 ;i<number; i++)
            {
                energy[i] = 0.0;
                dos[i] = 0.0;
                dos2[i] =0.0;
            }

            for(int i=0;i<number;i++)
            {
                in >> energy[i] >> dos[i];
            }
            if(!in.eof())
            {
                //cout<<"\n Read Over!"<<endl;
            }
            in.close();

            //----------------------------------------------------------
            // EXPLAIN : b is an empirical value.
            // please DIY b!!
            //----------------------------------------------------------

            //double b = INPUT.b_coef;
            double b = bcoeff;
            for(int i=0;i<number;i++)
            {
                double Gauss=0.0;

                for(int j=0;j<number;j++)
                {
                    double de = energy[j] - energy[i];
                    double de2 = de * de;
                    //----------------------------------------------------------
                    // EXPLAIN : if en
                    //----------------------------------------------------------
                    Gauss = exp(-de2/b/b)/sqrt(3.1415926)/b;
                    dos2[j] += dos[i]*Gauss;
                }
            }

            //----------------------------------------------------------
            // EXPLAIN : output DOS2.txt
            //----------------------------------------------------------
            stringstream sss;
            sss << global_out_dir << "DOS" << is+1 << "_smearing" << ".dat" ;
            ofstream out(sss.str().c_str());
            double sum2=0.0;
            for(int i=0;i<number;i++)
            {
                sum2 += dos2[i];
                //if(dos2[i]<1e-5)
                //{
                //    dos2[i] = 0.00;
                //}
                if(MY_RANK==0)
                {
                    out <<setw(20)<<energy[i]
                        <<setw(20)<<dos2[i]
                        <<setw(20)<<sum2<<"\n";
                }
            }
            out.close();

            //----------------------------------------------------------
            // DELETE
            //----------------------------------------------------------
            delete[] dos;
            delete[] dos2;
            delete[] energy;

            //cout<<" broden spectrum over, success : ) "<<endl;

        }

        // mulliken charge analysis
        if(out_dos == 2)
        {
            stringstream sp;
            sp << global_out_dir << "Mulliken.dat";
            Dos::calculate_Mulliken(sp.str());
        }

        if(nspin0==1)
        {
            ofs_running << " Fermi energy is " << this->ef << " Rydberg" << endl;
        }
        else if(nspin0==2)
        {
            ofs_running << " Fermi energy (spin = 1) is " << this->ef_up << " Rydberg" << endl;
            ofs_running << " Fermi energy (spin = 2) is " << this->ef_dw << " Rydberg" << endl;
        }

        //int nks;
        //if(nspin0==1) nks = kv.nkstot;
        //else if(nspin0==2) nks = kv.nkstot/2;

        /*
        for(int is=0; is<NSPIN; is++)
        {
            stringstream ss2;
            ss2 << global_out_dir << "BANDS_" << is+1 << ".dat";
            ofs_running << "\n Output bands in file: " << ss2.str() << endl;
            Dos::nscf_band(is, ss2.str(), nks, NBANDS, this->ef, wf.ekb);
        }*/

        if(out_dos==3)
        {
            for(int i=0; i<nspin0; i++)
            {
                stringstream ss3;
                ss3 << global_out_dir << "Fermi_Surface_" << i << ".bxsf";
                Dos::nscf_fermi_surface(ss3.str(),kv.nks,NBANDS,wf.ekb);
            }
        }
    }

    if(this->out_band) //pengfei 2014-10-13
    {
        int nks;
        if(nspin0==1) nks = kv.nkstot;
        else if(nspin0==2) nks = kv.nkstot/2;

        for(int is=0; is<nspin0; is++)
        {
            stringstream ss2;
            ss2 << global_out_dir << "BANDS_" << is+1 << ".dat";
            ofs_running << "\n Output bands in file: " << ss2.str() << endl;
            Dos::nscf_band(is, ss2.str(), nks, NBANDS, this->ef*0, wf.ekb);
        }
    }
}

void energy::print_band(const int &ik)
{
	//check the band energy.
    bool wrong = false;
	for(int ib=0; ib<NBANDS; ++ib)
	{
		if( abs( wf.ekb[ik][ib] ) > 1.0e10)
		{
			ofs_warning << " ik=" << ik+1 << " ib=" << ib+1 << " " << wf.ekb[ik][ib] << " Ry" << endl;
			wrong = true;
		}
	}
	if(wrong)
    {
        WARNING_QUIT("Threshold_Elec::print_eigenvalue","Eigenvalues are too large!");
    }



	if(MY_RANK==0)
	{
		//if( DIAGO_TYPE == "selinv" ) xiaohui modify 2013-09-02
		if(KS_SOLVER=="selinv") //xiaohui add 2013-09-02
		{
			ofs_running << " No eigenvalues are available for selected inversion methods." << endl;	
		}
		else
		{
			if( printe>0 && ((this->iter+1) % this->printe == 0))
			{
				//	NEW_PART("ENERGY BANDS (Rydberg), (eV)");
				ofs_running << setprecision(6);
				ofs_running << " Energy (eV) & Occupations  for spin=" << CURRENT_SPIN+1 << " K-point=" << ik+1 << endl;
				ofs_running << setiosflags(ios::showpoint);
				for(int ib=0;ib<NBANDS;ib++)
				{
					ofs_running << " "<< setw(6) << ib+1  
						<< setw(15) << wf.ekb[ik][ib] * Ry_to_eV;
					// for the first electron iteration, we don't have the energy
					// spectrum, so we can't get the occupations. 
					ofs_running << setw(15) << wf.wg(ik,ib);
					ofs_running << endl;
				}
			}
		}
	}
	return;
}
