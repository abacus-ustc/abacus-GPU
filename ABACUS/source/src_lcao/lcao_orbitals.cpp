#include "lcao_orbitals.h"
#include "../src_pw/global.h" // only use ucell.atoms[it]
#include <cstring>		// Peize Lin fix bug about strcmp 2016-08-02

//==============================
// Define a object here! 
//==============================
LCAO_Orbitals ORB;

LCAO_Orbitals::LCAO_Orbitals()
{
	this->nchimax = 0;// this initialzied must specified
	this->Phi = new Numerical_Orbital[1];	
	this->Beta = new Numerical_Nonlocal[1];
	this->Vna = new Neutral_Pot[1];

	this->nproj = new int[1];
	this->nprojmax = 0;
	this->read_in_flag = false;	

	this->dr_uniform = 0.001;
}
LCAO_Orbitals::~LCAO_Orbitals()
{
	if(test_deconstructor)
	{
		cout << " ~LCAO_Orbitals()" << endl;
	}
	delete[] Phi;
	delete[] Beta;
	delete[] Vna;
	delete[] nproj;
}

#ifdef __MPI
// be called in unitcell_pseudo.
void LCAO_Orbitals::bcast_files(void)
{
	TITLE("LCAO_Orbitals","bcast_files");

	// 'read_in_flag' is true when there is a
	// block "NUMERICAL_ORBITAL" in structure
	// file.
	Parallel_Common::bcast_bool(read_in_flag);
	if(!read_in_flag)
	{
		return;
	}

	assert(ucell.ntype > 0 );

	ofs_running << "\n READING ORBITAL FILE NAMES FOR LCAO" << endl;
	for(int it=0; it<ucell.ntype; it++)
	{
		string ofile;
		string nfile;

		if(MY_RANK==0)
		{
			ofile = orbital_file[it];
			//-----------------------------------
			// Turn off the read in NONLOCAL file
			// function since 2013-08-02 by mohan
			//-----------------------------------
//			nfile = nonlocal_file[it];
		}

		Parallel_Common::bcast_string(ofile);
		//-----------------------------------
		// Turn off the read in NONLOCAL file
		// function since 2013-08-02 by mohan
		//-----------------------------------
//		Parallel_Common::bcast_string(nfile);

		if(MY_RANK!=0)
		{
			orbital_file.push_back( ofile );
			//-----------------------------------
			// Turn off the read in NONLOCAL file
			// function since 2013-08-02 by mohan
			//-----------------------------------
//			nonlocal_file.push_back ( nfile );
		}

		ofs_running << " " << ucell.atoms[it].label << " orbital file: " << orbital_file[it] << endl;
//		ofs_running << " " << ucell.atoms[it].label << " nonlocal file: " << nonlocal_file[it] << endl;
	}
	return;
}
#endif

