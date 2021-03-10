/* hermit.cpp file */
#include "tools.h"
#include "global.h"
#include "algorithms.h"
#include "hamilt_pw.h"
#include "../src_external/src_pdiag/blas_interface.h"
#include "optical.h" // only get judgement to calculate optical matrix or not.

#ifdef __EPM
#include "../src_epm/nscf_epm.h"
#endif

int Hamilt_PW::moved = 0;

Hamilt_PW::Hamilt_PW()
{
    hpsi = new complex<double>[1];
    spsi = new complex<double>[1];
    GR_index = new int[1];

#ifndef __EPM
    Bec = new complex<double>[1];
    Ps = new complex<double>[1];
#else
    GR_index2 = new int[1];
#endif
}

Hamilt_PW::~Hamilt_PW()
{
	if(test_deconstructor)
	{
		cout << " ~Hamilt_PW()" << endl;
	}
    delete[] hpsi;
    delete[] spsi;
    delete[] GR_index;
#ifndef __EPM
    delete[] Bec;
    delete[] Ps;
#else
    delete[] GR_index2;
#endif
}

#ifdef __EPM
void Hamilt_PW::init()
{
	TITLE("Hamilt_PW","init");
	const int nrxxs = pw.nrxxs;

	delete[] hpsi;
	delete[] spsi;
	delete[] GR_index;
	delete[] GR_index2;

	this->hpsi = new complex<double>[wf.npwx];
	this->spsi = new complex<double>[wf.npwx];
	this->GR_index = new int[nrxxs];
	this->GR_index2= new int[nrxxs];

	OUT(ofs_running,"hpsi allocation = ","Done");
	OUT(ofs_running,"spsi allocation = ","Done");
	OUT(ofs_running,"GR_index allocation = ","Done");
	OUT(ofs_running,"GR_index2 allocation = ","Done");

	ZEROS(hpsi,wf.npwx);
	ZEROS(spsi,wf.npwx);
	ZEROS(GR_index,wf.npwx);
	ZEROS(GR_index2,wf.npwx);

	return;
}
#else
void Hamilt_PW::init(void)
{
    if (test_hm) TITLE("Hamilt_PW","init");

    delete[] hpsi;
    delete[] spsi;
    delete[] GR_index;
    delete[] Bec;
    delete[] Ps;

    this->hpsi = new complex<double> [wf.npwx * NPOL];
    this->spsi = new complex<double> [wf.npwx * NPOL];
    this->GR_index = new int[pw.nrxxs];
    this->Bec = new complex<double> [ppcell.nkb];
    this->Ps  = new complex<double> [ppcell.nkb * NPOL];

    ZEROS(this->hpsi,wf.npwx * NPOL);
    ZEROS(this->spsi,wf.npwx * NPOL);
    ZEROS(this->GR_index, pw.nrxxs);
//    ofs_running << "\n Hamiltonian allocate done."<<endl;
    return;
}
#endif

void Hamilt_PW::init_k(const int ik)
{
    if (test_hm==1) TITLE("Hamilt_PW","init_k");
	
	// mohan add 2010-09-30
	// (1) Which spin to use.
	if(NSPIN==2)CURRENT_SPIN = kv.isk[ik];
	// (2) Kinetic energy.
	wf.ekin(ik);
	// (3) Take the local potential.
	for (int ir=0; ir<pw.nrxx; ir++)
	{
		pot.vrs1[ir] = pot.vrs(CURRENT_SPIN, ir);//mohan add 2007-11-12
	}
	// (4) Calculate nonlocal pseudopotential vkb
	//if (ppcell.nkb > 0 && !LINEAR_SCALING) xiaohui modify 2013-09-02
	if(ppcell.nkb > 0 && (BASIS_TYPE=="pw" || BASIS_TYPE=="lcao_in_pw")) //xiaohui add 2013-09-02. Attention...
	{
		ppcell.getvnl(ik);
	}
	// (5) The number of wave functions.
	wf.npw = kv.ngk[ik];
	// (6) The index of plane waves.
    for (int ig = 0;ig < wf.npw;ig++)
    {
        this->GR_index[ig] = pw.ig2fftw[ wf.igk(ik, ig) ];
    }

#ifdef __EPM
    for ( int ig=0; ig< wf.npw;ig++)
    {
        this->GR_index2[ig] = pw.ig2ngg[ wf.igk(ik, ig) ];
    }
#endif
    return;
}


