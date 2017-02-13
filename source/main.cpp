//	Pulse Compressor and Doppler Processor for NetRAD
//	written by: Darryn Jordan
// 	email: jrddar001@myuct.ac.za


#include "main.hpp"

//=====================================================================================================
// Global Variables
//=====================================================================================================

double 		tStart, tEnd;	
int			dopplerDataStart = 0; 
int			dopplerThresholdSlider = 0;
int 		processingDelay = 0;
bool 		doppOn = false;
bool 		suggestOn = false;

radarData netrad(D5);

int REFSIZE 			= netrad.getRefSigSize();
int PADRANGESIZE 		= netrad.getPaddedSize();

int DOPPLERSIZE 		= 64;
int UPDATELINE 			= 10000;
int RANGESIZE 			= 2048;
int RANGELINES 			= 130000;
int FFTW_THREADS		= 1;
int THREADS				= 1;

int RANGELINESPERTHREAD = RANGELINES/THREADS;

//=====================================================================================================
// Allocate Memory
//=====================================================================================================

uint16_t *realDataBuffer      = (uint16_t*)malloc(RANGELINES*RANGESIZE*sizeof(uint16_t));

double   *realRangeBuffer 	  = (double*)malloc(PADRANGESIZE*sizeof(double));
double   *realRefBuffer       = (double*)malloc(PADRANGESIZE*sizeof(double));

uint8_t  *matchedImageBuffer  = (uint8_t*)malloc(PADRANGESIZE*sizeof(uint8_t));
uint8_t  *dopplerImageBuffer  = (uint8_t*)malloc(DOPPLERSIZE*sizeof(uint8_t));

fftw_complex *fftRangeBuffer  = (fftw_complex*)malloc((PADRANGESIZE/2 + 1)*sizeof(fftw_complex));
fftw_complex *fftRefBuffer    = (fftw_complex*)malloc((PADRANGESIZE/2 + 1)*sizeof(fftw_complex)); 

fftw_complex *hilbertBuffer   = (fftw_complex*)malloc((PADRANGESIZE)*sizeof(fftw_complex));
fftw_complex *dopplerBuffer   = (fftw_complex*)malloc((DOPPLERSIZE)*sizeof(fftw_complex));

fftw_complex *dopplerData     = (fftw_complex*)malloc((PADRANGESIZE*DOPPLERSIZE)*sizeof(fftw_complex));

float *refWindow			  = (float*)malloc(REFSIZE*sizeof(float));
float *doppWindow			  = (float*)malloc(DOPPLERSIZE*sizeof(float));

//=====================================================================================================
// Setup FFTW Plans
//=====================================================================================================

fftw_plan refPlan = fftw_plan_dft_r2c_1d(PADRANGESIZE, realRefBuffer, fftRefBuffer, FFTW_ESTIMATE);
fftw_plan rangePlan = fftw_plan_dft_r2c_1d(PADRANGESIZE, realRangeBuffer, fftRangeBuffer, FFTW_MEASURE);
fftw_plan resultPlan  = fftw_plan_dft_c2r_1d(PADRANGESIZE, hilbertBuffer, realRangeBuffer, FFTW_MEASURE | FFTW_PRESERVE_INPUT);
fftw_plan hilbertPlan = fftw_plan_dft_1d(PADRANGESIZE, hilbertBuffer, hilbertBuffer, FFTW_BACKWARD, FFTW_MEASURE);
fftw_plan dopplerPlan = fftw_plan_dft_1d(DOPPLERSIZE, dopplerBuffer, dopplerBuffer, FFTW_FORWARD, FFTW_MEASURE);	