void LCAO_Orbitals::Read_Orbitals(void)
{
	TITLE("LCAO_Orbitals", "Read_Orbitals");
	timer::tick("LCAO_Orbitals","Read_Orbitals",'C');

	ofs_running << "\n\n\n\n";
	ofs_running << " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
	ofs_running << " |                                                                    |" << endl;
	ofs_running << " | Setup numerical orbitals:                                          |" << endl;
	ofs_running << " | This part setup: numerical atomic orbitals, non-local projectors   |" << endl;
	ofs_running << " | and neutral potential (1D). The atomic orbitals information        |" << endl;
	ofs_running << " | including the radius, angular momentum and zeta number.            |" << endl;
	ofs_running << " | The neutral potential is the sum of local part of pseudopotential  |" << endl;
	ofs_running << " | and potential given by atomic charge, they will cancel out beyond  |" << endl;
	ofs_running << " | a certain radius cutoff, because the Z/r character.                |" << endl;
	ofs_running << " |                                                                    |" << endl;
	ofs_running << " <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
	ofs_running << "\n\n\n\n";	



	//--------------------------
	//(1) check dk, dR, Rmax.
	//--------------------------

	ofs_running << "\n SETUP ONE DIMENSIONAL ORBITALS/POTENTIAL" << endl;

	if(!read_in_flag)
	{
		WARNING_QUIT("LCAO_Orbitals::Read_Orbitals","Set the NUMERICAL_ORBITAL block in structure file.");
	}


	//OUT(ofs_running,"ecutwfc for kmesh",ecutwfc);
	OUT(ofs_running,"delta k  (1/Bohr)",dk);
	OUT(ofs_running,"delta r    (Bohr)",dR);
	OUT(ofs_running,"dr_uniform (Bohr)",dr_uniform);
	OUT(ofs_running,"rmax       (Bohr)",Rmax);

	// check the read in data.
    assert(dk > 0.0);
    assert(ecutwfc > 0.0);
    assert(dR > 0.0);
    assert(Rmax > 0.0);

	this->ntype = ucell.ntype;
	this->lmax = ucell.lmax;
	for(int i=0; i<ucell.ntype; i++)
	{
		OUT(ofs_running,"atom label",ucell.atoms[i].label);
	}


	//-------------------------------------------------
	//(2) set the kmesh according to ecutwfc and dk. 
	//-------------------------------------------------

	//-----------------------------------------------------------------
	// calculate number of k mesh according to energy cutoff.
	// Mohan choose ecutwfc according to interpolation requirement.
	//	cout << " ecutwfc=" << ecutwfc << endl;
	//LiuXh modified 2016-01-25, 2016-07-20
	if(ecutwfc< 20)
	{
		this->kmesh = static_cast<int>( 2 * sqrt(ecutwfc) / dk )  + 4;
	}
	else
	{
		this->kmesh = static_cast<int>( sqrt(ecutwfc) / dk )  + 4;
	}
	//	this->kmesh = static_cast<int> (PI / 0.01 / 4 / this->dk);
	if(kmesh%2==0) kmesh++;
	OUT(ofs_running,"kmesh",kmesh);
	//-----------------------------------------------------------------











	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//~~~~~~~~~~~~~~~~~~~~~~   1    ~~~~~~~~~~~~~~~~~~~~~~~~~
	// Read in numerical atomic orbitals for each atom type.
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	delete[] this->Phi;
	this->Phi = new Numerical_Orbital[ucell.ntype];
	for(int it=0; it<ucell.ntype; it++)
	{
		this->Read_PAO(it);	
	}











	
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//~~~~~~~~~~~~~~~~~~~~~~   2    ~~~~~~~~~~~~~~~~~~~~~~~~~
	// Read in non-local projector for each atom type.
	// In fact this should be improved,
	// the non-local projector should be transferred
	// from .UPF file directly.
	// mohan note 2011-03-04
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	delete[] this->Beta;
	this->Beta = new Numerical_Nonlocal[ucell.ntype];

	delete[] nproj;
	this->nproj = new int[ucell.ntype];
	ZEROS(nproj, ucell.ntype);
	
	this->nprojmax = 0;
	

	// if true: read in the nonlocal file from file.
	// if false: get nonlocal information from .upf or .vwr directly
	bool readin_nonlocal = false;

	for(int it=0; it<ucell.ntype; it++)
	{
		if(readin_nonlocal)
		{
			this->Read_NonLocal(it, this->nproj[it]);	
		}
		else
		{
			this->Set_NonLocal(it, this->nproj[it]);
		}
		this->nprojmax = std::max( this->nprojmax, this->nproj[it] );
	}

	this->set_nl_index();
	ofs_running << " max number of nonlocal projetors among all species is " << nprojmax << endl; 









	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//~~~~~~~~~~~~~~~~~~~~~~   3    ~~~~~~~~~~~~~~~~~~~~~~~~~
	// Setup one dimensional neutral potential for each
	// element. Vna = Vl_pseudo + Vh_atomic
	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



	delete[] this->Vna;
	this->Vna = new Neutral_Pot[ucell.ntype];

	for(int it=0; it<ucell.ntype; it++)
	{
		Vna[it].setup_Vna(it);
		Vna[it].uniform_Vna(it, dr_uniform);

		if(VNA==-1)
		{
			Vna[it].init_proj( this->Phi[it], dr_uniform );
		}
	}










	timer::tick("LCAO_Orbitals","Read_Orbitals",'C');
	return;
}