// from cinitcgg.f90
// Hamiltonian diagonalization in the subspace spanned
// by nstart states psi (atomic or random wavefunctions).
// Produces on output n_band eigenvectors (n_band <= nstart) in evc.
void Hamilt_PW::cinitcgg(
    const int ik,
    const int nstart,
    const int n_band,
    const ComplexMatrix &psi,
    ComplexMatrix &evc,
    double *en)
{
    if (test_hm==1) TITLE("Hamilt_PW","cinitcgg");
    timer::tick("Hamilt_PW","cinitcgg",'G');

	assert(nstart!=0);
	assert(n_band!=0);

    ComplexMatrix hc(nstart, nstart);
    ComplexMatrix sc(nstart, nstart);
    ComplexMatrix hvec(nstart,n_band);

	complex<double> **p = new complex<double>*[nstart];
	for(int i=0; i<nstart; i++)
	{
		if(NPOL==2)
		p[i] = new complex<double>[wf.npwx*NPOL];
		else p[i] = new complex<double>[wf.npwx];
	}

	for(int i=0; i<nstart; i++)
	{
		for(int j=0; j<wf.npwx; j++)
		{
			p[i][j] = psi(i,j);
		}
	}
	if(NPOL == 2){
		for(int i=0; i<nstart; i++)
		{
			for(int j=0; j<wf.npwx; j++)
			{
				p[i][j+wf.npwx] = psi(i,j+wf.npwx);
			}
		}
	}

	//int incx = 1;
	//int incy = 1; 
    // Set up the Hamiltonian and Overlap matrix
//    timer::tick("Hamilt_PW","construct_HS");
	const int npw = kv.ngk[ik];
	int dmin,dmax;
	if(!NONCOLIN)
	{
		dmin= npw;
		dmax = wf.npwx;
	}
	else {
		dmin = wf.npwx*NPOL;
		dmax = wf.npwx*NPOL;
	}
	complex<double>* hpsi = new complex<double>[dmin];
	complex<double>* spsi = new complex<double>[dmin];

    for (int m=0; m<nstart; m++)
    {
        this->h_1psi(dmin, p[m], hpsi, spsi);

        hc(m,m) = this->just_ddot(dmin, p[m], hpsi);
        sc(m,m) = this->just_ddot(dmin, p[m], spsi);

        if (m+1<nstart)
        {
	//		timer::tick("Hamilt_PW","zdotc");
            for (int j = m + 1;j < nstart;j++)
            {
                hc(j,m) = this->just_ddot(dmin, p[j], hpsi);
                sc(j,m) = this->just_ddot(dmin, p[j], spsi);
                hc(m, j) = conj(hc(j, m));
                sc(m, j) = conj(sc(j, m));
            } // enddo
	//		timer::tick("Hamilt_PW","zdotc");
        }
    } // enddo
	
	delete[] hpsi;
	delete[] spsi;

  //  timer::tick("Hamilt_PW","construct_HS");

//	timer::tick("Hamilt_PW","reduce");
	if(NPROC_IN_POOL>1)
	{
		Parallel_Reduce::reduce_complex_double_pool( hc.c, nstart*nstart );
		Parallel_Reduce::reduce_complex_double_pool( sc.c, nstart*nstart );
	}
//	timer::tick("Hamilt_PW","reduce");

	for(int i=0; i<nstart; i++)
	{
		delete[] p[i];
	}
	delete[] p;

    hm.cdiaghg(nstart, n_band, hc, sc, nstart, en, hvec);

	//cout << "\n <2>  pw.gcar[0] = " << " " << pw.gcar[0].x << " " << pw.gcar[0].y << " " << pw.gcar[0].z << endl; 
    //=======================
    //diagonize the H-matrix
    //=======================

    if (test_hm > 2)
    {
		cout << setprecision(3);
		out.printV3(ofs_running,kv.kvec_c[ik]);
		out.printcm_norm("sc",sc,1.0e-4);
		out.printcm_norm("hvec",hvec,1.0e-4);
		out.printcm_norm("hc",hc,1.0e-4);
		cout << endl;
    }
//	cout << " ----------------------------- ik=" << ik << endl;
//	out.printV3( kv.kvec_c[ik] );
//	cout << " cinitcgg" << endl;
	cout << setprecision(5);

//--------------------------
// KEEP THIS BLOCK FOR TESTS
//--------------------------
/*
	cout << "  hc matrix" << endl;
	for(int i=0; i<NLOCAL; i++)
	{
		for(int j=0; j<NLOCAL; j++)
		{
			double a = hc(i,j).real();
			if(abs(a) < 1.0e-5) a = 0;
			cout << setw(6) << a;
		}
		cout << endl;
	}

	cout << "  sc matrix" << endl;
	for(int i=0; i<NLOCAL; i++)
	{
		for(int j=0; j<NLOCAL; j++)
		{
			double a = sc(i,j).real();
			if(abs(a) < 1.0e-5) a = 0;
			cout << setw(6) << a;
		}
		cout << endl;
	}

	cout << "\n Band Energy" << endl;
	for(int i=0; i<NBANDS; i++)
	{
		cout << " e[" << i+1 << "]=" << en[i] * Ry_to_eV << endl;
	}
*/
//--------------------------
// KEEP THIS BLOCK FOR TESTS
//--------------------------

//	cout << "OUTPUT Overlap Matrix = " << setprecision(6);

	//if(LOCAL_BASIS==4 && CALCULATION=="nscf" && !Optical::opt_epsilon2) xiaohui modify 2013-09-02
	if((BASIS_TYPE=="lcao" || BASIS_TYPE=="lcao_in_pw") && CALCULATION=="nscf" && !Optical::opt_epsilon2) //xiaohui add 2013-09-02. Attention!
	{
		ofs_running << " Not do zgemm to get evc." << endl;
	}
	//else if(LOCAL_BASIS==4 && ( CALCULATION == "scf" || CALCULATION == "md") ) xiaohui modify 2013-09-02
	else if((BASIS_TYPE=="lcao" || BASIS_TYPE=="lcao_in_pw") && ( CALCULATION == "scf" || CALCULATION == "md" || CALCULATION == "relax")) //pengfei 2014-10-13
	{
		// because psi and evc is different here,
		// I think if psi and evc is the same, 
		// there may be problems, mohan 2011-01-01
		char transa = 'N';
		char transb = 'T';
		zgemm_( &transa,
				&transb,
				&dmax, // m: row of A,C
				&n_band, // n: col of B,C
				&nstart, // k: col of A, row of B
				&ONE, // alpha
				psi.c, // A
				&dmax, // LDA: if(N) max(1,m) if(T) max(1,k)
				hvec.c, // B
				&n_band, // LDB: if(N) max(1,k) if(T) max(1,n)
				&ZERO,  // belta
				evc.c, // C
				&dmax ); // LDC: if(N) max(1, m)
	}
	else
	{
		/* As the evc and psi may refer to the same matrix, we first
		 * create a temporary matrix to story the result. (by wangjp) */
		ComplexMatrix evctmp(n_band, dmax);
		for(int ib=0; ib<n_band; ib++)
		{
			for(int ig=0; ig<wf.npw; ig++)
			{
				complex<double> sum = ZERO;
				complex<double> sum1 = ZERO;
				for(int ib2=0; ib2<nstart; ib2++)
				{
					sum += psi(ib2, ig) * hvec(ib2, ib);
					if(NPOL == 2) {
						sum1 +=  psi(ib2,ig + wf.npwx) * hvec(ib2, ib);
					}
				}
				evctmp(ib,ig) = sum;
				if(NPOL == 2){
					evctmp(ib,ig + wf.npwx) = sum1;
				}
			}
		}
		for(int ib=0; ib<n_band; ib++)
		{
			for(int ig=0; ig<wf.npw; ig++)
			{
				evc(ib,ig) = evctmp(ib,ig);
				if(NPOL==2) evc(ib, ig + wf.npwx) = evctmp(ib, ig+wf.npwx);
			}
		}

		/*
		for(int ib=0; ib<n_band; ib++)
		{
			for(int ig=0; ig<wf.npw; ig++)
			{
				complex<double> diff = evctmp(ib, ig) - evc(ib, ig);
				if( norm(diff) > 1.0e-5 )
				{
					cout << " npw = " << wf.npw << endl;
					cout << " nstart = " << nstart << endl;
					cout << " nbands = " << n_band << endl;
					cout << " wf.npwx = " << wf.npwx << endl;
					cout << " ib=" << ib << " ig=" << ig << " diff=" << diff << endl;
				}
			}
		}
		*/

	}
    //out.printr1_d("en",en,n_band);

//	cout << "\n bands" << endl;
//	for(int ib=0; ib<n_band; ib++)
//	{
//		cout << " ib=" << ib << " " << en[ib] * Ry_to_eV << endl; 
//	}

    //out.printcm_norm("hvec",hvec,1.0e-8);

    timer::tick("Hamilt_PW","cinitcgg",'G');

	//cout << "\n <3>  pw.gcar[0] = " << " " << pw.gcar[0].x << " " << pw.gcar[0].y << " " << pw.gcar[0].z << endl; 
    return;
} //end subroutine cinitcgg