int main(int argc, char *argv[])
{
	boost::thread_group threadGroup;
	boost::thread threads[THREADS];		
	
	initTerminal();	
	startTime();

	fftw_init_threads();
	fftw_plan_with_nthreads(FFTW_THREADS);

	popLookUpTables();
	initOpenCV();	
	loadRefData();		
	normRefData();	
	fftw_execute(refPlan);	
	complxConjRef();		
	
	loadRangeData();
	
	for (int i = 0; i < THREADS; i++)
	{
		threadGroup.create_thread(boost::bind(&perThread, i, i*RANGELINESPERTHREAD));
	}
	
	threadGroup.join_all();		
	
	freeMem();
	cv::waitKey(0);
	
	return 0;
}


void perThread(int id, int start_index)
{
	std::cout << "Thread: " << id << ", Index: " << start_index << std::endl;	
	
	for (int i = start_index; i < start_index + RANGELINESPERTHREAD; i++)
	{
		popRangeBuffer(i, realRangeBuffer);	
		fftw_execute(rangePlan);		
		complxMulti();			
		ifftMatchedData();								
		postProcessMatched();
		updateWaterfall(i, realRangeBuffer);

		if (doppOn)
		{
			hilbertTransform();		//unnecessary cycles			
			popDopplerData(i); 		
			processDoppler(i);		
		}					
	}
	
	free(realRangeBuffer);
	
	timePerLine();
}


void processDoppler(int rangeLine)
{
	if ((rangeLine + 1 - dopplerDataStart) == DOPPLERSIZE)  //check that dopplerData is full
	{
		for (int i = 0; i < PADRANGESIZE; i++)		
		{
			popDopplerBuffer(i);	
			fftDopplerData();
			postProcessDoppler();
			updateDoppler(dopplerImageBuffer);	
		}
		plotDoppler();
	}
}

void popDopplerData(int rangeLine)
{
	if (rangeLine%UPDATELINE == 0)
		dopplerDataStart = rangeLine;

	if ((rangeLine + 1 - dopplerDataStart) <= DOPPLERSIZE)
	{
		for (int j = 0; j < PADRANGESIZE; j++)
		{
			dopplerData[j*DOPPLERSIZE + (rangeLine - dopplerDataStart)][0] = hilbertBuffer[j][0];
			dopplerData[j*DOPPLERSIZE + (rangeLine - dopplerDataStart)][1] = hilbertBuffer[j][1];
		}
	}
}

void popDopplerBuffer(int dopplerLine)
{
	for (int j = 0; j < DOPPLERSIZE; j++)
	{	
		dopplerBuffer[j][0] = dopplerData[dopplerLine*DOPPLERSIZE + j][0]*doppWindow[j]; 
		dopplerBuffer[j][1] = dopplerData[dopplerLine*DOPPLERSIZE + j][1]*doppWindow[j];
	}
}

void postProcessMatched(void)
{
	/*float maxResult = 0.0f;

	for (int j = 0; j < PADRANGESIZE; j++)
	{
		realRangeBuffer[j] = (abs(realRangeBuffer[j]));  //Largest Bottleneck -> Removed Log

		if (realRangeBuffer[j] > maxResult)
			maxResult = realRangeBuffer[j];
	}

	for (int j = 0; j < PADRANGESIZE; j++)
		matchedImageBuffer[j] = (uint8_t)((realRangeBuffer[j]/maxResult)*255);*/	
}

void postProcessDoppler(void)
{
	float maxResult = 0.0f;	
	float result = 0.0f;
	int processed = 0;

	for (int i = 0; i < DOPPLERSIZE; i++)
	{
		result = (sqrt(dopplerBuffer[i][0]*dopplerBuffer[i][0] + dopplerBuffer[i][1]*dopplerBuffer[i][1]));

		if (result > maxResult)
			maxResult = result;
	}

	for (int i = 0; i < DOPPLERSIZE; i++)
	{
		processed = (uint8_t)(((sqrt(dopplerBuffer[i][0]*dopplerBuffer[i][0] + dopplerBuffer[i][1]*dopplerBuffer[i][1]))/maxResult)*255);

		if (processed < dopplerThresholdSlider) //set threshold
			processed = 0;
		
		if (i < (DOPPLERSIZE/2 + 1))		
			dopplerImageBuffer[i + (DOPPLERSIZE/2 - 1)] = processed;
		else
			dopplerImageBuffer[i - (DOPPLERSIZE/2 + 1)] = processed;
	}
}