// mohan add 2013-08-02
// In order to get rid of the read in file .NONLOCAL.
void LCAO_Orbitals::Set_NonLocal(const int &it, int &n_projectors)
{
	TITLE("LCAO_Orbitals","Set_NonLocal");

	// set a pointer
	Atom* atom = &ucell.atoms[it];

	// get the number of non-local projectors
	n_projectors = atom->nbeta;
	const int N_PROJECTORS = atom->nh;//zhengdy-soc
//cout << " number of projectros " << N_PROJECTORS << endl;
//	cout << " number of projectros " << n_projectors << endl;

	// set the nonlocal projector objects
	Numerical_Nonlocal_Lm* tmpBeta_lm = new Numerical_Nonlocal_Lm[n_projectors];

	//const int nproj_allowed = atom->lmax + 1;	
	//matrix Coefficient_D_in(nproj_allowed, nproj_allowed); //LiuXh 2016-01-14
	matrix Coefficient_D_in(n_projectors, n_projectors); //LiuXh 2016-01-14
	ComplexMatrix Coefficient_D_in_so(N_PROJECTORS*2, N_PROJECTORS*2);//zhengdy-soc

	if(!atom->has_so)
	{
		for(int p1 = 0; p1<n_projectors; p1++)
		{
			const int lnow = atom->lll[p1];

		// this will be wrong if dion is non-diagoal
		//Coefficient_D_in(lnow,lnow)=atom->dion(p1,p1);//LiuXh 2016-01-14
			Coefficient_D_in(p1,p1)=atom->dion(p1,p1);//LiuXh 2016-01-14

		// only keep the nonzero part.
			int cut_mesh = atom->mesh; 
			for(int ir=atom->mesh-1; ir>=0; --ir)
			{
				if( abs( atom->betar(p1,ir) ) > 1.0e-10 )
				{
					cut_mesh = ir; 
					break;
				}
			}
			if(cut_mesh %2 == 0) ++cut_mesh;

//		cout << " cut_mesh=" << cut_mesh << endl;
			double* beta_r = new double[cut_mesh];
			ZEROS(beta_r, cut_mesh);
			for(int ir=0; ir<cut_mesh; ++ir)
			{
				beta_r[ir] = atom->betar(p1,ir);
			}

			tmpBeta_lm[p1].set_NL_proj(
					atom->label,
					it, //type
					lnow, // angular momentum L
					cut_mesh, // number of radial mesh
					atom->rab,
					atom->r, // radial mesh value (a.u.)
					beta_r,
					this->kmesh,
					this->dk,
					dr_uniform); // delta k mesh in reciprocal space

			delete[] beta_r;
				
		}
		
		//WARNING("LCAO_Orbitals::Set_NonLocal","bug in line "+TO_STRING(__LINE__)+", matrix ic>=nc");		// Peize Lin add 2019-01-23
		this->Beta[it].set_type_info(it, atom->label, atom->pp_type, atom->lmax, Coefficient_D_in, Coefficient_D_in_so, n_projectors, 0, atom->lll, tmpBeta_lm);//LiuXh 2016-01-14, 2016-07-19
	}
	else//added by zhengdy-soc
	{
		int lmaxkb = - 1;
		for (int ibeta = 0; ibeta < ucell.atoms[it].nbeta; ibeta++)
		{
			lmaxkb = max( lmaxkb, ucell.atoms[it].lll[ibeta]);
		}
		soc.rot_ylm(lmaxkb);
		soc.fcoef.create(ucell.ntype, ucell.atoms[it].nh, ucell.atoms[it].nh);
		int ip1=0;
		for(int p1 = 0; p1<n_projectors; p1++)//nbeta
		{
			const int lnow = atom->lll[p1];
			
			const int l1 = atom->lll[p1];
			const double j1 = atom->jjj[p1];
			for(int m1=0; m1<2*l1+1; m1++)
			{
				int ip2=0;
				for(int p2 = 0; p2<n_projectors; p2++)
				{
					const int l2 = atom->lll[p2];
					const double j2 = atom->jjj[p2];
					for(int m2 = 0;m2<2*l2+1;m2++)
					{
						if(l1==l2 && fabs(j1-j2)<1e-7)
						{
							for(int is1=0;is1<2;is1++)
							{
								for(int is2=0;is2<2;is2++)
								{
									complex<double> coeff = complex<double>(0.0,0.0);
									for(int m=-l1-1;m<l1+1;m++)
									{
										const int mi = soc.sph_ind(l1,j1,m,is1) + lmaxkb ;
										const int mj = soc.sph_ind(l2,j2,m,is2) + lmaxkb ;
										coeff += soc.rotylm(m1,mi) * soc.spinor(l1,j1,m,is1)*
											conj(soc.rotylm(m2,mj))*soc.spinor(l2,j2,m,is2);
									}
									soc.fcoef(it,is1,is2,ip1,ip2) = coeff;
									Coefficient_D_in_so(ip1 + N_PROJECTORS*is1, ip2 + N_PROJECTORS*is2) = atom->dion(p1,p2) * soc.fcoef(it, is1, is2, ip1, ip2);
									if(p1 != p2) soc.fcoef(it, is1, is2, ip1, ip2) = complex<double>(0.0,0.0);
								}
							}
						}
						ip2++;
					}
				}
				assert(ip2==N_PROJECTORS);
				ip1++;
			}
		// only keep the nonzero part.
			int cut_mesh = atom->mesh; 
			for(int ir=atom->mesh-1; ir>=0; --ir)
			{
				if( abs( atom->betar(p1,ir) ) > 1.0e-10 )
				{
					cut_mesh = ir; 
					break;
				}
			}
			if(cut_mesh %2 == 0) ++cut_mesh;

//		cout << " cut_mesh=" << cut_mesh << endl;
			double* beta_r = new double[cut_mesh];
			ZEROS(beta_r, cut_mesh);
			for(int ir=0; ir<cut_mesh; ++ir)
			{
				beta_r[ir] = atom->betar(p1,ir);
			}

			tmpBeta_lm[p1].set_NL_proj(
					atom->label,
					it, //type
					lnow, // angular momentum L
					cut_mesh, // number of radial mesh
					atom->rab,
					atom->r, // radial mesh value (a.u.)
					beta_r,
					this->kmesh,
					this->dk,
					dr_uniform); // delta k mesh in reciprocal space

			delete[] beta_r;
		}
		assert(ip1==N_PROJECTORS);
		this->Beta[it].set_type_info(it, atom->label, atom->pp_type, atom->lmax, Coefficient_D_in, Coefficient_D_in_so, n_projectors, N_PROJECTORS, atom->lll, tmpBeta_lm);//zhengdy-soc 2018-09-10
	}//end if

	delete[] tmpBeta_lm;

	cout << " Set NonLocal Pseudopotential Projectors " << endl;


	return;
}