void Hamilt_PW::h_1psi( const int npw_in, const complex < double> *psi,
                        complex<double> *hpsi, complex < double> *spsi)
{
#ifdef __EPM
    nscf_epm::hpsi(psi, hpsi);
#else
    this->h_psi(psi, hpsi);
#endif

    for (int i=0;i<npw_in;i++)
    {
        spsi[i] = psi[i];
    }
    return;
}

void Hamilt_PW::s_1psi
(
    const int dim,
    const complex<double> *psi,
    complex<double> *spsi
)
{
    for (int i=0; i<dim; i++)
    {
        spsi[i] = psi[i];
    }
    return;
}


#ifndef __EPM
void Hamilt_PW::h_psi(const complex<double> *psi_in, complex<double> *hpsi)
{
    timer::tick("Hamilt_PW","h_psi",'H');
    int i = 0;
    int j = 0;
    int ig= 0;

	if(!NONCOLIN) ZEROS(hpsi, wf.npw);
	else ZEROS(hpsi, wf.npwx * NPOL);//added by zhengdy-soc

	//------------------------------------
	//(1) the kinetical energy.
	//------------------------------------
 	if(T_IN_H)
	{	
		for (ig = 0;ig < wf.npw;ig++)
		{
			hpsi[ig] = wf.g2kin[ig] * psi_in[ig];
		}
		//added by zhengdy-soc
		if(NONCOLIN){
			for (ig = wf.npwx;ig < wf.npw + wf.npwx;ig++)
			{
				hpsi[ig] = wf.g2kin[ig - wf.npwx] * psi_in[ig];
			}
		}
	}

	//------------------------------------
	//(2) the local potential.
	//------------------------------------
	if(VL_IN_H)
	{
		if(!NONCOLIN){
			ZEROS( UFFT.porter, pw.nrxxs);
			UFFT.RoundTrip( psi_in, pot.vrs1, GR_index, UFFT.porter );

			for (j = 0;j < wf.npw;j++)
			{
				hpsi[j] += UFFT.porter[ GR_index[j] ];
			}
		}
		else
		{
			complex<double>* porter1 = new complex<double>[pw.nrxxs];
			ZEROS( UFFT.porter, pw.nrxxs);
			ZEROS( porter1, pw.nrxxs);
			for (int ig=0; ig< wf.npw; ig++)
			{
				UFFT.porter[ GR_index[ig]  ] = psi_in[ig];
				porter1[ GR_index[ig]  ] = psi_in[ig + wf.npwx];
			}
			// (2) fft to real space and doing things.
			pw.FFT_wfc.FFT3D( UFFT.porter, 1);
			pw.FFT_wfc.FFT3D( porter1, 1);
			complex<double> sup,sdown;
			for (int ir=0; ir< pw.nrxx; ir++)
			{
				sup = UFFT.porter[ir] * (pot.vrs(0,ir) + pot.vrs(3,ir)) +
					porter1[ir] * (pot.vrs(1,ir) - complex<double>(0.0,1.0) * pot.vrs(2,ir));
				sdown = porter1[ir] * (pot.vrs(0,ir) - pot.vrs(3,ir)) +
				UFFT.porter[ir] * (pot.vrs(1,ir) + complex<double>(0.0,1.0) * pot.vrs(2,ir));
				UFFT.porter[ir] = sup;
				porter1[ir] = sdown;
			}
			// (3) fft back to G space.
			pw.FFT_wfc.FFT3D( UFFT.porter, -1);
			pw.FFT_wfc.FFT3D( porter1, -1);

			for (j = 0;j < wf.npw;j++)
			{
				hpsi[j] += UFFT.porter[ GR_index[j] ];
				hpsi[j+wf.npwx] += porter1[ GR_index[j] ];
			}
			delete[] porter1;
		}
	}

	//------------------------------------
	// (3) the nonlocal pseudopotential.
	//------------------------------------
	if(VNL_IN_H)
	{
		if ( ppcell.nkb > 0)
		{
			complex<double> *becp = new complex<double>[ ppcell.nkb * NPOL ];
			ZEROS(becp,ppcell.nkb * NPOL);
			//zgemm('N','C',1,ppcell.nkb,wf.npw,ONE,psi,1,ppcell.vkb,ppcell.nkb,ZERO,becp,1);
			for (i=0;i< ppcell.nkb;i++)
			{
				const complex<double>* p = &ppcell.vkb(i,0);
				const complex<double>* const p_end = p + wf.npw;
				const complex<double>* psip = psi_in; 
				for (;p<p_end;++p,++psip)
				{
//					becp[i] += psi_in[j]* conj( ppcell.vkb(i,j) );
					if(!NONCOLIN) becp[i] += psip[0]* conj( p[0] );
					else{
						becp[i*2] += psip[0]* conj( p[0] );
						becp[i*2+1] += psip[wf.npwx]* conj( p[0] );
					}
				}
			}

			Parallel_Reduce::reduce_complex_double_pool( becp, ppcell.nkb * NPOL);
			this->add_vuspsi(hpsi, becp);
			delete[] becp;
		}
	}

	// mohan add 2010-07-22
	if( DFT_FUNCTIONAL == "PBE0" )
	{
		//exxpw.vxx_psi(psi_in, hpsi);
	}

    timer::tick("Hamilt_PW","h_psi",'H');
    return;
}
#endif

