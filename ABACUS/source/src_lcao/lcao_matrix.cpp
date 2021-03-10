#include "lcao_matrix.h"
#include "global_fp.h"

LCAO_Matrix::LCAO_Matrix()
{
	// for gamma_only
	Sloc = new double[1];
	Hloc_fixed = new double[1];
	Hloc = new double[1];
	Sdiag = new double[1];

	// for many k points
	Sloc2 = new complex<double>[1];
	Hloc_fixed2 = new complex<double>[1];
	Hloc2 = new complex<double>[1];
	Sdiag2 = new complex<double>[1];
}

LCAO_Matrix::~LCAO_Matrix()
{
 	// delete matrix for gamma_only.
    delete[] Sloc;
    delete[] Hloc_fixed;
    delete[] Hloc;
    delete[] Sdiag;

    // delete matrix for many k points
    delete[] Sloc2;
    delete[] Hloc_fixed2;
    delete[] Hloc2;	
    delete[] Sdiag2;
}

void LCAO_Matrix::divide_HS_in_frag(void)
{
	TITLE("LCAO_Matrix","divide_HS_in_frag");

	ofs_running << "\n SETUP THE DIVISION OF H/S MATRIX" << endl;
	
	// (1) calculate nrow, ncol, nloc.
	//if (DIAGO_TYPE=="hpseps" || DIAGO_TYPE=="scalpack" || DIAGO_TYPE=="selinv") xiaohui modify 2013-09-02
	if (KS_SOLVER=="genelpa" || KS_SOLVER=="hpseps" || KS_SOLVER=="scalpack" || KS_SOLVER=="selinv") //xiaohui add 2013-09-02
	{
		ofs_running << " divide the H&S matrix using 2D block algorithms." << endl;
#ifdef __MPI
		ParaO.divide_HS_2d(DIAG_WORLD);
#else
		WARNING_QUIT("LCAO_Matrix::init","diago_type = 'HPSEPS' is not available for series version.\n You can use 'LAPACK' instead.");
#endif
	}
    	//else if (DIAGO_TYPE == "canonical"
        //    	|| DIAGO_TYPE == "trace_resetting"
        //    	|| DIAGO_TYPE == "trace_correcting") xiaohui modify 2013-09-02
	else if(KS_SOLVER == "canonical"
		|| KS_SOLVER == "trace_resetting"
		|| KS_SOLVER == "trace_correcting") //xiaohui add 2013-09-02
	{
		ofs_running << " divide the H&S matrix according to atoms." << endl;
		// done nothing.
		// the nloc is calculated in "ParaA.set_trace"
	}
	else
	{
		ParaO.nloc = NLOCAL * NLOCAL;
	}


	// (2) set the trace, then we 
	// can calculate the nnr.
	// for 2d: calculate ParaO.nloc first, then trace_loc_row and trace_loc_col
	// for O(N): calculate the three together.
	ParaO.set_trace();



	// (3) allocate matrix.
	if(GAMMA_ONLY_LOCAL)
	{
		if(BFIELD)
		{
			// if b field is chosen,
			// the hamiltonian is complex<double>.
			allocate_HS_k(ParaO.nloc);	
		}
		else
		{
			allocate_HS_gamma(ParaO.nloc);
		}
	}
	else
	{
		allocate_HS_k(ParaO.nloc);
	}


	return;
}

void LCAO_Matrix::allocate_HS_gamma(const int &nloc)
{
	TITLE("LCAO_Matrix","allocate_HS_gamma");

	OUT(ofs_running,"nloc",nloc);
	if(nloc==0) return; //mohan fix bug 2012-05-25

	delete[] Sloc;
	delete[] Hloc_fixed;
	delete[] Hloc;
	delete[] Sdiag;

	this->Sloc = new double[nloc];
	this->Hloc_fixed = new double[nloc];
	this->Hloc = new double[nloc];
	this->Sdiag = new double[nloc];

	ZEROS(Sloc,nloc);
	ZEROS(Hloc_fixed,nloc);
	ZEROS(Hloc,nloc);

	return;
}