void LCAO_Orbitals::Read_NonLocal(const int &it, int &n_projectors)
{
	TITLE("LCAO_Orbitals","Read_NonLocal");

	ifstream ifs;

	// mohan add 2010-09-08.
	// check if the non-local pseudopotential file exist.
	bool open = false;	
	if(MY_RANK==0)
	{
		ifs.open( this->nonlocal_file[it].c_str() );
		if(ifs)
		{
			open = true;
		}
	}
#ifdef __MPI
	Parallel_Common::bcast_bool(open);
#endif
	if(!open)
	{
		ofs_warning << " Non-local File : " << nonlocal_file[it] << endl;
		WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","Can not find the NONLOCAL file.");
	}
	else
	{
//		ofs_running << " open nonLocal pseudopotential file: " << nonlocal_file[it] << endl;
	}


	string label;
	string ps_type;

	// maximal lmax allowed in this calculation
	int nlmax = 0;

	if(MY_RANK==0)
	{
		if(SCAN_BEGIN(ifs, "<HEADER>"))
		{
			READ_VALUE(ifs, label);
			READ_VALUE(ifs, ps_type);
			if(ps_type != "NC")
			{
				WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","Only available for NC nonlocal pseudopotential");
			}
			READ_VALUE(ifs, nlmax);
//			cout << " " << label << " " << ps_type << " " << nlmax << endl; 
			assert(nlmax >= -1);
			SCAN_END(ifs,"</HEADER>");
		}
	}

#ifdef __MPI
	Parallel_Common::bcast_string(label);
	Parallel_Common::bcast_string(ps_type);
	Parallel_Common::bcast_int(nlmax);
#endif

	//mohan add 2012-06-09
	if( nlmax != -1 )
	{
		bool find_lmax = false;
		for(int ic=0; ic<ucell.atoms[it].nbeta; ic++)
		{
			if( nlmax == ucell.atoms[it].lll[ic] )
			{
				//			cout << " nlmax = " << nlmax << endl;
				//			cout << " lchi = " << ucell.atoms[it].lll[ic] << endl;
				find_lmax = true;
				break;
			}
		}

		if( !find_lmax )
		{
			ofs_warning << " For element " << label << endl;
			ofs_warning << " Max L Read in from NONLOCAL = " << nlmax << endl;
			for(int ib=0; ib<ucell.atoms[it].nbeta; ++ib)
			{
				ofs_warning << " Max L Read in from pseudopotential file = " << ucell.atoms[it].lll[ib] << endl;
			}
			WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","nlmax != ucell.atoms[it].lll");
		}
	}


//	OUT(ofs_running,"Type",it);
	OUT(ofs_running,"label",label);
//	OUT(ofs_running,"ps_type",ps_type);
	OUT(ofs_running,"nlmax",nlmax);

	//-------------------------------------------
	// if each L has projectors more than once,
	// this needed to be modified.	
	//-------------------------------------------
	int nproj_allowed = nlmax+1;
	matrix Coefficient_D_in(nproj_allowed, nproj_allowed);
	ComplexMatrix Coefficient_D_in_so(nproj_allowed*2, nproj_allowed*2);

//	OUT(ofs_running,"nproj_allowed",nproj_allowed);

	if(MY_RANK==0)
	{
		if(SCAN_BEGIN(ifs, "<DIJ>"))
		{
			//--------------------------------------
			// this parameter is very important!!!
			//--------------------------------------
			READ_VALUE(ifs, n_projectors);
			OUT(ofs_running,"n_projectors",n_projectors);
			
			for (int p1 = 0; p1 < n_projectors; p1++)
        	{
            	for (int p2 = 0; p2 < n_projectors; p2++)
            	{
                	int L1_read, L2_read;
                	
					ifs >> L1_read >> L2_read;
					
					assert(L1_read <= nlmax);
					assert(L2_read <= nlmax);
                	
					ifs >> Coefficient_D_in(L1_read, L2_read);
					
//					ofs_running << " L1=" << L1_read << " L2=" << L2_read << " Coef=" << Coefficient_D_in(L1_read,L2_read) << endl;
            	}
        	}
			SCAN_END(ifs,"</DIJ>");
		}
	}

#ifdef __MPI
	Parallel_Common::bcast_int(n_projectors); // mohan add 2010-12-20
	Parallel_Common::bcast_double(Coefficient_D_in.c, Coefficient_D_in.nr * Coefficient_D_in.nc);
#endif

	Numerical_Nonlocal_Lm* tmpBeta_lm = new Numerical_Nonlocal_Lm[n_projectors];
	int* LfromBeta = new int[n_projectors];
	ZEROS(LfromBeta, n_projectors);

	for(int p1 = 0; p1<n_projectors; p1++)
	{
		int meshr_ps = 0;
		if(MY_RANK==0)
		{
			if(SCAN_BEGIN(ifs, "<PP_BETA>", 0))
			{
				int iproj;
				READ_VALUE(ifs, iproj);
				if(iproj!=p1)
				{
					cout << " iproj=" << iproj << " p1=" << p1 << endl;
					WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","Check non-local projector index.");
				}
				
				READ_VALUE(ifs, LfromBeta[p1]);
				assert( LfromBeta[p1] >= 0 );
				assert( LfromBeta[p1] <= nlmax );

				READ_VALUE(ifs, meshr_ps);
				if(meshr_ps%2==0)
				{
					cout << " meshr_ps = " << meshr_ps << endl;
					WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","meshr_ps must be odd!");
				}
			}
			else
			{
				WARNING_QUIT("LCAO_Orbitals::Read_NonLocal","<PP_BETA> doesn't match!");
			}
		}// end MY_RANK==0

//		OUT(ofs_running,"meshr_ps",meshr_ps);

#ifdef __MPI
		Parallel_Common::bcast_int(meshr_ps);
		Parallel_Common::bcast_int(LfromBeta[p1]);
#endif		

		double* radial_ps = new double[meshr_ps];
		double* rab_ps = new double[meshr_ps];
		double* beta_r = new double[meshr_ps];
		ZEROS(radial_ps, meshr_ps);
		ZEROS(rab_ps, meshr_ps);
		ZEROS(beta_r, meshr_ps);

		if(MY_RANK==0)
		{
            for (int ir = 0; ir < meshr_ps; ir++)
            {
               	ifs >> radial_ps[ir];
               	ifs >> beta_r[ir];
               	ifs >> rab_ps[ir];
            }
		}

#ifdef __MPI
		Parallel_Common::bcast_double(radial_ps, meshr_ps);
		Parallel_Common::bcast_double(beta_r, meshr_ps);
		Parallel_Common::bcast_double(rab_ps, meshr_ps);
#endif
			
//		OUT(ofs_running,"radial_ps max",radial_ps[meshr_ps-1]);

//		cout << this->kmesh << endl;
        tmpBeta_lm[p1].set_NL_proj(
        		label,
                it, //type
                LfromBeta[p1], //angular momentum L
                meshr_ps, // number of radial mesh
                rab_ps,
                radial_ps,// radial mesh value(a.u.)
                beta_r,
                this->kmesh,
                this->dk,
				dr_uniform); // delta k mesh in reciprocal space

		delete[] radial_ps;
		delete[] rab_ps;
		delete[] beta_r;
		
		if(MY_RANK==0)
		{
			SCAN_END(ifs,"</PP_BETA>");
		}
	}// end projectors.
	
	this->Beta[it].set_type_info(it, label, ps_type, nlmax, Coefficient_D_in, Coefficient_D_in_so, n_projectors, 0, LfromBeta, tmpBeta_lm);
		
	ifs.close();
	delete[] LfromBeta;
	delete[] tmpBeta_lm;

	return;
}