#ifndef __EPM
void Hamilt_PW::add_vuspsi(complex<double> *hpsi_in,const complex<double> *becp)
{
    timer::tick("Hamilt_PW","add_vuspsi",'I');
    ZEROS( Ps, ppcell.nkb * NPOL );

    int sum = 0;
    int iat = 0;
    // this function sum up each non-local pseudopotential located in each atom,
    // all we need to do is put the right Dij coefficient to each becp, which
    // is calculated before.
    for (int it=0; it<ucell.ntype; it++)
    {
        const int Nprojs = ucell.atoms[it].nh;
        for (int ia=0; ia<ucell.atoms[it].na; ia++)
        {
            // each atom has Nprojs, means this is with structure factor;
            // each projector (each atom) must multiply coefficient
            // with all the other projectors.
            for (int ip=0; ip<Nprojs; ip++)
            {
                for (int ip2=0; ip2<Nprojs; ip2++)
                {
			if(!NONCOLIN)
				this->Ps[sum+ip2] += ppcell.deeq(CURRENT_SPIN, iat, ip, ip2) * becp[sum+ip];
			else
			{
				this->Ps[sum+ ip2*2] += ppcell.deeq_nc(0, iat, ip2, ip) * becp[sum+ip*2]
							+ppcell.deeq_nc(1, iat, ip2, ip) * becp[sum+ip*2+1];
				this->Ps[sum+ ip2*2+1] += ppcell.deeq_nc(2, iat, ip2, ip) * becp[sum+ip*2]
							+ppcell.deeq_nc(3, iat, ip2, ip) * becp[sum+ip*2+1];
			}
                }// end ih
            }//end jh
		if(!NONCOLIN) sum += Nprojs;
		else sum += 2 * Nprojs;
		++iat;
        } //end na
    } //end nt

	/*
    for (int ig=0;ig<wf.npw;ig++)
    {
        for (int i=0;i< ppcell.nkb;i++)
        {
            hpsi_in[ig]+=this->Ps[i]*ppcell.vkb(i,ig);
        }
    }
	*/


	// use simple method.
	if(!NONCOLIN)
	for(int i=0; i<ppcell.nkb; i++)
	{
		complex<double>* p = &ppcell.vkb(i,0);
		complex<double>* p_end = p + wf.npw;
		complex<double>* hp = hpsi_in;
		complex<double>* psp = &Ps[i];
		for (;p<p_end;++p,++hp)
		{
			hp[0] += psp[0] * p[0];
		}
	}
	else
	for(int i=0; i<ppcell.nkb; i++)
	{
		complex<double>* p = &ppcell.vkb(i,0);
		complex<double>* p_end = p + wf.npw;
		complex<double>* hp = hpsi_in;
		complex<double>* hp1 = hpsi_in + wf.npwx;
		complex<double>* psp = &Ps[i*2];
		for (;p<p_end;p++,++hp,++hp1)
		{
			hp[0] += psp[0] * (p[0]);
			hp1[0] += psp[1] * (p[0]);
		}
	}

//	zgemm('N', 'N', 1, wf.npw, ppcell.nkb, ONE, this->Ps , 1, ppcell.vkb,
//	      ppcell.nkb, ONE, hpsi , 1);
    timer::tick("Hamilt_PW","add_vuspsi",'I');
    return;
}
#endif