void LCAO_Matrix::allocate_HS_k(const int &nloc)
{
	TITLE("LCAO_Matrix","allocate_HS_k");

	OUT(ofs_running,"nloc",nloc);
	if(nloc==0) return; //mohan fix bug 2012-05-25

	delete[] Sloc2;
	delete[] Hloc_fixed2;
	delete[] Hloc2;
	delete[] Sdiag2;

	this->Sloc2 = new complex<double>[nloc];
	this->Hloc_fixed2 = new complex<double>[nloc];
	this->Hloc2 = new complex<double>[nloc];
	this->Sdiag2 = new complex<double>[nloc];

	ZEROS(Sloc2,nloc);
	ZEROS(Hloc_fixed2,nloc);
	ZEROS(Hloc2,nloc);
	
	return;
}

void LCAO_Matrix::allocate_HS_R(const int &nnR)
{
	if(!NONCOLIN)
	{
		delete[] HlocR;
		delete[] SlocR;
		delete[] Hloc_fixedR;	
	
		this->HlocR = new double[nnR];
		this->SlocR = new double[nnR];
		this->Hloc_fixedR = new double[nnR];

		ZEROS(HlocR, nnR);
		ZEROS(SlocR, nnR);
		ZEROS(Hloc_fixedR, nnR);
	}
	else
	{
		delete[] HlocR_soc;
		delete[] SlocR_soc;
		delete[] Hloc_fixedR_soc;

		this->HlocR_soc = new complex<double>[nnR];
		this->SlocR_soc = new complex<double>[nnR];
		this->Hloc_fixedR_soc = new complex<double>[nnR];
		
		ZEROS(HlocR_soc, nnR);
		ZEROS(SlocR_soc, nnR);
		ZEROS(Hloc_fixedR_soc, nnR);
		
	}

	return;
}

void LCAO_Matrix::set_HSgamma(const int &iw1_all, const int &iw2_all, const double &v, const char &dtype)
{
    // use iw1_all and iw2_all to set Hloc
    // becareful! The ir and ic may < 0!!!!!!!!!!!!!!!!
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];
    //const int index = ir * ParaO.ncol + ic;
	int index;
	if(KS_SOLVER=="genelpa")  // save the matrix as column major format
	{
		index=ic*ParaO.nrow+ir;
	}
	else
	{
		index=ir*ParaO.ncol+ic;
  	}
   
   	if( index >= ParaO.nloc)
	{
		cout << " iw1_all = " << iw1_all << endl;
		cout << " iw2_all = " << iw2_all << endl;
		cout << " ir = " << ir << endl;
		cout << " ic = " << ic << endl;
		cout << " index = " << index << endl;
		cout << " ParaO.nloc = " << ParaO.nloc << endl;
		WARNING_QUIT("LCAO_Matrix","set_HSgamma");
	}	 

	// S : S matrix element.
	// T : T matrix element.
	// N : nonlocal H matrix element.
	// L : local H matrix element.

    if (dtype=='S')
    {
        this->Sloc[index] += v;
    }
    else if (dtype=='T' || dtype=='N')
    {
        this->Hloc_fixed[index] += v;
    }
    else if (dtype=='L')
    {
        this->Hloc[index] += v;
    }

    return;
}

void LCAO_Matrix::set_HSk(const int &iw1_all, const int &iw2_all, const complex<double> &v, const char &dtype, const int spin)
{
    // use iw1_all and iw2_all to set Hloc
    // becareful! The ir and ic may < 0!!!!!!!!!!!!!!!!
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];
    //const int index = ir * ParaO.ncol + ic;
	int index;
	if(KS_SOLVER=="genelpa")  // save the matrix as column major format
	{
		index=ic*ParaO.nrow+ir;
	}
	else
	{
		index=ir*ParaO.ncol+ic;
  	}
    assert(index < ParaO.nloc);
	if (dtype=='S')//overlap Hamiltonian.
	{
		this->Sloc2[index] += v;
	}
	else if (dtype=='T' || dtype=='N')// kinetic and nonlocal Hamiltonian.
	{
		this->Hloc_fixed2[index] += v; // because kinetic and nonlocal Hamiltonian matrices are already block-cycle staraged after caculated in lcao_nnr.cpp
                                      // this statement will not be used.
	}
	else if (dtype=='L') // Local potential Hamiltonian.
	{
		this->Hloc2[index] += v;
	}
	else
	{
		WARNING_QUIT("LCAO_Matrix","set_HSk");
	}

    return;
}