void LCAO_Orbitals::Read_PAO(const int& it)
{
	TITLE("LCAO_Orbitals","Read_PAO");
	int lmaxt = ucell.atoms[it].nwl;

//	OUT(ofs_running,"Lmax for this type",lmaxt);

	// allocate space
	// number of chi for each L.
	int *nchi = new int[lmaxt+1];
	for(int l=0; l<=lmaxt; l++)
	{
		nchi[l] = ucell.atoms[it].l_nchi[l];
		this->nchimax = std::max( this->nchimax, nchi[l]);
	}

	// calculate total number of chi
    int total_nchi = 0;
    for(int l=0; l<=lmaxt; l++)
    {
        total_nchi += nchi[l];
    }
//	OUT(ofs_running,"Total number of chi(l,n)",total_nchi);
	
	delete[] Phi[it].phiLN;
	this->Phi[it].phiLN = new Numerical_Orbital_Lm[total_nchi];

	ifstream in;

	int count=0;
	ofs_running << " " << setw(8) << "ORBITAL" << setw(3) << "L" 
	<< setw(3) << "N" << setw(8) << "nr" << setw(8) << "dr"
	<< setw(8) << "RCUT" << setw(12) << "CHECK_UNIT"
	<< setw(12) << "NEW_UNIT" << endl;
    for (int L=0; L<=lmaxt; L++)
    {
        for (int N=0; N<nchi[L]; N++)
        {
			ofs_running << " " << setw(8) << count+1 << setw(3) << L << setw(3) << N;

			// mohan add 2010-09-08.
			// check if the orbital file exists.
            bool open=false;
            if(MY_RANK==0)
            {
				//cout << " file name : " << orbital_file[it] << endl;
                in.open(this->orbital_file[it].c_str());
                if(in)
                {
                    open=true;
                }
            }
#ifdef __MPI
            Parallel_Common::bcast_bool( open );
#endif
            if(!open)
            {
                ofs_warning << " Orbital file : " << this->orbital_file[it] << endl;
                WARNING_QUIT("LCAO_Orbitals::Read_PAO","Couldn't find orbital files");
            }

			double* radial; // radial mesh
			double* psi; // radial local orbital
			double* psir;// psi * r
			double* rab;// dr
            int meshr; // number of mesh points
            char word[80];
			double dr; // only used in case 1

			int meshr_read;
			if(MY_RANK==0)    //pengfei 2014-10-13
			{
                                while (in.good())
                                {
                                    in >> word;
                                    if (std::strcmp(word , "END") == 0)		// Peize Lin fix bug about strcmp 2016-08-02
                                    {
                                        break;
                                    }
                                }

				CHECK_NAME(in, "Mesh");
				in >> meshr;
				meshr_read = meshr;
				if(meshr%2==0) 
				{
					++meshr;
				}
			}
				
#ifdef __MPI
			Parallel_Common::bcast_int( meshr );	
			Parallel_Common::bcast_int( meshr_read );	
#endif				
			if(MY_RANK==0)
			{
				CHECK_NAME(in, "dr");
				in >> dr;
			}
			
#ifdef __MPI
			Parallel_Common::bcast_double( dr );
#endif
			// set the number of mesh and the interval distance.
			ofs_running << setw(8) << meshr << setw(8) << dr;

            radial = new double[meshr];
			psi = new double[meshr];
            psir = new double[meshr];
            rab = new double[meshr];

			ZEROS( radial, meshr );
			ZEROS( psi, meshr);
			ZEROS( psir, meshr );
			ZEROS( rab, meshr );

			for(int ir=0; ir<meshr; ir++)
			{
				rab[ir] = dr;
				// plus one because we can't read in r = 0 term now.
				// change ir+1 to ir, because we need psi(r==0) information.
				radial[ir] = ir*dr; //mohan 2010-04-19
			}

			// set the length of orbital
			ofs_running << setw(8) << radial[meshr-1];

			// mohan update 2010-09-07
			bool find = false;
			if(MY_RANK==0)
			{
				string name1, name2, name3;
				int tmp_it, tmp_l ,tmp_n;
				while( !find )
				{
					if(in.eof())
					{
						ofs_warning << " Can't find l=" 
						<< L << " n=" << N << " orbital." << endl; 			
						break;
					}

					
					in >> name1 >> name2 >> name3;
					assert( name1 == "Type" );
					in >> tmp_it >> tmp_l >> tmp_n;
					if( L == tmp_l && N == tmp_n )
					{
						// meshr_read is different from meshr if meshr is even number.
						for(int ir=0; ir<meshr_read; ir++)
						{
							in >> psi[ir];
							/*
							double rl = pow (ir*dr, l);	
							psi[ir] *= rl;
							*/
							psir[ir] = psi[ir] * radial[ir];
						}
						find = true;
					}
					else
					{
						double no_use;
						for(int ir=0; ir<meshr_read; ir++)
						{
							in >> no_use;
						}
					}
				}//end find
			}

#ifdef __MPI
			Parallel_Common::bcast_bool(find);
#endif
			if(!find)
			{
				WARNING_QUIT("LCAO_Orbitals::Read_PAO","Can't find atomic orbitals.");
			}

#ifdef __MPI
			Parallel_Common::bcast_double( psi, meshr_read ); // mohan add 2010-06-24
			Parallel_Common::bcast_double( psir, meshr_read );
#endif

			// renormalize radial wave functions
			double* inner = new double[meshr];
			for(int ir=0; ir<meshr; ir++)
			{
				inner[ir] = psir[ir] * psir[ir];
			}
			double unit = 0.0;
			Mathzone::Simpson_Integral(meshr, inner, rab, unit);

			// check unit: \sum ( psi[r] * r )^2 = 1
			ofs_running << setprecision(3) << setw(12) << unit;

			for(int ir=0; ir<meshr; ir++)
			{
				psi[ir] /= sqrt(unit);
				psir[ir] /= sqrt(unit);
			}

			for(int ir=0; ir<meshr; ir++)
			{
				inner[ir] = psir[ir] * psir[ir];
			}
			Mathzone::Simpson_Integral(meshr, inner, rab, unit);
			delete[] inner;
			ofs_running << setw(12) << unit << endl;
			
			this->Phi[it].phiLN[count].set_orbital_info(
                ucell.atoms[it].label,
                it, //type
                L, //angular momentum L
                N, // number of orbitals of this L
                meshr, // number of radial mesh
                rab,
                radial,// radial mesh value(a.u.)
				Numerical_Orbital_Lm::Psi_Type::Psi,// psi type next
                psi, // radial wave function
                this->kmesh,
                this->dk,
				this->dr_uniform,
				true,
				true); // delta k mesh in reciprocal space

            delete[] radial;
            delete[] rab;
            delete[] psir;
			delete[] psi;

            ++count;
			in.close();
        }
    }

    this->Phi[it].set_orbital_info(
        it, // type
        ucell.atoms[it].label, // label
        lmaxt,
        nchi,
        total_nchi); //copy twice !

    delete[] nchi;
    return;
}