void complxConjRef(void)
{
	for (int i = 0; i < (PADRANGESIZE/2 + 1); i++)
		fftRefBuffer[i][1] = -1*fftRefBuffer[i][1];
	printMsg("Complex Conjugate Reference");
}

void complxMulti(void)
{
	for (int j = 0; j < (PADRANGESIZE/2 + 1); j++)
	{			
		hilbertBuffer[j][0] = (fftRangeBuffer[j][0]*fftRefBuffer[j][0] - fftRangeBuffer[j][1]*fftRefBuffer[j][1]);
		hilbertBuffer[j][1] = (fftRangeBuffer[j][0]*fftRefBuffer[j][1] + fftRangeBuffer[j][1]*fftRefBuffer[j][0]);
	}
}

void popRangeBuffer(int rangeLine, double* realRangeBuffer)
{
	int start = rangeLine*RANGESIZE;

	//populate complex range data and remove offset	
	for (int i = 0; i < PADRANGESIZE; i++)
	{
		if (i < RANGESIZE)
			realRangeBuffer[i] = (double)(realDataBuffer[i + start] - ADCOFFSET);
		else
			realRangeBuffer[i] = 0; 
	}
}

void loadRangeData(void)
{
	FILE *dataBinFile_p;	
	
	//open binary file
	dataBinFile_p = fopen(netrad.getDataSetName(), "r");

	//read from binary files into buffers
	fread(realDataBuffer, sizeof(uint16_t), RANGELINES*RANGESIZE, dataBinFile_p);

	//close binary files
	fclose(dataBinFile_p);

	printMsg("Range Data Loaded");
}

void loadRefData(void)
{
	std::vector<double> refVector;	
	std::ifstream file(netrad.getRefSigName());
    std::string line; 

    while (std::getline(file, line))
    {
		std::stringstream ss(line);

		double i;
		while (ss >> i)
		{
			refVector.push_back(i);

			if (ss.peek() == ',')
				ss.ignore();
		}
	}

	//populate complex ref data and window
	for (int i = 0; i < PADRANGESIZE; i++)
	{		
		if (i < REFSIZE)
			realRefBuffer[i] = (float)((-1*refVector[i])*refWindow[i]);
		else
			realRefBuffer[i] = 0;
	}

	printMsg("Reference Data Loaded");	

}

void normRefData(void)
{
	float normFactor = getNormFactor();	
	
	for (int i = 0; i < PADRANGESIZE; i++)
		realRefBuffer[i] = realRefBuffer[i]/normFactor;

	printMsg("Reference Data Normalized");
}

float getNormFactor(void)
{
	float refMax = 0;	
	for(int i = 0; i < REFSIZE; i++)
	{
		if (realRefBuffer[i] > refMax)
			refMax = realRefBuffer[i];
	}
	
	return refMax;
}

void freeMem(void)
{
	fftw_free(fftRangeBuffer);
	fftw_free(fftRefBuffer);
	fftw_free(hilbertBuffer);
	fftw_free(dopplerBuffer);
	fftw_free(dopplerData);

	free(realDataBuffer);	
	free(realRefBuffer);
	
	fftw_destroy_plan(rangePlan);		
	fftw_destroy_plan(resultPlan);	
	fftw_destroy_plan(hilbertPlan);	
	fftw_destroy_plan(dopplerPlan);
	fftw_destroy_plan(refPlan);

	printMsg("Memory Free \n");
}


void timePerLine(void)
{
	std::cout << "\nAverage Time per Line: "<< std::setprecision (2) << std::fixed << getTime()/RANGELINES * 1000000 << " us\n" << std::endl;
}




