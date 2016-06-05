/*
 * InteractionNetwork.cpp - Bill White - 12/1/12
 *
 * In Silico Lab interaction network class.
 * 
 * Imported into inbix and removed external library dependencies: 
 * boost, armadillo and igraph. - bcw - 5/13/13
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>
#include <cmath>
#include <numeric>

#include <armadillo>

#include "plink.h"
#include "helper.h"
#include "stats.h"
#include "StringUtils.h"

#include "InteractionNetwork.h"

using namespace std;
using namespace Insilico;
using namespace arma;

InteractionNetwork::InteractionNetwork(string matrixFileParam,
		 MatrixFileType fileType, bool isUpperTriangular, Plink* pp)
{
  // provide access to the inbix (PLINK) environment)
  inbixEnv = pp;
  
	// read the matrix file and store in adjacencyMatrix member variable
	switch(fileType) {
	case REGAIN_FILE:
		if(!ReadGainFile(matrixFileParam, isUpperTriangular)) {
			error("FATAL ERROR: Reading (re)GAIN file: " + matrixFileParam + "\n");
		}
		break;
	case CORR_1D_FILE:
		if(!ReadBrainCorr1DFile(matrixFileParam)) {
			error("FATAL ERROR: Reading matrix file: " + 
							matrixFileParam + "\n");
		}
		break;
	case CSV_FILE:
		if(!ReadCsvFile(matrixFileParam)) {
			error("FATAL ERROR: Reading matrix file: " + matrixFileParam + "\n");
		}
		break;
	case SIF_FILE:
		if(!ReadSifFile(matrixFileParam)) {
			error("FATAL ERROR: Reading SIF file: " + matrixFileParam + "\n");
		}
		break;
	default:
		error("Could not determine the matrix file type: " + 
						int2str(fileType) + "\n");
	}
	networkFile = matrixFileParam;

	// set default values
  connectivityThreshold = DEFAULT_CONNECTIVITY_THRESHOLD;
  useBinaryThreshold = true;
	startMergeOrder = 2;
	maxMergeOrder = 4;
	maxModuleSize = 10;
	minModuleSize = 200;
	connMatrix = adjMatrix;
}

InteractionNetwork::InteractionNetwork(double** variablesMatrix,
		unsigned int dim, vector<string>& variableNames, Plink* pp)
{
  // provide access to the inbix (PLINK) environment)
  inbixEnv = pp;

	// setup G matrix for SNPrank algorithm
	adjMatrix.resize(dim, dim);
	// copy variable matrix values into G
	numEdges = 0;
	for(unsigned int i=0; i < dim; ++i) {
		for(unsigned int j=0; j < dim; ++j) {
			adjMatrix(i , j) = adjMatrix(j , i) = variablesMatrix[i][j];
			numEdges++;
		}
	}
	for(unsigned int i=0; i < dim; ++i) {
		nodeNames.push_back(variableNames[i]);
	}
	numNodes = dim;
	networkFile = "<in-memory matrix>";

	// set default values
  connectivityThreshold = DEFAULT_CONNECTIVITY_THRESHOLD;
  useBinaryThreshold = true;
	startMergeOrder = 2;
	maxMergeOrder = 4;
	maxModuleSize = 10;
	minModuleSize = 200;
	connMatrix = adjMatrix;
}

InteractionNetwork::~InteractionNetwork()
{}

bool InteractionNetwork::PrepareConnectivityMatrix() {

	// keep original adjacency matrix
	connMatrix = adjMatrix;
	numNodes = connMatrix.n_cols;

	// convert the adjacency matrix to a connectivity matrix
	connMatrix.diag() = zeros<vec>(numNodes);
	for(unsigned int i=0; i < numNodes; ++i) {
		for(unsigned int j=0; j < numNodes; ++j) {
			double edgeValue = connMatrix(i, j);
			if(useConnectivityThreshold) {
				if(connectivityThresholdAbs) {
					edgeValue = fabs(edgeValue);
				}
				if(edgeValue <= connectivityThreshold) {
					connMatrix(i, j) = 0.0;
				} else {
	        if(useBinaryThreshold) {
	          connMatrix(i, j) = 1.0;
	        } // else use weight
				}
			}
		}
	}
	inbixEnv->printLOG("--- Connectivity matrix finalized\n");

	degrees = sum(connMatrix, 0);
	numEdges = 0.5 * sum(degrees);
	
	this->PrintSummary();
  
  return true;
}

bool InteractionNetwork::SetConnectivityThresholding(bool binaryFlag) {
  useConnectivityThreshold = binaryFlag;
  
  return true;
}

bool InteractionNetwork::SetConnectivityThreshold(double threshold) {
	useConnectivityThreshold = true;
  connectivityThreshold = threshold;

  return true;
}

bool InteractionNetwork::SetConnectivityThresholdAbs(bool absFlag) {
	connectivityThresholdAbs = true;
 
  return true;
}

bool InteractionNetwork::SetBinaryThresholding(bool binaryFlag) {
  useBinaryThreshold = binaryFlag;
  
  return true;
}

unsigned int InteractionNetwork::NumNodes()
{
	return adjMatrix.n_cols;
}

arma::mat InteractionNetwork::GetAdjacencyMatrix() {
	return adjMatrix;
}

arma::mat InteractionNetwork::GetConnectivityMatrix() {
	return connMatrix;
}

vector<string> InteractionNetwork::GetNodeNames() {
	return nodeNames;
}

void InteractionNetwork::PrintAdjacencyMatrix() {
	for(unsigned int i=0; i < nodeNames.size(); ++i) {
		cout << setw(12) << nodeNames[i];
	}
	cout << endl;
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=0; j < adjMatrix.n_cols; ++j) {
			if(j <= i) {
				printf("%8.6f\t", adjMatrix(i, j));
			}
		}
		cout << endl;
	}
}

void InteractionNetwork::PrintConnectivityMatrix() {
	for(unsigned int i=0; i < nodeNames.size(); ++i) {
		cout << setw(12) << nodeNames[i];
	}
	cout << endl;
	for(unsigned int i=0; i < connMatrix.n_cols; ++i) {
		for(unsigned int j=0; j < connMatrix.n_cols; ++j) {
			if(j <= i) {
				printf("%8.6f\t", connMatrix(i, j));
			}
		}
		cout << endl;
	}
}

void InteractionNetwork::PrintSummary()
{
	inbixEnv->printLOG("Matrix source: " + networkFile + "\n");
	inbixEnv->printLOG("Matrix Dimensions: "	+ dbl2str(numNodes) + 
		" x " + dbl2str(numNodes) + "\n");
	inbixEnv->printLOG("Edges: " + dbl2str(numEdges) + "\n");
	if(par::modEnableConnectivityThreshold) {
		inbixEnv->printLOG("Edge Threshold: " + 
			dbl2str(connectivityThreshold) + "\n");
	}
	inbixEnv->printLOG("Adjacency Matrix:\n");
  inbixEnv->printLOG("Minimum: " + dbl2str(adjMatrix.min()) + "\n");
  inbixEnv->printLOG("Maximum: " + dbl2str(adjMatrix.max()) + "\n");
	inbixEnv->printLOG("Connectivity Matrix:\n");
  inbixEnv->printLOG("Minimum: " + dbl2str(connMatrix.min()) + "\n");
  inbixEnv->printLOG("Maximum: " + dbl2str(connMatrix.max()) + "\n");
}

bool InteractionNetwork::WriteToFile(string outFile, MatrixFileType fileType)
{
	bool success = false;
  
	switch(fileType) {
	case CSV_FILE:
		success = WriteDelimitedFile(outFile, ",");
		break;
	case REGAIN_FILE:
		success = WriteDelimitedFile(outFile, "\t");
		break;
	case SIF_FILE:
		success = WriteSifFile(outFile);
		break;
	default:
		cerr << "InteractionNetwork::WriteToFile: "
				<< "ERROR: Unknown file type: " << fileType << endl;
	}

	return success;
}

bool InteractionNetwork::WriteDelimitedFile(string outFilename, string delimiter)
{
	ofstream outputFileHandle(outFilename.c_str());
	for(unsigned int i=0; i < nodeNames.size(); ++i) {
		if(i) {
			outputFileHandle << delimiter << nodeNames[i];
		}
		else {
			outputFileHandle << nodeNames[i];
		}
	}
	outputFileHandle << endl << setiosflags(ios::fixed) << setprecision(8);
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=0; j < adjMatrix.n_cols; ++j) {
			if(j) {
				outputFileHandle << delimiter << adjMatrix(i , j);
			}
			else {
				outputFileHandle << adjMatrix(i , j);
			}
		}
		outputFileHandle << endl;
	}
	outputFileHandle.close();

	return true;
}

bool InteractionNetwork::WriteSifFile(string outFilename)
{
	ofstream outputFileHandle(outFilename.c_str());
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=i+1; j < adjMatrix.n_cols; ++j) {
			if(adjMatrix(i , j)) {
				outputFileHandle
					<< nodeNames[i] << "\t" << adjMatrix(i , j) << nodeNames[j] << endl;
			}
		}
	}
	outputFileHandle.close();

	return true;
}

bool InteractionNetwork::Merge(InteractionNetwork& toMerge,
	                             double priorProbEdges,
	                             double alpha,
	                             double omega,
	                             double threshold) {
	if(toMerge.NumNodes() != adjMatrix.n_cols) {
		cerr << "ERROR: Cannot merge networks of different sizes." << endl;
		return false;
	}
	arma::mat otherAdjacencyMatrix = toMerge.GetAdjacencyMatrix();

	double posteriorProb = 0.0;
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=i; j < adjMatrix.n_cols; ++j) {
//			if(i == j) {
//				adjMatrix(i , j) = 0;
//				continue;
//			}
			double beta_ij_1 = adjMatrix(i , j);
			double beta_ij_2 = otherAdjacencyMatrix(i , j);
			double probWgE1 = alpha * (1.0 - exp(-omega * beta_ij_1));
			double probWgE2 = alpha * (1.0 - exp(-omega * beta_ij_2));
      double p = probWgE1 * probWgE2 * priorProbEdges;
			posteriorProb = p * (1 + log(1 / p));
      
//			cout
//				<< "B_ij_1: " << beta_ij_1 << " "
//				<< "B_ij_2: " << beta_ij_2 << " "
//				<< "P(W1|E1): " << probWgE1 << " "
//				<< "P(W1|E1): " << probWgE2 << " "
//				<< "posterior: " << posteriorProb
//				<< endl;
			if(posteriorProb > threshold) {
				adjMatrix(i , j) = posteriorProb;
				adjMatrix(j , i) = posteriorProb;
			}
			else {
				adjMatrix(i , j) = 0;
				adjMatrix(j , i) = 0;
			}
		}
	}

	return true;
}

// apply a power transform with exponent
bool InteractionNetwork::ApplyPowerTransform(double transformExponent) {
	
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=0; j < adjMatrix.n_cols; ++j) {
			adjMatrix(i, j) = pow(adjMatrix(i, j), transformExponent);
		}
	}
	
	return true;
}

// apply a Fisher transformation for correlation values
bool InteractionNetwork::ApplyFisherTransform() {
	
	for(unsigned int i=0; i < adjMatrix.n_cols; ++i) {
		for(unsigned int j=0; j < adjMatrix.n_cols; ++j) {
			double r = adjMatrix(i, j);
			if(r > par::modFisherTransformCutoff) { 
				r = par::modFisherTransformCutoff; 
			}
			if(r == -par::modFisherTransformCutoff) { 
				r = -par::modFisherTransformCutoff; 
			}
			adjMatrix(i, j) = log((1 + r) / (1 - r));
		}
	}
	
	return true;
}

// ------------------ P R I V A T E   M E T H O D S --------------------------

bool InteractionNetwork::ReadCsvFile(string matrixFilename)
{
  ifstream matrixFileHandle(matrixFilename.c_str());
  if(!matrixFileHandle.is_open()) {
      cerr << "ERROR: Could not open matrix file" << matrixFilename << endl;
      return false;
  }

  string line;
  string delimiter = ",";

  // read first line - header with node names
  getline(matrixFileHandle, line);
  string trimmedLine = trim(line);
  vector<string> lineParts;
  split(lineParts, trimmedLine, delimiter);
  for(unsigned int nn=0; nn < lineParts.size(); ++nn) {
  	nodeNames.push_back(lineParts[nn]);
  	nodeNameIndex[lineParts[nn]] = nn;
  }

  // initialize dimensions of the adjacency matrix
  size_t adjDim = lineParts.size();
  if(!adjDim) {
      cerr << "ERROR: Could not parse header values" << endl;
      return false;
  }
  adjMatrix.resize(adjDim, adjDim);
  numNodes = adjDim;

  // read the rest of the file as adjacency matrix values
  numEdges = 0;
  unsigned int row = 0;
  unsigned int tokensExpected = adjDim;
  while(getline(matrixFileHandle, line)) {
    string trimmedLine = trim(line);
    lineParts.clear();
    split(lineParts, trimmedLine, delimiter);
    if(lineParts.size() != tokensExpected) {
      cerr << "ERROR line:" << endl << endl << line << endl << endl
      		<< "ERROR: Could not parse file row: " << (row+1) << endl
      		<< "Expecting " << tokensExpected << " values, got "
      		<< lineParts.size() << endl;
      return false;
    }
    // set the matrix values to the parsed row values
    unsigned int col = 0;
      for(; col < lineParts.size(); ++col) {
        string trimmedPart = trim(lineParts[col]);
				double t;
				if(!from_string<double>(t, lineParts[col], std::dec)) {
					error("Parsing CSV line " + line);
				}
        adjMatrix(row, col) = t;
        ++numEdges;
      }
    row++;
  }
  matrixFileHandle.close();
  numEdges /= 2;

  return true;
}

bool InteractionNetwork::ReadGainFile(string gainFilename, bool isUpperTriangular)
{
	ifstream gainFileHandle(gainFilename.c_str());
	if(!gainFileHandle.is_open()) {
		cerr << "ERROR: Could not open (re)GAIN file: " << gainFilename << endl;
		return false;
	}

	string line;

	// read first line (header)
	getline(gainFileHandle, line);
	trim(line);

	// tokenize header
  vector<string> lineParts;
  split(lineParts, line, "\t");
  for(unsigned int nn=0; nn < lineParts.size(); ++nn) {
  	nodeNames.push_back(lineParts[nn]);
  	nodeNameIndex[lineParts[nn]] = nn;
  }

	// initialize dimensions of data matrix
	size_t numVars = nodeNames.size();
	if(!numVars) {
		cerr << "ERROR: Could not parse SNP names from (re)GAIN file header" << endl;
		return false;
	}
	adjMatrix.resize(numVars, numVars);
	numNodes = numVars;

	// read numeric data into G
	size_t row = 0;
	vector<string> lineTokens;
	unsigned int tokensExpected = numVars;
	numEdges = 0;
	while(getline(gainFileHandle, line)) {
		trim(line);
		lineTokens.clear();
		split(lineTokens, line, "\t");
		if(lineTokens.size() != tokensExpected) {
			cerr << "ERROR line:" << endl << endl << line << endl << endl;
			cerr << "ERROR: Could not parse (re)GAIN file row: " << (row+2) << endl
					<< "Expecting " << tokensExpected << " values, got "
					<< lineTokens.size() << endl;
			return false;
		}
		size_t startIndex = numVars - tokensExpected;
		for (size_t tokenIndex=0, col=startIndex; col < numVars; ++tokenIndex, ++col) {
			string token = lineTokens[tokenIndex];
			trim(token);
			if(token == "") {
				cerr << "ERROR: parsing line " << row << " col " << col
						<< " of (re)Gain file, token [" << token << "]" << endl;
				return false;
			}
			double t;
			if(!from_string<double>(t, token, std::dec)) {
				error("Parsing failed in REGAIN line:\n" + line);
			}
			adjMatrix(row, col) = t;
			if((row != col) && isUpperTriangular) {
				adjMatrix(col, row) = t;
				++numEdges;
			}
		}
		row++;
		if(isUpperTriangular) {
			--tokensExpected;
		}
	}

	gainFileHandle.close();

	return true;
}

bool InteractionNetwork::ReadSifFile(string sifFilename)
{
	// parse the graph data structures

	// unique node names
  set<string> nodeNameSet;
  // edges between nodes
  vector<pair<pair<string, string>, double> > edges;

  // parse the SIF text file
  ifstream sifFileHandle(sifFilename.c_str());
  if(!sifFileHandle.is_open()) {
      cerr << "ERROR: Could not open SIF file" << sifFilename << endl;
      return false;
  }

  string line;
  string delimiter = "\t";
  while(getline(sifFileHandle, line)) {
    trim(line);
    if(line == "") {
    	cout << "WARNING: Blank line skipped" << endl;
    	continue;
    }
    vector<string> sifValues;
    split(sifValues, line, delimiter);
    string node1 = sifValues[0];
		double t;
		if(!from_string<double>(t, sifValues[1], std::dec)) {
			error("Parsing SIF line " + line);
		}
    double weight = t;
    string node2 = sifValues[2];
    nodeNameSet.insert(node1);
    nodeNameSet.insert(node2);
    edges.push_back(make_pair(make_pair(node1, node2), weight));
  }
  sifFileHandle.close();

  // assign each node name a unique index
  map<string, unsigned int> nodeNameMap;
  set<string>::const_iterator nnsIt = nodeNameSet.begin();
  for(unsigned int nnIndex=0;
  		nnsIt != nodeNameSet.end();
  		++nnsIt, ++nnIndex) {
  	nodeNameMap[*nnsIt] = nnIndex;
  	nodeNames.push_back(*nnsIt);
  	nodeNameIndex[*nnsIt] = nnIndex;
  }
  numNodes = nodeNames.size();

  // set symmetric adjacency matrix for the edges
  adjMatrix.resize(nodeNameSet.size(), nodeNameSet.size());
  adjMatrix.fill(0.0);
  numEdges = 0;
  vector<pair<pair<string, string>, double> >::const_iterator edgeIt;
  for(edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt) {
  	pair<string, string> nodeNames = edgeIt->first;
  	double weight = edgeIt->second;
  	unsigned int node1Index = nodeNameMap[nodeNames.first];
  	unsigned int node2Index = nodeNameMap[nodeNames.second];
  	adjMatrix(node1Index, node2Index) = weight;
  	adjMatrix(node2Index, node1Index) = weight;
  	++numEdges;
  }

  return true;
}

bool InteractionNetwork::ReadBrainCorr1DFile(string corr1dFilename) {

	ifstream corr1dFileHandle(corr1dFilename.c_str());
  if(!corr1dFileHandle.is_open()) {
      cerr << "ERROR: ReadBrainCorr1DFile: Could not open "
      		<< corr1dFilename << endl;
      return false;
  }

  string line;
  string delimiter = " ";
  unsigned int row = 0;

  // read header line
  getline(corr1dFileHandle, line);
  trim(line);
  // strip off first the two characters: #<space>
  string header = line.substr(2, line.size()-2);
  vector<string> headerValues;
  split(headerValues, header);
  unsigned int adjDim = headerValues.size();
  if(!adjDim) {
		cerr << "ERROR: ReadBrainCorr1DFile: Could not parse 1D correlation values"
				<< endl;
		return false;
  }
  adjMatrix.resize(adjDim, adjDim);

  vector<string>::const_iterator hIt = headerValues.begin();
  unsigned int hIndex = 0;
  for(; hIt != headerValues.end(); ++hIt, ++hIndex) {
  	nodeNames.push_back(*hIt);
  	nodeNameIndex[*hIt] = hIndex;
  }
  numNodes = nodeNames.size();

  // read the rest of the matrix file
  vector<string> corr1dValues;
  unsigned int tokensExpected = adjDim;
  numEdges = 0;
  while(getline(corr1dFileHandle, line)) {
    trim(line);
    corr1dValues.clear();
    split(corr1dValues, line, delimiter);
    if(corr1dValues.size() != tokensExpected) {
      cerr << "ERROR line:" << endl << endl << line << endl << endl;
      cerr << "ERROR: Could not parse 1D correlation file row: "
        << (row+1) << endl << "Expecting " << tokensExpected
        << " values, got " << corr1dValues.size() << endl;
      return false;
    }
    // set the matrix values to the parsed row values
  	unsigned int col=0;
		for(; col < corr1dValues.size(); ++col) {
			string trimmedCor = trim(corr1dValues[col]);
			double t;
			if(!from_string<double>(t, trimmedCor, std::dec)) {
				error("Parsing Corr1D line " + line);
			}
			adjMatrix(row, col) = t;
			++numEdges;
		}
    row++;
  }
  corr1dFileHandle.close();
  numEdges /= 2;

	return true;
}

bool InteractionNetwork::ripM(unsigned int pStartMergeOrder,
	                            unsigned int pMaxMergeOrder,
	                            unsigned int pMinModuleSize, 
	                            unsigned int pMaxModuleSize) {
	// converted from R - bcw - June 2016
	startMergeOrder = pStartMergeOrder;
	maxMergeOrder = pMaxMergeOrder;
	maxModuleSize = pMaxModuleSize;
	minModuleSize = pMinModuleSize;
	inbixEnv->printLOG("RIPM: Setting configuration parameters\n");
	inbixEnv->printLOG("RIPM: Merge start order: " + int2str(startMergeOrder) + "\n");
	inbixEnv->printLOG("RIPM: Merge max order:   " + int2str(maxMergeOrder) + "\n");
	inbixEnv->printLOG("RIPM: Min module size:   " + int2str(minModuleSize) + "\n");
	inbixEnv->printLOG("RIPM: Max module size:   " + int2str(maxModuleSize) + "\n");

	inbixEnv->printLOG("RIPM: Creating initial module list\n");
	ModuleIndices firstModule;
	for(unsigned int i=0; i < (unsigned int) numNodes; ++i) {
		firstModule.push_back(i);
	}

	inbixEnv->printLOG("RIPM: Preparing connectivity matrix\n");
	PrepareConnectivityMatrix();	

	inbixEnv->printLOG("RIPM: Calling recursive rip-M algorithm\n");
	modules.clear();
	ModuleList results;
	bool success = this->RecursiveIndirectPathsModularity(connMatrix, firstModule, results);

	if(success) {
		inbixEnv->printLOG("RIPM: Found " + int2str(results.size()) + " modules\n");
		for(unsigned int i=0; i < results.size(); ++i) {
			inbixEnv->printLOG("RIPM: Module: " + int2str(i) + 
				                 " size: " + int2str(results[i].size()) + "\n");
		}
		modules = results;
	} else {
		inbixEnv->printLOG("RIPM: Failed!\n");
	}

	return success;
}

bool InteractionNetwork::RecursiveIndirectPathsModularity(mat& thisAdj, 
	                                                        ModuleIndices thisModuleIdx, 
	                                                        ModuleList& results) {
	mat A = thisAdj(conv_to<uvec>::from(thisModuleIdx), conv_to<uvec>::from(thisModuleIdx));
	rowvec k = sum(A, 0);
	double m = 0.5 * sum(k);

	inbixEnv->printLOG("RIPM: Running Newman modularity on module size: " + 
		                 int2str(thisModuleIdx.size()) + "\n");
	ModularityResult modResult;
	if(!this->GetNewmanModules(A, thisModuleIdx, modResult)) {
		// BASIS: cannot do anything with this module, so return from 
		// this level of recursion
		inbixEnv->printLOG("RIPM: WARNING: Cannot split this module, saving as is\n");
		inbixEnv->printLOG("RIPM: Exiting recursive rip-M algorithm\n");
		results.push_back(thisModuleIdx);
		return true;
	}
	// else look at the partition of modules
	double Q = modResult.first;
	unsigned int numModules = modResult.second.size();
	inbixEnv->printLOG("Total modularity Q = " + dbl2str(Q) + "\n");
	inbixEnv->printLOG("rip-M found " + int2str(numModules) + " modules" + "\n");
	for(unsigned int i=0; i < numModules; ++i) {
		inbixEnv->printLOG("RIPM: Module: " + int2str(i) + 
			                 " size: " + int2str(modResult.second[i].size()) + "\n");
	}

	if(numModules > 1) {
		// look at the modules list to see if they need merging or further splitting
		ModuleList smallModules;
		for(unsigned int i=0; i < numModules; ++i) {
			ModuleIndices thisModule = modResult.second[i];
			if(thisModule.size() > maxModuleSize) {
				// RECURSION
				inbixEnv->printLOG("RIPM: Recursing into rip-M algorithm module size:" + 
					                 int2str(thisModule.size()) + "\n");
				this->RecursiveIndirectPathsModularity(A, thisModule, results);
			} else {
				if(thisModule.size() < maxModuleSize) {
					// collect all modules less than max module size for merge attempt
					inbixEnv->printLOG("RIPM: Collecting small module size: " + 
					                 int2str(thisModule.size()) + "\n");
					smallModules.push_back(thisModule);
				} 
			}
		}
		if(smallModules.size()) {
			// MERGE: attempt to merge small modules list as a matrix of its own
			inbixEnv->printLOG("RIPM: Merging small module matrix size: " + 
					               int2str(smallModules.size()) + "\n");
			ModuleList smallModuleResults;
			if(this->MergeSmallModules(A, smallModules, smallModuleResults)) {
				for(unsigned int i=0; i < smallModuleResults.size(); ++i) {
					results.push_back(smallModuleResults[i]);
				}
			} else {
				results = smallModules;
			}
		}
	} else {
		// only one module, return it
		results.push_back(thisModuleIdx);
	}

	modules = results;

	return true;
}

bool InteractionNetwork::GetNewmanModules(mat& thisAdj, 
	                                        ModuleIndices thisModuleIdx,
	                                        ModularityResult& results) {
	mat A = thisAdj(conv_to<uvec>::from(thisModuleIdx), conv_to<uvec>::from(thisModuleIdx));
	double n = thisModuleIdx.size();
	if(n < 2) {
		cerr << "ERROR: GetNewmanModules: Cannot split module of size < 2: " << n << endl;
		return false;
	}
	rowvec k = sum(A, 0);
	double m = 0.5 * sum(k);

	// real symmetric modularity matrix B
	mat B;
	B.resize(n, n);
	colvec nodeDegrees = k.t();
//  mat kcp = nodeDegrees * nodeDegrees.t();
//  cout << "kcp:" << endl;
//  cout << kcp << endl;
	B = A - nodeDegrees * nodeDegrees.t() / (2.0 * m);
//  cout << "initial B:" << endl;
//  cout << B << endl;
  
	// ------------------------- I T E R A T I O N ------------------------------

	stack<vector<unsigned int> > processStack;

	// the starting module is the entire network
	ModuleIndices firstModule;
	for(unsigned int i=0; i < n; ++i) {
		firstModule.push_back(i);
	}
	processStack.push(firstModule);

	// iterate until stack is empty
	Q = 0;
	unsigned int iteration = 0;
	while(!processStack.empty()) {
		++iteration;

		vector<unsigned int> thisModule = processStack.top();
		processStack.pop();
		unsigned int newDim = thisModule.size();

		// get the submatrix Bg defined by the indices of this module (Eqn 6)
		mat Bg(newDim, newDim);
		for(unsigned int l1=0; l1 < newDim; ++l1) {
			for(unsigned int l2=0; l2 < newDim; ++l2) {
				Bg(l1, l2) = B(thisModule[l1], thisModule[l2]);
			}
		}

		// adjust the diagonal
		rowvec rowsums = arma::sum(Bg, 0);
		for(unsigned int i=0; i < rowsums.size(); ++i) {
			Bg(i, i) = Bg(i, i) - rowsums(i);
		}

		// call the community finding/modularity function
		pair<double, vec> sub_modules = ModularityBestSplit(Bg, m);
		double deltaQ = sub_modules.first;
		vec s = sub_modules.second;

		// find the split indices
		vector<unsigned int> s1;
		vector<unsigned int> s2;
		for(unsigned int mi=0; mi < s.size(); ++mi) {
			if(s(mi) > 0) {
				s1.push_back(thisModule[mi]);
			}
			else {
				s2.push_back(thisModule[mi]);
			}
		}

		// have we hit any stopping criteria?
		if((s1.size() == 0) || (s2.size() == 0)) {
			results.second.push_back(thisModule);
			if(iteration == 1) {
				Q = deltaQ;
			}
		}
		else {
			if(deltaQ <= MODULARITY_THRESHOLD) {
				results.second.push_back(thisModule);
			} else {
				// add the splits to the processing stack and recurse
				processStack.push(s1);
				processStack.push(s2);
				// accumulate global Q
				Q += deltaQ;
			}
		}
	}

	results.first = Q;

	return true;
}

bool InteractionNetwork::MergeSmallModules(mat& thisAdj, 
	                                         ModuleList& smallModules,
	                                         ModuleList& results) {
	// keep a map of the orignal matrix indices for results list
  inbixEnv->printLOG("RIPM: Mapping indices for return\n");	
  ModuleIndices optIdx;
	unsigned int newIdx = 0;
	map<unsigned int, unsigned int> origIdxMap;
	for(unsigned int i=0; i < smallModules.size(); ++i) {
		ModuleIndices thisSmallMod = smallModules[i];
		for(unsigned int j=0; j < thisSmallMod.size(); ++j) {
			unsigned int thisMatrixIdx = thisSmallMod[j];
			optIdx.push_back(thisMatrixIdx);
			origIdxMap[newIdx] = thisMatrixIdx;
			++newIdx;
		}
	}
	mat A = thisAdj(conv_to<uvec>::from(optIdx), conv_to<uvec>::from(optIdx));

	// try different power series matrices for indirect paths
	bool found = false;
	unsigned int thisMergeOrder = startMergeOrder;
	while(!found && (thisMergeOrder <= maxMergeOrder)) {
	  inbixEnv->printLOG("RIPM: Merge order: " + int2str(thisMergeOrder) + "\n");	
		mat tryMatrix;
		this->SumMatrixPowerSeries(A, thisMergeOrder, tryMatrix);
		ModularityResult tryResults;
		if(!this->GetNewmanModules(tryMatrix, optIdx, tryResults)) {
			return false;
		}
		if(CheckMergeResults(tryResults)) {
			// SUCCESS!
		  inbixEnv->printLOG("RIPM: Merge successful!\n");	
			// map modules indices back to passed matrix indices
		  inbixEnv->printLOG("RIPM: Mapping return indices back to caller\n");	
			results.clear();
			results.resize(tryResults.second.size());
			for(unsigned int i=0; i < tryResults.second.size(); ++i) {
				ModuleIndices thisModule = tryResults.second[i];
				for(unsigned int j=0; j < thisModule.size(); ++j) {
					unsigned int mappedNode = origIdxMap[thisModule[j]];
					results[i].push_back(mappedNode);
				}
			}
			found = true;
		}
		++thisMergeOrder;	
	}

	return found;
}

bool InteractionNetwork::SumMatrixPowerSeries(mat& A, 
	                                            unsigned int maxPower,
	                                            mat& B) {
  B = A;
  mat currPowerOfA = A;
  if(maxPower >= 2) {
    for(unsigned int i=1; i < maxPower; ++i) {
      currPowerOfA = currPowerOfA * A;
      B = B + currPowerOfA;
    }
  }

  return true;
}

bool InteractionNetwork::CheckMergeResults(ModularityResult results) {
	bool mergeSuccess = true;
	for(unsigned int i=0; i < results.second.size(); ++i) {
		unsigned int thisModuleSize = results.second[i].size();
		if((thisModuleSize < minModuleSize) || (thisModuleSize > maxModuleSize)) {
			mergeSuccess = false;
		}
	}
	return mergeSuccess;
}

ModularityResult InteractionNetwork::ModularityLeadingEigenvector() {

	PrepareConnectivityMatrix();
	mat A = connMatrix;
	degrees = sum(A, 0);
	numEdges = 0.5 * sum(degrees);
	colvec nodeDegrees = degrees.t();

	// real symmetric modularity matrix B
	mat B;
	B.resize(numNodes, numNodes);
	B = A - nodeDegrees * nodeDegrees.t() / (2.0 * numEdges);
  
	// ------------------------- I T E R A T I O N ------------------------------
	stack<vector<unsigned int> > processStack;

	// the starting module is the entire network
	vector<unsigned int> firstModule;
	for(unsigned int i=0; i < numNodes; ++i) {
		firstModule.push_back(i);
	}
	processStack.push(firstModule);

	// iterate until stack is empty
	unsigned int iteration = 0;
	while(!processStack.empty()) {
		++iteration;

		vector<unsigned int> thisModule = processStack.top();
		processStack.pop();
		unsigned int newDim = thisModule.size();

		// get the submatrix Bg defined by the indices of this module (Eqn 6)
		mat Bg(newDim, newDim);
		for(unsigned int l1=0; l1 < newDim; ++l1) {
			for(unsigned int l2=0; l2 < newDim; ++l2) {
				Bg(l1, l2) = B(thisModule[l1], thisModule[l2]);
			}
		}

		// adjust the diagonal
		rowvec rowsums = arma::sum(Bg, 0);
		for(unsigned int i=0; i < rowsums.size(); ++i) {
			Bg(i, i) = Bg(i, i) - rowsums(i);
		}

		// call the community finding/modularity function
		pair<double, vec> sub_modules = ModularityBestSplit(Bg, numEdges);
		double deltaQ = sub_modules.first;
		vec s = sub_modules.second;

		// find the split indices
		vector<unsigned int> s1;
		vector<unsigned int> s2;
		for(unsigned int mi=0; mi < s.size(); ++mi) {
			if(s(mi) > 0) {
				s1.push_back(thisModule[mi]);
			}
			else {
				s2.push_back(thisModule[mi]);
			}
		}

		// have we hit any stopping criteria?
		if((s1.size() == 0) || (s2.size() == 0)) {
			modules.push_back(thisModule);
			if(iteration == 1) {
				Q = deltaQ;
			}
		}
		else {
			if(deltaQ <= MODULARITY_THRESHOLD) {
				modules.push_back(thisModule);
			} else {
				// add the splits to the processing stack and recurse
				processStack.push(s1);
				processStack.push(s2);
				// accumulate global Q
				Q += deltaQ;
			}
		}
	}

	return make_pair(Q, modules);
}

bool InteractionNetwork::Homophily(HomophilyResult& results) {

  if(!modules.size()) {
    error("Cannot compute homphily: no modules exist");
  }

	double globalHomophily = 0.0;
	vector<double> localHomophilies;

	unsigned int totalNodes = connMatrix.n_cols;
	// cout << "Total nodes: " << totalNodes << endl;

	// for each module in the modules list
	for(unsigned int i=0; i < modules.size(); ++i) {

		// get the indices of the nodes in the module
		unsigned int modSize = modules[i].size();
		// cout << "Module size: " << modSize << endl;

		uvec modIndices;
		modIndices.set_size(modSize);
		for(unsigned int mi=0; mi < modSize; ++mi) {
			modIndices(mi) = modules[i][mi];
		}
		// modIndices.print("module indices");

		// get the indices of the nodes not in the module
		uvec notIndices;
		unsigned int notIndex = 0;
		notIndices.set_size(totalNodes - modSize);
		for(unsigned int j=0; j < modules.size(); ++j) {
			if(j != i) {
				for(unsigned int k=0; k < modules[j].size(); ++k) {
					notIndices(notIndex) = modules[j][k];
					++notIndex;
				}
			}
		}
		// notIndices.print("indices NOT in module");

		// get the number of internal connections
		mat modMatrix = connMatrix(modIndices, modIndices);
		double internalConnections = sum(sum(trimatu(modMatrix)));

		// get the number of external connections
		mat notMatrix = connMatrix(modIndices, notIndices);
		double externalConnections = sum(sum(notMatrix));

//		cout << "int: " << internalConnections
//				<< ", ext: " << externalConnections << endl;

		// calculate and save local homophily
		double modHomophily = 0.0;
		if((internalConnections != 0) && (externalConnections != 0)) {
			modHomophily = (internalConnections - externalConnections) /
					(internalConnections + externalConnections);
		}
		// cout << "Module homophily: " << modHomophily << endl;
		double localHomophily = modSize * modHomophily / totalNodes;
		// cout << "Module frac: " << localHomophily << endl;
		localHomophilies.push_back(localHomophily);

		// update global homophily
		globalHomophily += localHomophily;
	}

	results.first = globalHomophily;
	results.second.resize(localHomophilies.size());
	results.second = localHomophilies;

	return true;
}

void InteractionNetwork::ShowHomophily() {
  inbixEnv->printLOG("Q from existing modules: " + dbl2str(ComputeQ()) + "\n");
  HomophilyResult homophily;
  Homophily(homophily);
  inbixEnv->printLOG("Total homophily: " + dbl2str(homophily.first) + "\n");
  unsigned int modIdx = 0;
  for(unsigned int i=0; i < homophily.second.size(); ++i, ++modIdx) {
  	double thisHomMod = homophily.second[i];
    inbixEnv->printLOG("Homophily for module " + int2str(modIdx+1) +
             ": " + dbl2str(thisHomMod) + "\n");
  }
}

double InteractionNetwork::ComputeQ() {
	vector<unsigned int> allModules = FlattenModules();
	if(modules.size() < 2) {
		if(modules.size() < 1) {
			cerr << "ERROR: No modules detected." << endl;
			return 0;
		}
		else {
			cerr << "WARNING: Only one module detected." << endl;
			return 0;
		}
	}

	// m = number of edges
  double m = sum(sum(connMatrix)) * 0.5;
  double q = 0.0;
  for(unsigned int i=0; i < connMatrix.n_cols; ++i) {
    for(unsigned int j=0; j < connMatrix.n_cols; ++j) {
      double temp = (connMatrix(i, j) - degrees(i) * degrees(j) / (2.0 * m)) *
					((double) (allModules[i] == allModules[j]) - 0.5) * 2.0;
      q += temp;
    }
  }
  q /= (4.0 * m);

  return q;
}

bool InteractionNetwork::SetModulesFromFile(string modulesFilename) {
  // parse the module text file
  ifstream modFileHandle(modulesFilename.c_str());
  if(!modFileHandle.is_open()) {
      cerr << "ERROR: Could not open modules file" << modulesFilename << endl;
      return false;
  }

  string line;
  string delimiter = "\t";
  map<string, unsigned int> modMap;
  set<unsigned int> moduleNumbers;
  while(getline(modFileHandle, line)) {
    trim(line);
    if(line == "") {
    	cout << "WARNING: Blank line skipped" << endl;
    	continue;
    }
    vector<string> modValues;
    split(modValues, line);
    // TODO: use inbix conversion here
    int moduleNumber = atoi(modValues[1].c_str());
    moduleNumbers.insert(moduleNumber);
    modMap[modValues[0]] = moduleNumber;
  }
  modFileHandle.close();

  modules.resize(moduleNumbers.size());
  map<string, unsigned int>::const_iterator modIt = modMap.begin();
  for(; modIt != modMap.end(); ++modIt) {
  	string nodeName = modIt->first;
  	unsigned int nodeModule = modIt->second;
  	// get index of node name
  	modules[nodeModule].push_back(nodeNameIndex[nodeName]);
  }

	return true;
}
void InteractionNetwork::ShowModules() {
	inbixEnv->printLOG("Modules:\n");
	for(unsigned int moduleIdx=0; moduleIdx < modules.size(); ++moduleIdx) {
		inbixEnv->printLOG("Nodes in module " + int2str(moduleIdx+1) + ": ");
		for(unsigned int memberIdx=0; memberIdx < modules[moduleIdx].size();
				++memberIdx) {
			inbixEnv->printLOG(nodeNames[modules[moduleIdx][memberIdx]] + " ");
		}
    inbixEnv->printLOG("\n");
	}
}

void InteractionNetwork::SaveModules(string saveFilename) {
	ofstream outputFileHandle(saveFilename.c_str());
  if(outputFileHandle.fail()) {
    error("Could not open network modules file for saving: " + saveFilename + "\n");
  }
  inbixEnv->printLOG("Saving network modules to [" + saveFilename + "]\n");
	for(unsigned int moduleIdx=0; moduleIdx < modules.size(); ++moduleIdx) {
		for(unsigned int memberIdx=0; memberIdx < modules[moduleIdx].size();
				++memberIdx) {
			unsigned int nodeIndex = modules[moduleIdx][memberIdx];
			outputFileHandle << nodeNames[nodeIndex] << "\t" << (moduleIdx + 1) << endl;
		}
	}
	outputFileHandle.close();
}

pair<double, vec> 
  InteractionNetwork::ModularityBestSplit(arma::mat& B, double m) {

 	vec eigval;
	mat eigvec;
	eig_sym(eigval, eigvec, B);
  //cout << "B:" << endl << B << endl;
  //cout << eigvec << endl;
  //cout << eigval << endl;

	uword  maxeig_idx;
	eigval.max(maxeig_idx);
	colvec s_out = eigvec.col(maxeig_idx);
  //cout << s_out << endl;
  //exit(1);
	for(unsigned int i=0; i < s_out.size(); ++i) {
		if(s_out(i) < 0) {
			s_out(i) = -1;
		}
		else {
			s_out(i) = 1;
		}
	}
  //cout << s_out << endl;
  //exit(1);
	mat Q_mat = s_out.t() * B * s_out;
	double Q = Q_mat(0, 0);
	Q *= (1.0 / (m * 4.0));

	return(make_pair(Q, s_out));
}

vector<unsigned int> InteractionNetwork::FlattenModules() {
	unsigned int numModules = modules.size();
	vector<unsigned int> flatModules(numModules);
	if(modules.size()) {
		for(unsigned int i=0; i < numModules; ++i) {
			for(unsigned int j=0; j < modules[i].size(); ++j) {
				flatModules[modules[i][j]] = i;
			}
		}
	}
	else {
		cerr << "FlattenModules: WARNING: no modules have been created" << endl;
		flatModules.clear();
	}
	
	return flatModules;
}

bool InteractionNetwork::Deconvolve(mat& nd, double alpha, double beta, int control) {

  // --------------------------------------------------------------------------
  // check parameters
  if((alpha > 1) || (alpha <= 0)) {
    cerr << "alpha [" << alpha << "] must be in (0,1)" << endl;
    return false;
  }
  if((beta >= 1) || (beta <= 0)) {
    cerr << "alpha [" << beta << "] must be in (0,1)" << endl;
    return false;
  }
  if((control != 0) && (control != 1)) {
    cerr << "control [" << control << "] must be in 0 or 1" << endl;
    return false;
  }
  
  // --------------------------------------------------------------------------
  // process the adjacency matrix
  mat newmat = adjMatrix;
  unsigned int numModules = adjMatrix.n_cols;
  //cout << "Adjacency matrix:" << endl << newmat << endl;
  
  // linear mapping to (0,1)
  newmat = (newmat-min(min(newmat))) / (max(max(newmat))-min(min(newmat)));
  //cout << "Linear map to (0,1):" << endl << newmat << endl;

  // zero diagonal
  newmat.diag() = zeros<vec>(numNodes);
  //cout << "Diagonal removed:" << endl << newmat << endl;

  // quantile threshold
  vector_t allValues;
  for(int i=0; i < numNodes; ++i) {
    for (int j=0; j < numNodes; ++j) {
      allValues.push_back(newmat(i,j));
    }
  }
  double y;
  quantile(allValues, 1-alpha, y);
  //cout << "Threshold to: " << y << endl;
  uvec passInd = find(newmat >= y);
  mat mat_th = zeros<mat>(numNodes, numNodes);
  for(int i=0; i < passInd.size(); ++i) {
    int index = passInd[i];
    int row = index / ((int) numNodes);
    int col = index % ((int) numNodes);
    mat_th(row, col) = newmat(row, col);
  }
  //cout << "Threshold matrix:" << endl << mat_th << endl;

  // make symmetric if not already
  mat_th = (mat_th + mat_th.t()) / 2;
  //cout << "Symmetric:" << endl << mat_th << endl;

  // --------------------------------------------------------------------------
  // eigenvector/value decomposition
 	vec D;
	mat U;
	eig_sym(D, U, mat_th);
  //cout << eigvec << endl;
  //cout << eigval << endl;
  double lam_n = abs(min(D));
  double lam_p = abs(max(D));
  double m1 = lam_p * (1 - beta) / beta;
  double m2 = lam_n * (1 + beta) / beta;
  double m = max(m1, m2);
//  cout << "Eigen calculations:" << endl 
//    << "lam_n: " << lam_n
//    << ", lam_p: " << lam_p
//    << ", m1: " << m1
//    << ", m2: " << m2
//    << ", m: " << m
//    << endl;
  
  // network deconvolution
  for(int i=0; i < D.size(); ++i) {
    D(i) = D(i) / (m + D(i));
  }
  mat mat_new1 = U * diagmat(D) * inv(U);
  //cout << "Eigenvector/value transform:" << endl << mat_new1 << endl;
  
  // --------------------------------------------------------------------------
  // handle "control" parameter
  int n_dim = (int) numNodes;
  mat mat_new2;
  if(control == 0) {
    mat ind_edges = zeros<mat>(n_dim, n_dim);
    uvec nzidx = find(mat_th > 0);
    ind_edges.elem(nzidx) = ones<vec>(nzidx.size());

    mat ind_nonedges = zeros<mat>(numNodes, numNodes);
    uvec zidx = find(mat_th == 0);
    ind_nonedges.elem(zidx) = ones<vec>(zidx.size());

    m1 = max(max(newmat % ind_nonedges));
    m2 = min(min(mat_new1));
    //cout << "control = 0: m1: " << m1 << ", m2: " << m2 << endl;
    mat_new2 = (mat_new1 + max(m1 - m2, 0.0)) % ind_edges + (newmat % ind_nonedges);
  }
  else {
    m2 = min(min(mat_new1));
    mat_new2 = (mat_new1 + max(-m2, 0.0));
  }
  //cout << "After control parameter handling:" << endl << mat_new2 << endl;

  // linear mapping of deconvolved matrix to (0,1)
  m1 = min(min(mat_new2));
  m2 = max(max(mat_new2));
  //cout << "m1: " << m1 << ", m2: " << m2 << endl;
  nd = (mat_new2 - m1) / (m2 - m1);
  //cout << "Deconvolved matrix:" << endl << nd << endl;
  
  return true;
}