void LCAO_Matrix::set_force
(
    const int &iw1_all,
    const int &iw2_all,
    const double& vx,
    const double& vy,
    const double& vz,
    const char &dtype)
{
    // use iw1_all and iw2_all to set Hloc
    // becareful! The ir and ic may < 0!!!!!!!!!!!!!!!!
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];
    const int index = ir * ParaO.ncol + ic;
	
	if( index >= ParaO.nloc)
	{
		cout << " iw1_all = " << iw1_all << endl;
		cout << " iw2_all = " << iw2_all << endl;
		cout << " ir = " << ir << endl;
		cout << " ic = " << ic << endl;
		cout << " index = " << index << endl;
		cout << " ParaO.nloc = " << ParaO.nloc << endl;
		WARNING_QUIT("LCAO_Matrix","set_force");
	}	 

    if (dtype == 'S')
    {
        this->DSloc_x[index] += vx;
        this->DSloc_y[index] += vy;
        this->DSloc_z[index] += vz;
    }
    else if (dtype == 'T')
    {
		// notice, the sign is '-', minus.
        this->DHloc_fixed_x[index] -= vx;
        this->DHloc_fixed_y[index] -= vy;
        this->DHloc_fixed_z[index] -= vz;
    }
    else if (dtype == 'N')
	{
		this->DHloc_fixed_x[index] += vx;
		this->DHloc_fixed_y[index] += vy;
		this->DHloc_fixed_z[index] += vz;
	}

	return;
}

void LCAO_Matrix::set_stress
(
    const int &iw1_all,
    const int &iw2_all,
    const double& vx,
    const double& vy,
    const double& vz,
    const char &dtype,
    const Vector3<double> &dtau)
{
    // use iw1_all and iw2_all to set Hloc
    // becareful! The ir and ic may < 0!!!!!!!!!!!!!!!!
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];
    const int index = ir * ParaO.ncol + ic;

	if( index >= ParaO.nloc)
	{
		cout << " iw1_all = " << iw1_all << endl;
		cout << " iw2_all = " << iw2_all << endl;
		cout << " ir = " << ir << endl;
		cout << " ic = " << ic << endl;
		cout << " index = " << index << endl;
		cout << " ParaO.nloc = " << ParaO.nloc << endl;
		WARNING_QUIT("LCAO_Matrix","set_stress");
	}

	if (dtype == 'S')
	{
		this->DSloc_11[index] += vx * dtau.x;
		this->DSloc_12[index] += vx * dtau.y;
		this->DSloc_13[index] += vx * dtau.z;
		this->DSloc_22[index] += vy * dtau.y;
		this->DSloc_23[index] += vy * dtau.z;
		this->DSloc_33[index] += vz * dtau.z;
	}
	else if (dtype == 'T')
	{
		// notice, the sign is '-', minus.
		this->DHloc_fixed_11[index] -= vx * dtau.x;
		this->DHloc_fixed_12[index] -= vx * dtau.y;
		this->DHloc_fixed_13[index] -= vx * dtau.z;
		this->DHloc_fixed_22[index] -= vy * dtau.y;
		this->DHloc_fixed_23[index] -= vy * dtau.z;
		this->DHloc_fixed_33[index] -= vz * dtau.z;
	}
	else if (dtype == 'N')
	{
		this->DHloc_fixed_11[index] += vx * dtau.x;
		this->DHloc_fixed_12[index] += vx * dtau.y;
		this->DHloc_fixed_13[index] += vx * dtau.z;
		this->DHloc_fixed_22[index] += vy * dtau.y;
		this->DHloc_fixed_23[index] += vy * dtau.z;
		this->DHloc_fixed_33[index] += vz * dtau.z;
	}

	return;
}

void LCAO_Matrix::zeros_HSgamma(const char &mtype)
{
	if (mtype=='S') ZEROS(Sloc,ParaO.nloc);
	else if (mtype=='T') ZEROS(Hloc_fixed,ParaO.nloc);
	else if (mtype=='H') ZEROS(Hloc,ParaO.nloc);
	return;
}

