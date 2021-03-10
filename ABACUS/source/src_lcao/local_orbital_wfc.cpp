#include "local_orbital_wfc.h"
#include "../src_pw/global.h"
#include "../src_pw/algorithms.h"
#include "../src_lcao/wf_local.h"

Local_Orbital_wfc::Local_Orbital_wfc()
{
	allocate_flag = false;
	allocate_aug_flag = false;
	trace_aug = nullptr;	
	wfck_flag = false;	
	complex_flag = false;
}

Local_Orbital_wfc::~Local_Orbital_wfc()
{
	// used for gamma
	if(allocate_flag)
	{
		if(BFIELD)
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int i=0; i<NBANDS; i++) 
				{
					delete[] this->WFC_GAMMA_B[is][i];
				}
				delete[] this->WFC_GAMMA_B[is];
			}
			delete[] this->WFC_GAMMA_B;
		}
		else
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int i=0; i<NBANDS; i++) 
				{
					delete[] this->WFC_GAMMA[is][i];
				}
				delete[] this->WFC_GAMMA[is];
			}
			delete[] this->WFC_GAMMA;
		}
	}

	// used for force
	if(allocate_aug_flag)
	{
		if(GAMMA_ONLY_LOCAL)
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int i=0; i<NBANDS; i++) 
				{
					delete[] this->WFC_GAMMA_aug[is][i];
				}
				delete[] this->WFC_GAMMA_aug[is];
			}
			delete[] this->WFC_GAMMA_aug;
		}
		//mohan add 2012-01-09
		else
		{
			for(int ik=0; ik<kv.nks; ++ik)
			{
				for(int ib=0; ib<NBANDS; ++ib)
				{
					delete[] this->WFC_K_aug[ik][ib];
				}
				delete[] this->WFC_K_aug[ik];
			}
			delete[] this->WFC_K_aug;
		}
	}

	// used for k-points.
	if(complex_flag && this->wfck_flag)
	{
		for(int i=0; i<kv.nks; i++)
		{
			//for(int j=0; j<NBANDS; j++)
			//{
			//	delete[] this->WFC_K[i][j];
			//}
			delete[] this->WFC_K[i];
			//cout<<"delete WFC_K["<<i<<"] success"<<endl;
		}
		delete[] this->WFC_K;
		//cout<<"delete WFC_K success"<<endl;
		if(NLOCAL!= 0 )
		{
			delete[] this->WFC_K_POOL;
			//cout<<"delete WFC_K_POOL success"<<endl;
		}
	}

}