void LCAO_Orbitals::set_nl_index(void)
{
	TITLE("LCAO_Orbitals","set_nl_index");
	int ntype = ucell.ntype;

	this->nkb=0;
	for(int it=0; it<ntype; it++)
	{
		nkb += ucell.atoms[it].na * ucell.atoms[it].nh;
//		cout << " projectors for " << ucell.atoms[it].label << " is " << ucell.atoms[it].nh << endl;
	}

	// mohan update 2011-05-01
	if(nkb==0)
	{
		WARNING("LCAO_Orbitals","No non-local projectos, it must all be H atoms.");
		return;
	}
	

	this->itiaib2ib_all.create(ntype, ucell.namax, this->nkb);

	int ib_all = 0;
	for(int it=0; it<ucell.ntype; it++)
	{
		for(int ia=0; ia<ucell.atoms[it].na; ia++)
		{
			for(int ib=0; ib<ucell.atoms[it].nh; ib++)
			{
				itiaib2ib_all(it,ia,ib) = ib_all;
				++ib_all;
			}
			/*
			for(int ib=0; ib< this->nproj[it]; ib++)
			{
				for(int m=0; m< 2*Beta[it].Proj[ib].getL()+1; m++)
				{
					itiaib2ib_all(it,ia,ib) = ib_all;
					++ib_all;
				}
			}
			*/	
		}
	}
	assert(ib_all==nkb);


	int nh_max = 0;
	for(int it=0; it<ucell.ntype; it++)
	{
		nh_max = max(nh_max, ucell.atoms[it].nh);
	}

	this->ib2_ylm.create(ntype, nh_max);
	for(int it=0; it<ucell.ntype; it++)
	{
		int index = 0;
		for(int ib=0; ib< this->nproj[it]; ib++)
		{
			const int L = Beta[it].Proj[ib].getL();
			for(int m=0; m<2*L+1; m++)
			{
				this->ib2_ylm(it, index) = L*L+m;
				++index;
			}
		}
	}

	return;
}