void LCAO_Matrix::zeros_HSk(const char &mtype)
{
	if (mtype=='S') ZEROS(Sloc2,ParaO.nloc);
	else if (mtype=='T') ZEROS(Hloc_fixed2,ParaO.nloc);
	else if (mtype=='H') ZEROS(Hloc2,ParaO.nloc);
	return;
}

void LCAO_Matrix::zeros_HSR(const char &mtype, const int &nnr)
{
	if(!NONCOLIN)
	{
		if (mtype=='S') ZEROS(SlocR, nnr);
		else if (mtype=='T') ZEROS(Hloc_fixedR, nnr);
		else if (mtype=='H') ZEROS(HlocR, nnr);
	}
	else
	{
		if (mtype=='H') ZEROS(this->HlocR_soc, nnr);
		else if (mtype=='S') ZEROS(this->SlocR_soc, nnr);
		else if (mtype=='T') ZEROS(this->Hloc_fixedR_soc, nnr);
	}
	return;
}

void LCAO_Matrix::print_HSk(const char &mtype, const char &vtype, const double &accuracy)
{
	TITLE("LCAO_Matrix","print_HSk");
	if(mtype=='S') cout << "Sloc2 matrix" << endl;
	else if(mtype=='T') cout << "Hloc_fixed2 matrix" << endl;
	else if(mtype=='H') cout << "Hloc matrix" << endl;
	else
	{
		WARNING_QUIT("LCAO_Matrix::print_HSk","Check input parameter: mtype.");
	}

	if(vtype=='C') cout << " Output norm."  << endl;
	else if(vtype=='R') cout << " Output real part."  << endl;
	else if(vtype=='I') cout << " Output imag part."  << endl;


	cout << setprecision(8) << endl;
	for(int i=0; i<ParaO.nrow; i++)
	{
		cout << " " ;
		for(int j=0; j<ParaO.ncol; j++)
		{
			const int index = i * ParaO.ncol + j;
			double v=-888.888;//wrong number
			if(vtype=='R')
			{
				if(mtype=='S') v = Sloc2[index].real();
				else if(mtype=='T') v = Hloc_fixed2[index].real();
				else if(mtype=='H') v = Hloc2[index].real();
			}
			else if(vtype=='C')
			{
				if(mtype=='S') v = sqrt( norm ( Sloc2[index] ) );
				else if(mtype=='T') v = sqrt( norm ( Hloc_fixed2[index] ) );
				else if(mtype=='H') v = sqrt( norm ( Hloc2[index] ) );
			}
			else if(vtype=='I')
			{
				if(mtype=='S') v = Sloc2[index].imag();
				else if(mtype=='T') v = Hloc_fixed2[index].imag();
				else if(mtype=='H') v = Hloc2[index].imag();
			}
			if( abs(v) > accuracy )
			{
				cout << setw(15) << v;
			}
			else
			{
				cout << setw(15) << "0"; 
			}
		}
		cout << endl;
	}
	cout << endl;
	cout << setprecision(6) << endl;
	return;
}


