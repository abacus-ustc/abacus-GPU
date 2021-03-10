#ifndef TEST_FUNCTION_H
#define TEST_FUNCTION_H

#include<sstream>
#include<fstream>
#include<string>
#include<mpi.h>
#include<sys/time.h>

// Peize Lin add 2015-11-11
static void MPI_RANK_OFSTREAM( const std::string& file_name, std::stringstream &content )
{
	std::stringstream file;
	int my_rank;	MPI_Comm_rank(MPI_COMM_WORLD,&my_rank);	
	file<<file_name<<"_"<<my_rank;
	
	std::ofstream ofs(file.str().c_str(), ofstream::app);
	ofs<<content.str();
	
	content.str("");
	ofs.close();
}

// Peize Lin add 2016-06-06
template<typename T>
static std::ostream & operator<<( std::ostream & os, const std::vector<T> &v )
{
	for( const T &i : v )
		os<<i<<std::endl;
	return os;
}

// Peize Lin add 2016-10-10
static double cal_time( const timeval &t_begin )
{
	timeval t_end;
	gettimeofday( &t_end, NULL);
	return (double)(t_end.tv_sec-t_begin.tv_sec) + (double)(t_end.tv_usec-t_begin.tv_usec)/1000000.0;
}


#endif		// TEST_FUNCTION_H