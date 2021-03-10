// =============================================================================
//                          C++ Header File
// Project:         Wannier Basis O(N) Scaling Package
// File:            electronsns.cpp
// Principal Class:	electronsns
// Author:          Lixin He,Mohan Chen
// Comment:
// Warning:
// Start time:      2006
// modified:   		mohan 2008-08-10 change electronsns to be base class of ions
// =============================================================================

#include "global.h"
#include "threshold_elec.h"
Threshold_Elec::Threshold_Elec() 
{ 
	dr2 = 0.0; 
	conv_elec = false; 
}

void Threshold_Elec::set_ethr(void) const
{
    //========================================================================
    // setup ethr see setup.f90 Page 5/14
    // setup ethr, the convergence threshold for eigenvalues
    // tr2: the convergence threshold for self-consistency (rho or potential);
    //========================================================================

    //==================
    // nscf calculation
    //==================
    if (CALCULATION=="nscf")
    {
        if (ETHR == 0.0)
        {
            ETHR = 0.1 * std::min(1.0e-2, DRHO2 / ucell.nelec);
        }
    }
    //================
    // self consistent
    //================
    else if(CALCULATION=="scf" || CALCULATION=="md" || CALCULATION=="relax")
    {
        if (ETHR == 0.0)
        {
            if (pot.start_pot == "file")
            {
                //======================================================
                // if you think that the starting potential is good
                // do not spoil it with a louly first diagonalization:
                // set a strict ethr in the input file ()diago_the_init
                //======================================================
                ETHR = 1.0e-5;
            }
            else
            {
                //=======================================================
                // starting atomic potential is probably far from scf
                // don't waste iterations in the first diagonalization
                //=======================================================
                ETHR = 1.0e-2;
            }
        }
    }

    return;
}

void Threshold_Elec::update_ethr(const int &iter)
{
    //======================================================
    // Convergence threshold for iterative diagonalization
    // is automatically updated during self consistency
    //======================================================
    if (iter > 1)
    {
        if (iter == 2)
        {
            ETHR = 1.e-2;
        }

		//----------------------------
		// ETHR changes in CG Method. 
		// mohan update 2012-03-26
		// mohan update 2012-02-08
		//----------------------------
		//if(LINEAR_SCALING==1 && LOCAL_BASIS==4) xiaohui modify 2013-09-02
		if(BASIS_TYPE=="lcao") //xiaohui add 2013-09-02
		{
			ETHR = std::min( ETHR, 0.01*dr2/ std::max(1.0, ucell.nelec));
		}
		// mohan update 2009-09-04
		else
		{
			ETHR = std::min( ETHR, 0.1*dr2/ std::max(1.0, ucell.nelec));
			//cout << " new ethr = " << ETHR << endl;
		}

    }
    return;
}

void Threshold_Elec::iter_end(ofstream &ofs)
{
        //figure::picture(d_rho,pw.ncx,pw.ncy,pw.ncz);
	//xiaohui add 'OUT_LEVEL' line, 2015-09-16
	if(OUT_LEVEL != "m") print_eigenvalue(ofs);

//      if (pot.tefield)
//      {
//          ofs << "\n electric field correction = "<< pot.etotefield << " ryd";
//      }
//      if(lda_plus_u)
//      {
//          ofs << "\n Hubbard energy            = "<< eth << " ryd";
//      }
//    if ( Occupy::gauss() )
//    {
//          ofs << " Correction of metals = "<< en.demet << " Rydberg" << endl;
//    }
    return;
}

void Threshold_Elec::print_eigenvalue(ofstream &ofs)
{
	bool wrong = false;
	for(int ik=0; ik<kv.nks; ++ik)
	{
		for(int ib=0; ib<NBANDS; ++ib)
		{
			if( abs( wf.ekb[ik][ib] ) > 1.0e10)
			{
				ofs_warning << " ik=" << ik+1 << " ib=" << ib+1 << " " << wf.ekb[ik][ib] << " Ry" << endl;
				wrong = true;
			}
		}
	}
	if(wrong)
	{
		WARNING_QUIT("Threshold_Elec::print_eigenvalue","Eigenvalues are too large!");
	}


	if(MY_RANK!=0) return;
	TITLE("Threshold_Elec","print_eigenvalue");

    ofs << "\n STATE ENERGY(eV) AND OCCUPATIONS.";
	ofs << setprecision(5);
    for (int ik = 0;ik < kv.nks;ik++)
    {
        if (NSPIN==2)
        {
            if (kv.isk[ik] == 0)ofs << "\n spin up :";
            if (kv.isk[ik] == 1)ofs << "\n spin down :";
        }
        
        if (NSPIN==2)
        {
            if (kv.isk[ik] == 0)
            {
                ofs << " " << ik+1 << "/" << kv.nks/2 << " kpoint (Cartesian) = "
                << kv.kvec_c[ik].x << " " << kv.kvec_c[ik].y << " " << kv.kvec_c[ik].z
                << " (" << kv.ngk[ik] << " pws)" << endl;

                ofs << setprecision(6);

            }
            if (kv.isk[ik] == 1)
            {
                ofs << " " << ik+1-kv.nks/2 << "/" << kv.nks/2 << " kpoint (Cartesian) = "
                << kv.kvec_c[ik].x << " " << kv.kvec_c[ik].y << " " << kv.kvec_c[ik].z
                << " (" << kv.ngk[ik] << " pws)" << endl;

                ofs << setprecision(6);

            }
        }       // Pengfei Li  added  14-9-9
	else	
	{
             	ofs << " " << ik+1 << "/" << kv.nks << " kpoint (Cartesian) = " 
		<< kv.kvec_c[ik].x << " " << kv.kvec_c[ik].y << " " << kv.kvec_c[ik].z 
		<< " (" << kv.ngk[ik] << " pws)" << endl; 

		ofs << setprecision(6);
        }
		//----------------------
		// no energy to output
		//----------------------
		//if(DIAGO_TYPE=="selinv") xiaohui modify 2013-09-02
		if(KS_SOLVER=="selinv") //xiaohui add 2013-09-02
		{
			ofs << " USING SELINV, NO BAND ENERGY IS AVAILABLE." << endl;
		}
		//----------------------
		// output energy
		//----------------------
		else
		{
			//ofs << setw(12) << kv.ngk[ik] << " PWs ";
			ofs_running << setprecision(6);
			ofs_running << setiosflags(ios::showpoint);
			for (int ib = 0; ib < NBANDS; ib++)
			{
				ofs << " [spin" << kv.isk[ik]+1 << "_state] " << setw(8) << ib+1 
				<< setw(15) << wf.ekb[ik][ib] * Ry_to_eV << setw(15) << wf.wg(ik, ib) << endl;
			}
			ofs << endl;
		}
    }//end ik
    return;
}