void Local_Orbital_wfc::allocate_k(const Grid_Technique &gt)
{
	TITLE("Local_Orbital_wfc","allocate_k");
	if(NLOCAL < NBANDS)
	{
		WARNING_QUIT("Local_Orbital_wfc::allocate","NLOCAL<NBANDS");
	}

	// mohan add the flag 2011-03-02
	// allocate the first part (only once!).
	if(this->wfck_flag == false)
	{
		this->WFC_K = new complex<double>**[kv.nks];
		for(int ik=0; ik<kv.nks; ik++)
		{
			this->WFC_K[ik] = new complex<double>*[NBANDS];
		}
		this->wfck_flag = true;
	}
	
	if(this->complex_flag)
	{
		delete[] this->WFC_K_POOL;
		this->complex_flag = false;
	}
	// allocate the second part.
	//if(gt.lgd != 0) xiaohui modify 2015-02-04, fixed memory bug
	//if(gt.lgd != 0 && this->complex_flag == false)
	if(gt.lgd != 0)
	{
		//cout<<"gt.lgd="<<gt.lgd<<" ; NLOCAL="<<NLOCAL<<endl; //delete 2015-09-06, xiaohui
		const int page=NBANDS*gt.lgd;
		this->WFC_K_POOL=new complex<double> [kv.nks*page];
		ZEROS(WFC_K_POOL, kv.nks*page);
		for(int ik=0; ik<kv.nks; ik++)
		{
			for(int ib=0; ib<NBANDS; ib++)
			{
				this->WFC_K[ik][ib] = &WFC_K_POOL[ik*page+ib*gt.lgd];
				//cout<<"ik="<<ik<<" ib="<<ib<<endl<<"WFC_K address: "<<WFC_K[ik][ib]<<" WFC_K_POOL address: "<<&WFC_K_POOL[ik*page+ib*gt.lgd]<<endl;
			}
			//cout<<"set WFC_K pointer success, ik: "<<ik<<endl;
			Memory::record("LocalOrbital_Coef","WFC_K",NBANDS*NLOCAL,"cdouble");
			//OUT(ofs_running,"MemoryForWaveFunctions (MB)",mem);
			//cout<<"WFC_K["<<ik<<"] use "<<mem<<" MB"<<endl;
			this->complex_flag = true;
		}
	}

	if(wf.start_wfc == "atomic" )
	{
		
	}
	else if(wf.start_wfc == "file")
	{
		int error;
		cout << " Read in wave functions files: " << kv.nkstot << endl;
		for(int ik=0; ik<kv.nkstot; ++ik)
		{
			ofs_running << " Read in wave functions " << ik + 1 << endl;
			error = WF_Local::read_lowf_complex( this->WFC_K[ik], ik );
		}
#ifdef __MPI
		Parallel_Common::bcast_int(error);
#endif
		ofs_running << " Error=" << error << endl;
		if(error==1)
		{
			WARNING_QUIT("Local_Orbital_wfc","Can't find the wave function file: LOWF.dat");
		}
		else if(error==2)
		{
			WARNING_QUIT("Local_Orbital_wfc","In wave function file, band number doesn't match");
		}
		else if(error==3)
		{
			WARNING_QUIT("Local_Orbital_wfc","In wave function file, nlocal doesn't match");
		}
		else if(error==4)
		{
			WARNING_QUIT("Local_Orbital_wfc","In k-dependent wave function file, k point is not correct");
		}
	}
	else
	{
		WARNING_QUIT("Local_Orbital_wfc","check the parameter: start_wfc");
	}

	return;
}

//================================================================
// FUNCTION: 
// wave functions must be distributed.
// wave functions are used to construct the new charge density.
//================================================================
void Local_Orbital_wfc::aloc_gamma_wfc(const Grid_Technique &gt)
{
	TITLE("Local_Orbital_wfc","aloc_gamma_wfc");

//	if(DIAGO_TYPE=="selinv") return; //mohan add 2011-10-23

	if(NLOCAL < NBANDS)
	{
		WARNING_QUIT("Local_Orbital_wfc::allocate","NLOCAL<NBANDS");
	}

	// mohan add 2010-07-01
	if(allocate_flag)
	{
		if(BFIELD)
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int ib=0; ib<NBANDS; ib++)
				{
					delete[] this->WFC_GAMMA_B[is][ib];
				}
			}
		}
		else
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int ib=0; ib<NBANDS; ib++)
				{
					delete[] this->WFC_GAMMA[is][ib];
				}
			}
		}
		allocate_flag = false;
	}

	// mohan fix bug 2011-03-15
	static bool first = true;
	if(first)
	{
		if(BFIELD)
		{
			this->WFC_GAMMA_B = new complex<double>**[NSPIN];
			for(int is=0; is<NSPIN; is++)
			{
				this->WFC_GAMMA_B[is] = new complex<double>*[NBANDS];
			}
		}
		else
		{
			this->WFC_GAMMA = new double**[NSPIN];
			for(int is=0; is<NSPIN; is++)
			{
				this->WFC_GAMMA[is] = new double*[NBANDS];
			}
		}
		first=false;
	}
	
	// mohan add 2010-09-06
	if( gt.lgd != 0)
	{
    	// Create wave functions(Coefficients) in local basis.
    	// Same as evc in plane wave basis.
		if(BFIELD)
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int ib=0; ib<NBANDS; ib++)
				{
					this->WFC_GAMMA_B[is][ib] = new complex<double>[gt.lgd];
					ZEROS(this->WFC_GAMMA_B[is][ib],gt.lgd);
				}
				Memory::record("LocalOrbital_Coef","WFC_GAMMA_B",NSPIN*NBANDS*NLOCAL,"cdouble");
				allocate_flag = true;
			}
		}
		else
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int ib=0; ib<NBANDS; ib++)
				{
					this->WFC_GAMMA[is][ib] = new double[gt.lgd];
					ZEROS(this->WFC_GAMMA[is][ib],gt.lgd);
				}
				Memory::record("LocalOrbital_Coef","WFC_GAMMA",NSPIN*NBANDS*NLOCAL,"double");
				allocate_flag = true;
			}
		}
	}

	OUT(ofs_running,"start_wfc",wf.start_wfc);
	if(wf.start_wfc == "atomic" )
	{
		// Bfield
		if(BFIELD)
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int i=0; i<NBANDS; i++)
				{
					for(int j=0; j<NLOCAL; j++)
					{
						if(i==j)
						{
							const int mu = gt.trace_lo[j];
							if(mu>=0)
							{
								// be careful! std::rand must be translated to double!
								// c[i][j] = static_cast<double>( std::rand() ) / RAND_MAX ;

								// mohan modify 2010-07-02
								this->WFC_GAMMA_B[is][i][mu] = complex<double>(1.0, 0.0);
							}
						}
					}// j
				}// i
			}// is
		}
		// no Bfield
		else
		{
			for(int is=0; is<NSPIN; is++)
			{
				for(int i=0; i<NBANDS; i++)
				{
					for(int j=0; j<NLOCAL; j++)
					{
						if(i==j)
						{
							const int mu = gt.trace_lo[j];
							if(mu>=0)
							{
								// be careful! std::rand must be translated to double!
//								this->WFC_GAMMA[is][i][mu] = static_cast<double>( std::rand() ) / RAND_MAX ;

								// mohan modify 2010-07-02
								this->WFC_GAMMA[is][i][mu] = 1.0;
							}
						}
					}// j
				}// i
			}// is
		}