void Hamilt_PW::diag_zheev(const int &npw_in, ComplexMatrix &psi, const int &nband, double *em, double *err)
{
    TITLE("Hamilt_PW","diag_zheev");
    assert(nband < npw_in) ;

    // if flag =0, this means success.
    // RedM means Reduced matrix, because the dimension of plane wave
    // is much larger than nbands.

    ComplexMatrix RedM(nband, nband);
    complex<double> * eta =  new complex<double>[npw_in] ;
    complex<double> * hpsi1 =  new complex<double>[npw_in] ;
    complex<double> * spsi1 =  new complex<double>[npw_in] ;

	ZEROS(eta, npw_in);
	ZEROS(hpsi1, npw_in);
	ZEROS(spsi1, npw_in);

    double * tmpen = new double[nband] ;

    assert(eta  !=  0) ;
    assert(hpsi1  !=    0) ;
    assert(spsi1    !=  0) ;
    assert(tmpen != 0) ;

    // <j|H|i>, where j < = i is calculated.
    // first calculate eta =|i>, and hpsi = H|i>

    complex<double> tmp ;
    complex<double> tmp1 ;

    // calculate tmpen[i]

    for (int i = 0; i < nband ; i++)
    {
        dcopy(psi, i, eta);
        h_1psi(npw_in, eta, hpsi1, spsi1) ;

        tmp = ZERO ;
        tmp1 = ZERO ;

        for (int ig = 0; ig < npw_in; ig++)
        {
            tmp += conj(eta[ig]) * hpsi1[ig] ;
            tmp1 += conj(eta[ig]) * eta[ig] ;
        }

        tmp /= tmp1 ;

        tmpen[i] = tmp.real() ;

        for (int j = 0; j <= i; j++)
        {
            // calculate H[i,j] = <i|eta> where |eta> = H|j>
            // RedM(j, i) = <j|H|i>

            tmp = ZERO ;

            for (int ig = 0; ig < npw_in; ig++)
            {
                tmp += conj(psi(j, ig)) * hpsi1[ig];
            }

            RedM(j, i) = tmp ;

            if (i != j) RedM(i, j) = conj(tmp) ;
        }
    }

    // This is for calling zheev.
    char  jobz = 'V' ;  // eigenvalues and eigenvectors

    char  uplo = 'U' ;  // the upper is stored.

    const int lwork = 2 * nband;

    complex<double> * work = new complex<double>[lwork]() ;

    double * rwork = new double[3*nband-2] ;

    int info = 0 ;

    // diag by calling zheev
    // basis are psi_0, psi_1, psi_2...

    LapackConnector::zheev(jobz, uplo, nband, RedM, nband, em, work, lwork, rwork, &info);

    delete[] eta  ;

    delete[] hpsi1 ;

    delete[] spsi1 ;

    //  change back to plane wave basis.

//  cout << " calling zheev is performed " << endl ;
//  cout << " zheev output info... " << endl;

    // delete the allocated data array.

    delete[] work ;

    delete[] rwork ;

    // cout the infomation from zheev...

    // cout the infomation from zheev...
    if (info == 0)
    {
        cout << "  successful exit of zheev " << endl ;
    }
    else if (info < 0)
    {
        cout << " the i-th argument had an illegal value. info =  " << info << endl ;
    }
    else
    {
        cout << "the algorithm failed to converge. info = " << info << endl ;
    }

    ComplexMatrix kpsi(nband, npw_in);

    // kpsi = c_1 \psi_1 + c_2 \psi_2 + \cdots + c_N \psi_N
    // For the mth wavefunction, |psi(m)> = U(1, m) \psi_1 + ... U(k, m) \psi_k + ...
    // So, |psi(m)>_j means the jth element of |psi(m)>
    //  We have |psi(m)>_j =  U(1, m) \psi(1, j) + U(2, m) \psi(2, j) + ...
    // = \sum_{k=1}^{nband} U(k, m) \psi(k, j)

    // Store the wavefunction in kpsi

    for (int m = 0; m < nband; m++)
    {
        for (int j = 0; j < npw_in; j++)
        {
            tmp = ZERO ;

            for (int  k = 0; k < nband; k++)
            {
                tmp += RedM(k, m) * psi(k, j);
            }

            kpsi(m, j) = tmp ;
        }
    }


    // update the wavefunction of psi
    for (int m = 0; m < nband ; m++)
    {
        for (int ig = 0; ig < npw_in; ig++)
        {
            psi(m, ig) = kpsi(m, ig);
        }
    }

    // calculate error of the last results.
    // err = || H\psi -E\psi||

    cout << " callilng cal_err " << endl ;

    cal_err(npw_in, psi, nband, em, err);


    // out put the results.
    cout<<setw(6)<<"Bands"
        <<setw(12)<<"energy(ev)"
        <<setw(12)<<"err"
        <<setw(25)<<"||H * psi - E * psi ||\n";

    for (int m = 0; m < 5; m++)
    {
        cout << setw(6) << m
             << setw(12) << em[m] * Ry_to_eV
             << setw(12) << err[m]
             << setw(25) << tmpen[m] * Ry_to_eV << endl ;
    }

    cout << " end of diag_zheev " << endl ;

    return;
}