void LCAO_Matrix::print_HSgamma(const char &mtype)
{
	TITLE("Parallel_Orbitals","print_HSgamma");

	ofs_running << " " << mtype << " matrix" << endl;
	ofs_running << " nrow=" << ParaO.nrow << endl;
	ofs_running << " ncol=" << ParaO.ncol << endl;
	ofs_running << " element number = " << ParaO.ncol << endl;
	if (mtype=='S')
	{
		if(!BFIELD)
		{
			cout << setprecision(8);
			cout << " print Sloc" << endl;
			for(int i=0; i<NLOCAL; ++i)
			{
				for(int j=0; j<NLOCAL; ++j)
				{
					double v = Sloc[i*ParaO.ncol+j];
					if( abs(v) > 1.0e-8)
					{
						cout << setw(15) << v;
					}
					else
					{
						cout << setw(15) << "0";
					}
				}//end j
				cout << endl;
			}//end i
		}

		/*
		   ofs_running << " " << setw(10) << "LocalRow" << setw(10) << "LocalCol"
		   << setw(10) << "GlobalRow" << setw(10) << "GloablCol"
		   << setw(10) << "Sloc" << endl;
		   for (int i=0; i<this->nrow; i++)
		   {
		   for (int j=0; j<this->ncol; j++)
		   {
		   if ( abs(Sloc[i * this->ncol + j]) > 1.0e-5 )
		   ofs_running << " " << setw(10) << i << setw(10) << j
		   << setw(10) << MatrixInfo.row_set[i] << setw(10) << MatrixInfo.col_set[j]
		   << setw(10) << Sloc[i * this->ncol + j] << endl;
		   }
		   }
		cout << "\n Smatrix" << endl;
		//ofs_running << setprecision(5) << endl;

		for(int i=0; i<ParaO.nrow; i++)
		{
			for(int j=0; j<ParaO.ncol; j++)
			{
				double s = Sloc[ i * ParaO.ncol + j];
				double h = Hloc[ i * ParaO.ncol + j];
				
				if( abs( h - s ) > 1.0e-3 )
				{
				//	ofs_running << setw(5) << i+1 << setw(5) << j+1 << setw(12) << h << setw(12) << s << endl;
				}
				
				
				if( abs(s) > 1.0e-5 )
				{
					ofs_running << setw(10) << s;
				//	ofs_running << setw(5) << i << setw(5) << j << setw(15) << s << endl;

				}
				else
				{
					ofs_running << setw(10) << "0";
				}
			}
			ofs_running << endl;
		}
		 */
	}
	if (mtype=='T')
	{


		/*
		ofs_running << " " << setw(10) << "LocalRow" << setw(10) << "LocalCol"
			<< setw(10) << "GlobalRow" << setw(10) << "GloablCol"
			<< setw(10) << "Hloc_fixed" << endl;
		for (int i=0; i<ParaO.nrow; i++)
		{
			for (int j=0; j<ParaO.ncol; j++)
			{
//				if ( abs(Hloc_fixed[i * ParaO.ncol + j]) > 1.0e-5 )
				{
					ofs_running << " " << setw(10) << i+1 << setw(10) << j+1;

					//mohan fix bug 2012-02-22
					if(ATOM_DISTRIBUTION!=1)
					{
						ofs_running << setw(10) << ParaO.MatrixInfo.row_set[i] << setw(10) << ParaO.MatrixInfo.col_set[j];
					}
					else
					{
						ofs_running << setw(10) << "0" << setw(10) << "0";
					}
					ofs_running << setw(10) << Hloc_fixed[i * ParaO.ncol + j] << endl;
				}
			}
		}
		for(int i=0; i<ParaO.nrow; i++)
		{
			for(int j=0; j<ParaO.ncol; j++)
			{
				double v = Hloc_fixed[i*ParaO.ncol+j];
				if( abs(v) > 1.0e-5 )
				{
					ofs_running << setw(15) << v;
				}
				else
				{
					ofs_running << setw(15) << "0";
				}
			}
			ofs_running << endl;
		}
		*/

		if(!BFIELD)
		{
			cout << " print Hloc_fixed" << endl;
			for(int i=0; i<NLOCAL; ++i)
			{
				for(int j=0; j<NLOCAL; ++j)
				{
					double v = Hloc_fixed[i*ParaO.ncol+j];
					if( abs(v) > 1.0e-8)
					{
						cout << setw(15) << v;
					}
					else
					{
						cout << setw(15) << "0";
					}
				}//end j
				cout << endl;
			}//end i
		}
	}
	if (mtype=='H')
	{

		if(!BFIELD)
		{
			cout << " print Hloc" << endl;
			for(int i=0; i<NLOCAL; ++i)
			{
				for(int j=0; j<NLOCAL; ++j)
				{
					double v = Hloc[i*ParaO.ncol+j];
					if( abs(v) > 1.0e-8)
					{
						cout << setw(15) << v;
					}
					else
					{
						cout << setw(15) << "0";
					}
				}//end j
				cout << endl;
			}//end i
		}

	/*
		ofs_running << " " << setw(10) << "LocalRow" << setw(10) << "LocalCol"
			<< setw(10) << "GlobalRow" << setw(10) << "GloablCol"
			<< setw(10) << "Hloc" << endl;

		ofs_running << "\n Hmatrix" << endl;
		ofs_running << setprecision(8) << endl;
		for (int i=0; i<ParaO.nrow; i++)
		{
			ofs_running << " ";
			for (int j=0; j<ParaO.ncol; j++)
			{
				double a = Hloc[ i * ParaO.ncol + j];
				if ( abs( a ) > 1.0e-5 )
				{
				//	ofs_running << " " << setw(10) << i << setw(10) << j
				//		<< setw(10) << ParaO.MatrixInfo.row_set[i] << setw(10) << ParaO.MatrixInfo.col_set[j]
				//		<< setw(10) << a << endl;
					
					//
					ofs_running << setw(15) << a;
					//ofs_running << setw(5) << i << setw(5) << j << setw(15) << a << endl;

				}
				else
				{
					//
					ofs_running << setw(15) << "0";
				}
			}
			//
			ofs_running << endl;
		}
		*/
	}

	return;
}