//		cout << " set LCAO wave fuctions to unit." << endl;
	}
	else if(wf.start_wfc == "file")
	{
		int error;
		if(BFIELD)
		{
			cout << " not implemented read_lowf for B field yet." << endl;
			WARNING_QUIT("Local_Orbital_wfc::aloc_gamma_wfc","not implement yet");
		}
		else
		{
			for(int is=0; is<NSPIN; ++is)
			{
				CURRENT_SPIN = is;
				
				// read in the wave functions from file and then
				// distribute it.
				error = WF_Local::read_lowf( this->WFC_GAMMA[is] );
			}
		}
#ifdef __MPI
		Parallel_Common::bcast_int(error);
#endif
		ofs_running << " Error=" << error << endl;
		if(error==1)
		{
			WARNING_QUIT("Local_Orbital_wfc","Can't find the wave function file: LOWF.dat");
		}
		else if(error==2)
		{
			WARNING_QUIT("Local_Orbital_wfc","In wave function file, band number doesn't match");
		}
		else if(error==3)
		{
			WARNING_QUIT("Local_Orbital_wfc","In wave function file, nlocal doesn't match");
		}
	}
	else
	{
		WARNING_QUIT("Local_Orbital_wfc","check the parameter: start_wfc");
	}


	return;
}