void Hamilt_PW::cal_err
(
    const int &npw_in,
    ComplexMatrix &psi,
    const int &nband,
    double *em,
    double *err
)
{
//	TITLE("Hamilt_PW", "cal_err");
//	cout << "\n npw_in = " << npw_in << endl;

    assert(nband < npw_in);
    timer::tick("Hamilt_PW", "cal_err") ;

    complex<double> *psitmp =  new complex<double>[npw_in]();
    complex<double> *hpsitmp =  new complex<double>[npw_in]();
    complex<double> *spsitmp =  new complex<double>[npw_in]();

    complex<double> tmp1 ;

    for (int m = 0; m < nband; m++)
    {
//		cout << "\n m = " << m << endl;
        dcopy(psi, m, psitmp) ;
        h_1psi(npw_in, psitmp, hpsitmp, spsitmp);

        complex<double> tmp = ZERO;

        for (int ig=0;  ig<npw_in; ig++)
        {
            tmp1 =  hpsitmp[ig] - em[m] * psitmp[ig];
            tmp +=  conj(tmp1) * tmp1;
        }

        // err[m] = ||H\psitmp - \lambda_m \psitmp||
        err[m] = sqrt( tmp.real() ) ;
    }

    delete[] psitmp;
    delete[] hpsitmp;
    delete[] spsitmp;

//	cout << " calculate error of the wavefunctions " << endl ;
    timer::tick("Hamilt_PW", "cal_err") ;
    return;
}