// becareful! Update Hloc, we add new members to it.
void LCAO_Matrix::update_Hloc(void)
{
	for (int i=0; i<ParaO.nloc; i++)
	{
		Hloc[i] += Hloc_fixed[i];
	}
	return;
}

void LCAO_Matrix::update_Hloc2(void)
{
	for (int i=0; i<ParaO.nloc; i++)
	{
		Hloc2[i] += Hloc_fixed2[i];
	}
	return;
}


void LCAO_Matrix::output_HSk(const char &mtype, string &fn)
{
	TITLE("LCAO_Matrix","output_HSk");
	stringstream ss;
	ss << global_out_dir << fn;
	ofstream ofs(ss.str().c_str());
	ofs << NLOCAL << endl;
	for(int i=0; i<NLOCAL; i++)
	{
		for(int j=0; j<NLOCAL; j++)
		{	
			const int index = i * NLOCAL + j;
			if(mtype=='S') ofs << Sloc2[index].real() << " " << Sloc2[index].imag() << endl;
			else if(mtype=='T') ofs << Hloc_fixed2[index].real() << " " << Hloc_fixed2[index].imag() << endl;
			else if(mtype=='H') ofs << Hloc2[index].real() << " " << Hloc2[index].imag() << endl;
		}
	}
	ofs.close();
	return;
}