void Local_Orbital_wfc::set_trace_aug(const Grid_Technique &gt)
{
	TITLE("Local_Orbital_wfc","set_trace_aug");
	timer::tick("Local_Orbital_wfc","set_trace_aug",'D');
	// this function must be called after ParaO.trace_loc_row
	// , ParaO.trace_loc_col and GridT.trace_lo have been called.

	//xiaohui add 'OUT_LEVEL' line, 2015-09-16
	if(OUT_LEVEL != "m") ofs_running << "\n SETUP ARRAY FOR EXTRA WAVE FUNCTIONS" << endl;

	bool* occ2d = new bool[NLOCAL];
	for(int i=0; i<NLOCAL; i++)
	{
		occ2d[i] = false;
	}

	//------------------------------
	// 2d parallel of H, S matrix.
	// because in some terms of force,
	// we need to adapted to this. 
	//------------------------------
	for(int i=0; i<NLOCAL; i++)
	{
		const int mu = ParaO.trace_loc_row[i];
		const int nu = ParaO.trace_loc_col[i];
		if(mu>=0 || nu>=0)
		{
			occ2d[i] = true;
		}
	}

	//(1) init dimension of c_aug
	this->daug = 0;
	
	//(2) global positions of elementes in c_aug
	delete[] trace_aug;
	trace_aug = new int[NLOCAL];
	for(int i=0; i<NLOCAL; i++)
	{
		// this -1 is important.
		trace_aug[i] = -1;
		if(occ2d[i])
		{
			if(gt.trace_lo[i]<0) 
			{
				this->trace_aug[i] = daug;
//				ofs_running << " report daug " << setw(5) << i << setw(5) << daug << endl;
				++daug;
			}
		}
	}

	delete[] occ2d;

	//---------------------------------
	//second part: prepare for c_aug.
	//---------------------------------
	// mohan add 2010-09-26
	//OUT(ofs_running,"allocate_aug_flag",allocate_aug_flag);
	
	// mohan fix bug 2011-03-03
	static bool first = true;
	if(first)
	{
		if(GAMMA_ONLY_LOCAL)
		{
			this->WFC_GAMMA_aug = new double**[NSPIN];
			for(int is=0; is<NSPIN; ++is)
			{
				this->WFC_GAMMA_aug[is] = new double*[NBANDS];
			}
		}
		else //mohan add 2012-01-08
		{
			this->WFC_K_aug = new complex<double>**[kv.nks];
			for(int ik=0; ik<kv.nks; ++ik)
			{
				this->WFC_K_aug[ik] = new complex<double>*[NBANDS];
			}
		}
		first=false;
	}	
	
	if(allocate_aug_flag)
	{
		if(GAMMA_ONLY_LOCAL)
		{
			for(int is=0; is<NSPIN; ++is)
			{
				for(int i=0; i<NBANDS; ++i)
				{
					delete[] WFC_GAMMA_aug[is][i];
				}
			}
		}
		else //mohan add 2012-01-08
		{
			for(int ik=0; ik<kv.nks; ++ik)
			{
				for(int i=0; i<NBANDS; ++i)
				{
					delete[] WFC_K_aug[ik][i];
				}
			}
		}
		allocate_aug_flag = false;
	}

	// mohan add 2010-09-26
	if(daug != 0)
	{
		//------------------------------------------------------
    	// Create wave functions(Coefficients) in local basis.
    	// Same as evc in plane wave basis.
		//------------------------------------------------------
		if(GAMMA_ONLY_LOCAL)
		{
			for(int is=0; is<NSPIN; ++is)
			{	
				for(int i=0; i<NBANDS; ++i)
				{
					this->WFC_GAMMA_aug[is][i] = new double[daug];
					ZEROS(this->WFC_GAMMA_aug[is][i], daug);
				}
			}
			Memory::record("LocalOrbital_Coef","WFC_GAMMA_aug",NSPIN*NBANDS*daug,"double");
		}
		else // mohan add 2012-01-08
		{
			for(int ik=0; ik<kv.nks; ++ik)
			{
				for(int i=0; i<NBANDS; ++i)
				{
					this->WFC_K_aug[ik][i] = new complex<double>[daug];
					ZEROS(this->WFC_K_aug[ik][i], daug);
				}
			}
		}
		allocate_aug_flag = true;
	}

	if(OUT_LEVEL != "m") OUT(ofs_running,"daug",daug);

	timer::tick("Local_Orbital_wfc","set_trace_aug",'D');
	return; 
}