double Hamilt_PW::ddot_real
(
    const int &dim,
    const complex<double>* psi_L,
    const complex<double>* psi_R
)const
{
    complex<double> result(0,0);
    for (int i=0;i<dim;i++)
    {
        result += conj( psi_L[i] ) * psi_R[i];
    }
    Parallel_Reduce::reduce_complex_double_pool( result );
    return result.real();
}

complex<double> Hamilt_PW::ddot(
    const int & dim,
    const complex<double> * psi_L,
    const complex<double> * psi_R
)const
{
	complex<double> result = ZERO;
	const int incx = 1;
	const int incy = 1;
	// mohan add 2010-10-11
	zdotc_(&result, &dim, psi_L, &incx, psi_R, &incy); 
    
	if(NPROC_IN_POOL>1)
	{
		Parallel_Reduce::reduce_complex_double_pool( result );
	}
    return result;
}  // end of ddot

complex<double> Hamilt_PW::just_ddot(
    const int & dim,
    const complex<double> * psi_L,
    const complex<double> * psi_R
)const
{
	complex<double> result = ZERO;
	//const int incx = 1;
	//const int incy = 1;
	// mohan add 2010-10-11
//	zdotc_(&result, &dim, psi_L, &incx, psi_R, &incy);  

	// mohan update 2011-09-21
	static int warn_about_zdotc=true;
	if(warn_about_zdotc)
	{
		ofs_warning << " in Hamilt_PW::just_ddot, sometimes zdotc is not available due to GNU compiler!!!" << endl;
		ofs_warning << " So here I use simple for cicle to replace zdotc, but it will affect the speed." << endl;
		warn_about_zdotc=false;
	}
	for(int i=0; i<dim; ++i)
	{
		result += conj(psi_L[i])*psi_R[i];	
	}
    return result;
}  // end of ddot



// this return <psi(m)|psik>
complex<double> Hamilt_PW::ddot(
    const int & dim,
    const ComplexMatrix &psi,
    const int & m,
    const complex<double> *psik
)const
{
    complex<double> result(0, 0);
    assert(dim > 0) ;

    for (int i = 0; i < dim ; i++)
    {
        result += conj(psi(m, i)) *  psik[i] ;
    }

    Parallel_Reduce::reduce_complex_double_pool( result );

    return result;
}  // end of ddot