void LCAO_Matrix::allocate_Hloc_fixedR_tr(void)
{
    TITLE("LCAO_Matrix","allocate_Hloc_fixedR_tr");

    //int R_x = 10;
    //int R_y = 10;
    //int R_z = 10;
    int R_x = GridD.getCellX();
    int R_y = GridD.getCellY();
    int R_z = GridD.getCellZ();

    if(!NONCOLIN)
    {
        Hloc_fixedR_tr = new double***[R_x];
        //HR_tr = new double***[R_x];
        //SlocR_tr = new double***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            Hloc_fixedR_tr[ix] = new double**[R_y];
            //HR_tr[ix] = new double**[R_y];
            //SlocR_tr[ix] = new double**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                Hloc_fixedR_tr[ix][iy] = new double*[R_z];
                //HR_tr[ix][iy] = new double*[R_z];
                //SlocR_tr[ix][iy] = new double*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    Hloc_fixedR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    //HR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    //SlocR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    ZEROS(Hloc_fixedR_tr[ix][iy][iz], ParaO.nloc);
                    //ZEROS(HR_tr[ix][iy][iz], ParaO.nloc);
                    //ZEROS(SlocR_tr[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }
    else
    {
        Hloc_fixedR_tr_soc = new complex<double>***[R_x];
        //HR_tr = new double***[R_x];
        //SlocR_tr = new double***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            Hloc_fixedR_tr_soc[ix] = new complex<double>**[R_y];
            //HR_tr[ix] = new double**[R_y];
            //SlocR_tr[ix] = new double**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                Hloc_fixedR_tr_soc[ix][iy] = new complex<double>*[R_z];
                //HR_tr[ix][iy] = new double*[R_z];
                //SlocR_tr[ix][iy] = new double*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    Hloc_fixedR_tr_soc[ix][iy][iz] = new complex<double>[ParaO.nloc];
                    //HR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    //SlocR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    ZEROS(Hloc_fixedR_tr_soc[ix][iy][iz], ParaO.nloc);
                    //ZEROS(HR_tr[ix][iy][iz], ParaO.nloc);
                    //ZEROS(SlocR_tr[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }
//cout<<"R_x: "<<R_x<<endl;
//cout<<"R_y: "<<R_y<<endl;
//cout<<"R_z: "<<R_z<<endl;
//cout<<"ParaO.nloc: "<<ParaO.nloc<<endl;
//cout<<"SlocR_tr 1-3-3-27: "<<SlocR_tr[1][3][3][27]<<endl;
//cout<<"Hloc_fixedR_tr 1-3-3-27: "<<Hloc_fixedR_tr[1][3][3][27]<<endl;

    return;
}

void LCAO_Matrix::allocate_HR_tr(void)
{
    TITLE("LCAO_Matrix","allocate_HR_tr");

    //int R_x = 10;
    //int R_y = 10;
    //int R_z = 10;
    int R_x = GridD.getCellX();
    int R_y = GridD.getCellY();
    int R_z = GridD.getCellZ();

    if(!NONCOLIN)
    {
        HR_tr = new double***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            HR_tr[ix] = new double**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                HR_tr[ix][iy] = new double*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    HR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    ZEROS(HR_tr[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }
    else
    {
        HR_tr_soc = new complex<double>***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            HR_tr_soc[ix] = new complex<double>**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                HR_tr_soc[ix][iy] = new complex<double>*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    HR_tr_soc[ix][iy][iz] = new complex<double>[ParaO.nloc];
                    ZEROS(HR_tr_soc[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }

    return;
}

void LCAO_Matrix::allocate_SlocR_tr(void)
{
    TITLE("LCAO_Matrix","allocate_SlocR_tr");

    //int R_x = 10;
    //int R_y = 10;
    //int R_z = 10;
    int R_x = GridD.getCellX();
    int R_y = GridD.getCellY();
    int R_z = GridD.getCellZ();

    if(!NONCOLIN)
    {
        SlocR_tr = new double***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            SlocR_tr[ix] = new double**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                SlocR_tr[ix][iy] = new double*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    SlocR_tr[ix][iy][iz] = new double[ParaO.nloc];
                    ZEROS(SlocR_tr[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }
    else
    {
        SlocR_tr_soc = new complex<double>***[R_x];
        for(int ix=0; ix<R_x; ix++)
        {
            SlocR_tr_soc[ix] = new complex<double>**[R_y];
            for(int iy=0; iy<R_y; iy++)
            {
                SlocR_tr_soc[ix][iy] = new complex<double>*[R_z];
                for(int iz=0; iz<R_z; iz++)
                {
                    SlocR_tr_soc[ix][iy][iz] = new complex<double>[ParaO.nloc];
                    ZEROS(SlocR_tr_soc[ix][iy][iz], ParaO.nloc);
                }
            }
        }
    }

    return;
}

void LCAO_Matrix::destroy_Hloc_fixedR_tr(void)
{
    TITLE("LCAO_Matrix","destroy_Hloc_fixed2_R");

    //int R_x = 10;
    //int R_y = 10;
    //int R_z = 10;
    int R_x = GridD.getCellX();
    int R_y = GridD.getCellY();
    int R_z = GridD.getCellZ();

    if(!NONCOLIN)
    {
        for(int ix=0; ix<R_x; ix++)
        {
            for(int iy=0; iy<R_y; iy++)
            {
                for(int iz=0; iz<R_z; iz++)
                {
                    delete[] Hloc_fixedR_tr[ix][iy][iz];
                    delete[] HR_tr[ix][iy][iz];
                    delete[] SlocR_tr[ix][iy][iz];
                }
                delete[] Hloc_fixedR_tr[ix][iy];
                delete[] HR_tr[ix][iy];
                delete[] SlocR_tr[ix][iy];
            }
            delete[] Hloc_fixedR_tr[ix];
            delete[] HR_tr[ix];
            delete[] SlocR_tr[ix];
        }
        delete[] Hloc_fixedR_tr;
        delete[] HR_tr;
        delete[] SlocR_tr;
    }
    else
    {
        for(int ix=0; ix<R_x; ix++)
        {
            for(int iy=0; iy<R_y; iy++)
            {
                for(int iz=0; iz<R_z; iz++)
                {
                    delete[] Hloc_fixedR_tr_soc[ix][iy][iz];
                    delete[] HR_tr_soc[ix][iy][iz];
                    delete[] SlocR_tr_soc[ix][iy][iz];
                }
                delete[] Hloc_fixedR_tr_soc[ix][iy];
                delete[] HR_tr_soc[ix][iy];
                delete[] SlocR_tr_soc[ix][iy];
            }
            delete[] Hloc_fixedR_tr_soc[ix];
            delete[] HR_tr_soc[ix];
            delete[] SlocR_tr_soc[ix];
        }
        delete[] Hloc_fixedR_tr_soc;
        delete[] HR_tr_soc;
        delete[] SlocR_tr_soc;
    }

    return;
}

void LCAO_Matrix::set_HR_tr(const int &Rx, const int &Ry, const int &Rz, const int &iw1_all, const int &iw2_all, const double &v)
{
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];

//cout<<"ir: "<<ir<<endl;
//cout<<"ic: "<<ic<<endl;
    int index;
    if(KS_SOLVER=="genelpa")
    {
        index=ic*ParaO.nrow+ir;
//cout<<"index: "<<index<<endl;
    }
    else
    {
        index=ir*ParaO.ncol+ic;
//cout<<"index: "<<index<<endl;
    }

//cout<<"ParaO.nloc: "<<ParaO.nloc<<endl;
    assert(index < ParaO.nloc);
//cout<<"Rx: "<<Rx<<endl;
//cout<<"Ry: "<<Ry<<endl;
//cout<<"Rz: "<<Rz<<endl;
//cout<<"Hloc_fixedR_tr: "<<Hloc_fixedR_tr[Rx][Ry][Rz][index]<<endl;
//cout<<"v: "<<v<<endl;
    HR_tr[Rx][Ry][Rz][index] = Hloc_fixedR_tr[Rx][Ry][Rz][index] + v; 
    //HR_tr[Rx][Ry][Rz][index] = Hloc_fixedR_tr[Rx][Ry][Rz][index]; 
    //HR_tr[Rx][Ry][Rz][index] = v; 
    //HR_tr[Rx][Ry][Rz][index] = index; 

    return;
}

//LiuXh add 2019-07-16
void LCAO_Matrix::set_HR_tr_soc(const int &Rx, const int &Ry, const int &Rz, const int &iw1_all, const int &iw2_all, const complex<double> &v)
{
    const int ir = ParaO.trace_loc_row[ iw1_all ];
    const int ic = ParaO.trace_loc_col[ iw2_all ];

//cout<<"ir: "<<ir<<endl;
//cout<<"ic: "<<ic<<endl;
    int index;
    if(KS_SOLVER=="genelpa")
    {
        index=ic*ParaO.nrow+ir;
//cout<<"index: "<<index<<endl;
    }
    else
    {
        index=ir*ParaO.ncol+ic;
//cout<<"index: "<<index<<endl;
    }

//cout<<"ParaO.nloc: "<<ParaO.nloc<<endl;
    assert(index < ParaO.nloc);
//cout<<"Rx: "<<Rx<<endl;
//cout<<"Ry: "<<Ry<<endl;
//cout<<"Rz: "<<Rz<<endl;
//cout<<"Hloc_fixedR_tr: "<<Hloc_fixedR_tr[Rx][Ry][Rz][index]<<endl;
//cout<<"v: "<<v<<endl;
    HR_tr_soc[Rx][Ry][Rz][index] = Hloc_fixedR_tr_soc[Rx][Ry][Rz][index] + v; 
    //HR_tr[Rx][Ry][Rz][index] = Hloc_fixedR_tr[Rx][Ry][Rz][index]; 
    //HR_tr[Rx][Ry][Rz][index] = v; 
    //HR_tr[Rx][Ry][Rz][index] = index; 

    return;
}