void Local_Orbital_wfc::init_Cij( const bool change_c )
{
	TITLE("Local_Orbial_Coef", "init_Cij");
	timer::tick("Local_Orbital_wfc","init_Cij");

	OUT(ofs_running,"init S matrix",UHM.init_s);
	if(!UHM.init_s)
	{
		WARNING_QUIT("Local_Orbital_wfc","not yet init S matrix");
	}

	double *psi = new double[NLOCAL];
	double *sc = new double[NLOCAL];

	// if the trial wave function is not orthogonal,
	// try new set.
	// try_new_wave_function_again:

	// output s matrix.
	/*
	for(int i=0; i<NLOCAL; i++)
	{
		for(int j=0; j<NLOCAL; j++)
		{
			if(j%8==0) ofs_running << endl;
			ofs_running << "\n" << setw(15) << LM.Sloc[i*ParaO.nloc+j];
		}
		ofs_running << endl;
	}
	*/

	ofs_running << " " << setw(8) << "States" << setw(20) << "BeforeNormalize" << setw(20) << "AfterNormalize" << endl;
	for(int is=0; is<NSPIN; is++)
	{	
		for(int m=0; m<NBANDS; m++)
		{

			// get mth Bloch band.
			//ofs_running << "\n" << m;
			for(int j=0; j<NLOCAL; j++)
			{
				psi[j] = this->WFC_GAMMA[is][m][j];
				//	if(j%8==0) ofs_running << endl;
				//	ofs_running << setw(15) << psi[j];
			}

			//ofs_running << endl;

			// calculate S|psi>
			//hm.s_1psi_real(NLOCAL, psi, sc);

			// calculate <psi|S|psi>
			double norm = 0.0;
			for(int j=0; j<NLOCAL; j++)
			{
				norm += psi[j] * sc[j];
				//	if(j%8==0) ofs_running << endl;
				//	ofs_running << setw(15) << sc[j];
			}

			//ofs_running << endl;

			assert( norm > 0.0 );
			norm = sqrt( norm );

			ofs_running << " " << setw(8) << m+1 << setw(20) << norm;
			// if we want to make sure the c orthogonal,
			// we need <psi_i|S|psi_j>=delta_{ij}
			if(change_c)
			{
				// normalize using <c|s|c>.
				for(int j=0; j<NLOCAL; j++)
				{
					psi[j] /= norm;
				}
				for (int j=0; j<NLOCAL; j++)
				{
					this->WFC_GAMMA[is][m][j] = psi[j];
				}

				// check the norm again.
				//hm.s_1psi_real(NLOCAL, psi, sc); // sphi = S|psi(m)>

				norm = 0.0;
				for(int j=0; j<NLOCAL; j++)
				{
					norm += psi[j] * sc[j];
				}
				assert( norm > 0.0 );
				norm = sqrt( norm );
				ofs_running << setw(20) << norm << endl;

			}
		}
	}

	if(!change_c) return;

	for(int is=0; is<NSPIN; is++)
	{
		int which_band = -1;
		//xiaohui modified 2013-04-24
		//which_band = this->check_orthogonal(psi, sc, is);

		if(which_band == -1)
		{
			OUT(ofs_running,"all bands orthogonal", "true");
		}
		else
		{
			OUT(ofs_running,"not orthogonal band",which_band);
			BLOCK_HERE("orthogonal");
		}
	}

	delete[] psi;
	delete[] sc;
	timer::tick("Local_Orbital_wfc","init_Cij");
	return;
}

/* xiaohui modified 2013-04-24
int Local_Orbital_wfc::check_orthogonal(double *psi, double *sc, const int &is)
{
	Diago_CG_Real::reduce_type = 1;
	
	// make cij orthogonal
	double *tmpc = new double[NLOCAL];
	for(int m=0; m<NBANDS; m++)
	{
		//cout << "\n\n m = " << m;
		for(int j=0; j<NLOCAL; j++)
		{
			psi[j] = this->WFC_GAMMA[is][m][j];
		}
		//hm.s_1psi_real(NLOCAL, psi, sc);

		Diago_CG_Real::schmit_orth(NLOCAL, m, WFC_GAMMA[is], sc, psi);
		for(int j=0; j<NLOCAL; j++)
		{
			this->WFC_GAMMA[is][m][j] = psi[j];
		}
		// check orthogonality between m and k < m.
		for(int k=0; k<m; k++)
        {
            double overlap = 0.0;
            for(int j=0; j<NLOCAL; j++)
            {
                tmpc[j] = this->WFC_GAMMA[is][k][j];
            }
            for(int j=0; j<NLOCAL; j++)
            {
                overlap += tmpc[j] * sc[j] ;
            }
            if( overlap > 1.0e-5 )
            {
                cout << "\n overlap with k = " << k << "  overlap = " << overlap;
				return m;
            }
        }
    }

	delete[] tmpc;

	// return -1 means all bands are orthogonal.
	return -1;
}
*/
