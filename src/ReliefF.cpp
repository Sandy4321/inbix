/*
 * ReliefF.cpp - Bill White - 7/16/05
 * 
 * ReliefF algorithm implementation.
 *
 * Totally redone for the McKinney In Silico Lab in 2011.
 * Using OpenMP for multi-core parallelization - April 2011.
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <cmath>
#include <sstream>

#include <omp.h>

#include "ReliefF.h"
#include "Dataset.h"
#include "DatasetInstance.h"
#include "StringUtils.h"
#include "DistanceMetrics.h"
#include "Insilico.h"

#include "plink.h"
#include "options.h"
#include "helper.h"

using namespace std;
using namespace insilico;

/// scores map: score->attribute index
typedef vector<pair<double, unsigned int> > ScoresMap;
/// scores map iterator
typedef vector<pair<double, unsigned int> >::iterator ScoresMapIt;
/// attribute index map: attribute index->score
typedef vector<pair<unsigned int, double> > AttributeIndex;
/// attribute index map iterator
typedef vector<pair<unsigned int, double> >::const_iterator AttributeIndexIt;

/// attribute score sorting functor
bool scoreSort(const pair<double, string>& p1, const pair<double, string>& p2) {
  return p1.first < p2.first;
}

/// attribute index sorting functor
bool attributeSort(const pair<unsigned int, double>& p1,
        const pair<unsigned int, double>& p2) {
  return p1.first < p2.first;
}

/// functor for T comparison
typedef pair<unsigned int, DatasetInstance*> T;

class deref_less : public std::binary_function<T, T, bool> {
public:

  bool operator()(const T a, const T b) const {
    return(a.first < b.first);
  }
};

ReliefF::ReliefF(Dataset* ds, Plink* plinkPtr, AnalysisType anaType):
AttributeRanker::AttributeRanker(ds) {
  cout << Timestamp() << "ReliefF default initialization without "
          << "configuration parameters" << endl;
  if(ds) {
    dataset = ds;
  } else {
    cerr << "ERROR: dataset is not initialized" << endl;
    exit(-1);
  }
  analysisType = anaType;

  m = dataset->NumInstances();
  cout << Timestamp() << "Number of samples: m = " << m << endl;
  if(m == 0 || m == ds->NumInstances()) {
    // sample deterministically unless a sample size has been set
    cout << Timestamp() << "Sampling all instances deterministically"
            << endl;
    randomlySelect = false;
    m = ds->NumInstances();
  } else {
    cout << Timestamp() << "Sampling instances randomly" << endl;
    randomlySelect = true;
  }

  weightByDistanceMethod = par::weightByDistanceMethod;
    
  SetK(par::k);
  if(k) {
    cout << Timestamp() << "Number of nearest neighbors: k = " << k << endl;
    // k nearest neighbors and m randomly selected instances
    // spread differences and thus weight updates
    // over (m x k) iterations
    // double one_over_m_times_k = 1.0 / (((double) m) * ((double) k));
    //                               m       *           k
  } else {
    cout << Timestamp() << "k nearest neighbors will be optimized" << endl;
  }

  cout << Timestamp() << "Number of samples: m = " << m << endl;
  randomlySelect = true;
  if(m == 0 || m == ds->NumInstances()) {
    // sample deterministically unless a sample size has been set
    cout << Timestamp() << "Sampling all instances deterministically"
            << endl;
    randomlySelect = false;
    m = ds->NumInstances();
  } else {
    cout << Timestamp() << "Sampling instances randomly" << endl;
    randomlySelect = true;
  }

  removePerIteration = par::reliefIterNumToRemove;
  if(removePerIteration) {
    if((removePerIteration < 1)
            || (removePerIteration >= dataset->NumAttributes())) {
      error("ERROR: Number to remove per iteratopn [" +
              int2str(removePerIteration) + "] not in valid range");
    }
    cout << Timestamp() << "Iteratively removing " << removePerIteration
            << endl;
  } else {
    removePercentage = static_cast<unsigned int>(par::reliefIterPercentToRemove) / 100.0;
    if(removePercentage) {
      doRemovePercent = true;
      removePerIteration =
              (unsigned int) ((double) dataset->NumAttributes()
              * removePercentage + 0.5);
      if((removePerIteration < 1)
              || (removePerIteration >= dataset->NumAttributes())) {
        cerr << "ERROR: Number to remove per iteration ["
                << removePerIteration << "] not in valid range" << endl;
        exit(-1);
      }
      cout << Timestamp() << "Iteratively removing "
              << (removePercentage * 100) << "% = " << removePerIteration
              << endl;
    }
  }
  cout << Timestamp() << "Iteratively removing " << removePerIteration
          << endl;

  /// set the SNP metric function pointer based on command line params or defaults
  snpMetric = par::snpMetric;
  numMetric = par::numMetric;
  bool snpMetricFunctionUnset = true;
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "GM") {
    snpDiff = diffGMM;
    snpMetricFunctionUnset = false;
  }
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "AM") {
    snpDiff = diffAMM;
    snpMetricFunctionUnset = false;
  }
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "NCA") {
    snpDiff = diffNCA;
    snpMetricFunctionUnset = false;
  }
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "NCA6") {
    snpDiff = diffNCA6;
    snpMetricFunctionUnset = false;
  }
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "GRM") {
    // no need to set a function pointer here for GRM
    error("GCTA GRM metric is not allowed in weight update metric, only nearest neighbors");
  }
  if(snpMetricFunctionUnset && to_upper(snpMetric) == "KM") {
    cerr << "ERROR: KM is not supported as a ReliefF metric" << endl;
    exit(EXIT_FAILURE);
    // snpDiff = diffKM;
    // snpMetricFunctionUnset = false;
  }
  if(snpMetricFunctionUnset) {
    cerr << "ERROR: Cannot set SNP metric to [" << snpMetric << "]" << endl;
    exit(EXIT_FAILURE);
  }
  if(to_upper(numMetric) == "MANHATTAN") {
    numDiff = diffManhattan;
  } else {
    if(to_upper(numMetric) == "EUCLIDEAN") {
      numDiff = diffEuclidean;
    } else {
      cerr << "ERROR: [" << numMetric
              << "] is not a valid numeric metric type" << endl;
      exit(EXIT_FAILURE);
    }
  }

  cout << Timestamp() << "ReliefF SNP distance metric: " << snpMetric << endl;
  cout << Timestamp() << "ReliefF continuous distance metric: " << numMetric
          << endl;
  
  weightByDistanceMethod = par::weightByDistanceMethod;
  if((weightByDistanceMethod != "exponential")
          && (weightByDistanceMethod != "equal")) {
    error("ERROR: Invalid --weight-by-distance-method: " + weightByDistanceMethod);
  }
  weightByDistanceSigma = static_cast<double>(par::weightByDistanceSigma);
  cout << Timestamp() << "Weight by distance method: "
          << weightByDistanceMethod;
  if(weightByDistanceMethod == "exponential") {
    cout << Timestamp() << ", using sigma = " << weightByDistanceSigma
            << endl;
  } else {
    cout << endl;
  }

  int numProcs = omp_get_num_procs();
  int numThreads = omp_get_num_threads();
  cout << Timestamp() << numProcs << " OpenMP processors available" << endl;
  cout << Timestamp() << numThreads << " OpenMP threads in work team" << endl;

  vector<string> atrNames = dataset->GetAttributeNames();
  vector<string> numNames = dataset->GetNumericsNames();
  scoreNames.resize(atrNames.size() + numNames.size());
  copy(atrNames.begin(), atrNames.end(), scoreNames.begin());
  copy(numNames.begin(), numNames.end(),
          scoreNames.begin() + atrNames.size());

}

ReliefF::~ReliefF() {
}

bool ReliefF::ComputeAttributeScores() {

  // changed from matrix to map for ID matching - November 2011
  PreComputeDistances();

  /// algorithm line 1
	W.clear();
  W.resize(dataset->NumVariables(), 0.0);

  // pointer to the instance being sampled
  DatasetInstance* R_i;
  int i = 0;
  cout << Timestamp() << "Running Relief-F algorithm" << endl;
  double one_over_m_times_k = 1.0 / (((double) m) * ((double) k));
  cout << Timestamp() << "Averaging factor 1/(m*k): "  
          << one_over_m_times_k << endl;

  vector<string> instanceIds = dataset->GetInstanceIds();
//  for(i = 0; i < (int) m; i++) {
//    cout << instanceIds[i] << endl;
//  }
  /// algorithm line 2
  for(i = 0; i < (int) m; i++) {
    // algorithm line 3
    if(randomlySelect) {
      // randomly sample an instance (without replacement?)
      R_i = dataset->GetRandomInstance();
    } else {
      // deterministic/indexed instance sampling, ie, every instance against
      // every other instance
      unsigned int instanceIndex;
      dataset->GetInstanceIndexForID(instanceIds[i], instanceIndex);
      R_i = dataset->GetInstance(instanceIndex);
    }
    if(!R_i) {
      cerr
              << "ERROR: Random or indexed instance count not be found for index: ["
              << i << "]" << endl;
      return false;
    }
    ClassLevel class_R_i = R_i->GetClass();

    /// algorithm lines 4, 5 and 6
    // find k nearest hits and nearest misses
    vector<unsigned int> hits;
    map<ClassLevel, vector<unsigned int> > misses;
    bool canGetNeighbors = false;
    canGetNeighbors = R_i->GetNNearestInstances(k, hits, misses);
    //    cout << "Instance class: " << R_i->GetClass() << ", hits: ";
    //    for(unsigned int ii = 0; ii < hits.size(); ++ii) {
    //      cout << hits[ii] << " ";
    //    }
    //    cout << endl << "Misses:" << endl;
    //    map<ClassLevel, vector<unsigned int> >::const_iterator iit;
    //    for(iit = misses.begin(); iit != misses.end(); ++iit) {
    //      cout << "Class: " << iit->first << ", misses: ";
    //      vector<unsigned int> ids = iit->second;
    //      for(unsigned int jj = 0; jj < ids.size(); ++jj) {
    //        cout << ids[jj] << " ";
    //      }
    //      cout << endl;
    //    }

    if(!canGetNeighbors) {
      cerr << "ERROR: relieff cannot get " << k << " nearest neighbors"
              << endl;
      return false;
    }

    // check algorithm preconditions
    if(hits.size() < 1) {
      cerr << "ERROR: No nearest hits found" << endl;
      return false;
    }
    if(hits.size() < k) {
      cerr << "ERROR: Could not find enough neighbors that are hits"
              << endl;
      exit(1);
    }
    map<ClassLevel, vector<unsigned int> >::const_iterator it;
    for(it = misses.begin(); it != misses.end(); ++it) {
      vector<unsigned int> missIds = it->second;
      if(missIds.size() < 1) {
        cerr << "ERROR: No nearest misses found" << endl;
        return false;
      }
      if(missIds.size() < k) {
        cerr << "ERROR: Could not find enough neighbors that are misses"
                << endl;
        return false;
      }
      if(missIds.size() != hits.size()) {
        cerr
                << "ERROR: Could not find equal number of neighbors for hits and misses:"
                << hits.size() << " vs. " << misses.size() << endl;
        return false;
      }
    }

    // UPDATE WEIGHTS FOR ATTRIBUTE 'A' BASED ON THIS AND NEIGHBORING INSTANCES
    // update weights/relevance scores for each attribute averaged
    // across k nearest neighbors and m (possibly randomly) selected instances
    unsigned int A = 0;
    unsigned int scoresIdx = 0;
    if(dataset->HasGenotypes()) {
      vector<unsigned int> attributeIndicies =
              dataset->MaskGetAttributeIndices(DISCRETE_TYPE);
      /// algorithm line 7
      for(unsigned int attrIdx = 0; attrIdx < attributeIndicies.size();
              ++attrIdx) {
        A = attributeIndicies[attrIdx];
        double hitSum = 0.0, missSum = 0.0;
        /// algorithm line 8
        for(unsigned int j = 0; j < k; j++) {
          DatasetInstance* H_j = dataset->GetInstance(hits[j]);
          double rawDistance = snpDiff(A, R_i, H_j);
          hitSum += (rawDistance * one_over_m_times_k);
        }
        /// algorithm line 9
        map<ClassLevel, vector<unsigned int> >::const_iterator mit;
        for(mit = misses.begin(); mit != misses.end(); ++mit) {
          ClassLevel C = mit->first;
          vector<unsigned int> missIds = mit->second;
          double P_C = dataset->GetClassProbability(C);
          double P_C_R = dataset->GetClassProbability(class_R_i);
          double adjustmentFactor = P_C / (1.0 - P_C_R);
          double tempSum = 0.0;
          for(unsigned int j = 0; j < k; j++) {
            DatasetInstance* M_j = dataset->GetInstance(missIds[j]);
            double rawDistance = snpDiff(A, R_i, M_j);
            tempSum += (rawDistance * one_over_m_times_k);
          } // nearest neighbors
          missSum += (adjustmentFactor * tempSum);
        }

        W[scoresIdx] = W[scoresIdx] - hitSum + missSum;
        ++scoresIdx;
      } // all attributes
    } // has genotypes

    // loop here for numeric attributes if they exist - 6/19/11
    if(dataset->HasNumerics()) {
      vector<unsigned int> numericIndices =
              dataset->MaskGetAttributeIndices(NUMERIC_TYPE);
      for(unsigned int numIdx = 0; numIdx < numericIndices.size();
              ++numIdx) {
        A = numericIndices[numIdx];
        double hitSum = 0.0, missSum = 0.0;
        for(unsigned int j = 0; j < k; j++) {
          DatasetInstance* H_j = dataset->GetInstance(hits[j]);
          hitSum += (numDiff(A, R_i, H_j) * one_over_m_times_k);
        }

        map<ClassLevel, vector<unsigned int> >::const_iterator mit;
        for(mit = misses.begin(); mit != misses.end(); ++mit) {
          ClassLevel C = mit->first;
          vector<unsigned int> missIds = mit->second;
          double P_C = dataset->GetClassProbability(C);
          double P_C_R = dataset->GetClassProbability(class_R_i);
          double adjustmentFactor = P_C / (1.0 - P_C_R);
          double tempSum = 0.0;
          for(unsigned int j = 0; j < k; j++) {
            DatasetInstance* M_j = dataset->GetInstance(missIds[j]);
            tempSum += (numDiff(A, R_i, M_j) * one_over_m_times_k);
          } // nearest neighbors
          missSum += (adjustmentFactor * tempSum);
        }
        W[scoresIdx] = W[scoresIdx] - hitSum + missSum;
        ++scoresIdx;
      }
    } // has numerics

    // happy lights
    if(i && ((i % 100) == 0)) {
      cout << Timestamp() << i << "/" << m << endl;
    }

  } // number to randomly select
  cout << Timestamp() << i << "/" << m << " done" << endl;

  return true;
}

bool ReliefF::ComputeAttributeScoresIteratively() {
  // final scores after all iterations
  std::map<std::string, double> finalScores;

  // save the current dataset mask
  dataset->MaskPushAll();

  // IterativeReliefF or TuRF (Tuned Relief-F)
  unsigned int iterations = 1;
  while(dataset->NumVariables() > 0) {

    cout << Timestamp()
            << "------------------------------------------------------------"
            << "-----------------------------------------" << endl;
    cout << Timestamp() << "[" << iterations << "] Working attributes: "
            << dataset->NumVariables() << endl;

    ComputeAttributeScores();
    vector<pair<double, string> > attributeScores = GetScores();

    // save worst attributes and remove from consideration on next iteration
    sort(attributeScores.begin(), attributeScores.end(), scoreSort);
    unsigned int removeThisIteration = 0;
    if(dataset->NumVariables() < removePerIteration) {
      removeThisIteration = dataset->NumVariables();
    } else {
      if(doRemovePercent) {
        removeThisIteration =
                (unsigned int) ((double) dataset->NumAttributes()
                * removePercentage + 0.5);
      } else {
        removeThisIteration = removePerIteration;
      }
    }
    for(unsigned int i = 0; i < removeThisIteration; ++i) {
      string attributeToDelete = attributeScores[i].second;
      //      cout << "\t\t\tremoving attribute: " << attributeToDelete << endl;

      if(!dataset->MaskRemoveVariable(attributeToDelete)) {
        cerr << "ERROR: ReliefF::ComputeAttributeScoresIteratively: "
                << "could not find attribute name in data set: "
                << attributeToDelete << endl;
        return false;
      }
      finalScores[attributeToDelete] = attributeScores[i].first;
    }

    ++iterations;

    ResetForNextIteration();
  } // iterate

  // populate finalScores with remaining scores
  vector<double>::const_iterator scoresIt;
  vector<string> attrNames = dataset->GetAttributeNames();
  //  cout << "Scores: " << scores.size()
  //          << ", Attribute names: " << attrNames.size()
  //          << ", Final Scores: " << finalScores.size() << endl;
  for(unsigned int i = 0; i < attrNames.size(); ++i) {
    cout << attrNames[i] << " => " << scoresIt[i] << endl;
    finalScores[attrNames[i]] = W[i];
  }

  W.resize(scoreNames.size());
  for(unsigned int i = 0; i < scoreNames.size(); ++i) {
    if(finalScores.find(scoreNames[i]) == finalScores.end()) {
      cerr << "ERROR: Logic error. See Bill" << endl;
      exit(1);
    }
    W[i] = finalScores[scoreNames[i]];
  }

  // restore the dataset attribute mask
  dataset->MaskPopAll();

  return true;
}

bool ReliefF::ResetForNextIteration() {

  PreComputeDistances();

  return true;
}

void ReliefF::PrintAttributeScores(ofstream & outFile) {
  vector<double>::const_iterator scoresIt = W.begin();
  unsigned int nameIdx = 0;
  for(; scoresIt != W.end(); ++scoresIt) {
    outFile << *scoresIt << "\t" << scoreNames[nameIdx] << endl;
    ++nameIdx;
  }
}

void ReliefF::WriteAttributeScores(string baseFilename) {

  string resultsFilename = baseFilename;
  if(dataset->HasContinuousPhenotypes()) {
    resultsFilename += ".rrelieff";
  } else {
    resultsFilename += ".relieff";
  }

  ofstream outFile;
  outFile.open(resultsFilename.c_str());
  if(outFile.bad()) {
    cerr << "ERROR: Could not open scores file " << resultsFilename
            << "for writing" << endl;
    exit(1);
  }
  PrintAttributeScores(outFile);
  outFile.close();
}

bool ReliefF::PreComputeDistances() {
  cout << Timestamp() << "Precomputing instance distances" << endl;
  map<string, unsigned int> instanceMask = dataset->MaskGetInstanceMask();
  vector<string> instanceIds = dataset->MaskGetInstanceIds();
  int numInstances = instanceIds.size();

  // create a distance matrix
  cout << Timestamp() << "Allocating distance matrix";
  double** distanceMatrix;
  distanceMatrix = new double*[numInstances];
  for(int i = 0; i < numInstances; ++i) {
    distanceMatrix[i] = new double[numInstances];
    for(int j = 0; j < numInstances; ++j) {
      distanceMatrix[i][j] = 0.0;
    }
  }
  cout << " done" << endl;

  // TCGA genetic relationship matrix (GRM))
  if(par::snpMetricNN == "grm") {
    cout << Timestamp() 
            << "1) Computing instance-to-instance distances with GCTA " 
            << "genetic relationship matrix (GRM) ... "
            << endl;
    vector<double> p = dataset->GetMAFs();
    unsigned int N = dataset->NumAttributes();
    double A_jk = 0;
#pragma omp parallel for schedule(dynamic,1)
    for(int j = 0; j < numInstances; ++j) {
      // NOTE index variable names chosen to match GCTA paper
      for(int k = j; k < numInstances; ++k) {
//        unsigned int dsi1Index;
//        dataset->GetInstanceIndexForID(instanceIds[j], dsi1Index);
//        unsigned int dsi2Index;
//        dataset->GetInstanceIndexForID(instanceIds[k], dsi2Index);
        double sum = 0.0;
        for(int i = 0; i < N; ++i) {
          AttributeLevel x_ij = dataset->GetInstance(j)->GetAttribute(i);
          AttributeLevel x_ik = dataset->GetInstance(k)->GetAttribute(i);
          double p_i = p[i];
          double two_p_i = 2 * p_i;
          double summation_expr = 0;
          if(j == k) {
            summation_expr = 
                    ((x_ij * x_ij - ((1 + two_p_i)) * x_ij) + (two_p_i * two_p_i)) / 
                    (two_p_i * (1 - p_i));
          } else {
            summation_expr = 
                    ((x_ij - two_p_i) * (x_ik - two_p_i)) / 
                    (two_p_i * (1 - p_i));
          }
          sum += summation_expr;
        }
        if(j == k) {
          A_jk = 1 + (sum / N);
          distanceMatrix[j][k] = (1- A_jk);
        } else {
          A_jk = sum / N;
          distanceMatrix[j][k] = distanceMatrix[k][j] = (1 - A_jk);
        }
      }
      if(j && (j % 100 == 0)) {
        cout << Timestamp() << j << "/" << numInstances << endl;
      }
    }
    cout << Timestamp() << numInstances << "/" << numInstances << " done"
            << endl;
    // write GRM as the algorithm runs
    cout << Timestamp() << "[ " << par::output_file_name << ".grm.tab ]" << endl;
    ofstream outFile(par::output_file_name + ".grm.tab");
    for(int i=0; i < numInstances; ++i) {
      for(int j=0; j < numInstances; ++j) {
        if(j) {
          outFile << "\t" << distanceMatrix[i][j];  
        } else {
          outFile << distanceMatrix[i][j];  
        }
      }
      outFile << endl;
    }
    outFile.close();
  } else {
    // populate the matrix - upper triangular
    // NOTE: make complete symmetric matrix for neighbor-to-neighbor sums
    cout << Timestamp() << "1) Computing instance-to-instance distances with ... " << endl;
#pragma omp parallel for schedule(dynamic,1)
    for(int i = 0; i < numInstances; ++i) {
      // cout << "Computing instance to instance distances. Row: " << i << endl;
      // #pragma omp parallel for
      for(int j = i + 1; j < numInstances; ++j) {
        unsigned int dsi1Index;
        dataset->GetInstanceIndexForID(instanceIds[i], dsi1Index);
        unsigned int dsi2Index;
        dataset->GetInstanceIndexForID(instanceIds[j], dsi2Index);
        /// be sure to call Dataset::ComputeInstanceToInstanceDistance
        distanceMatrix[i][j] = distanceMatrix[j][i] =
                dataset->ComputeInstanceToInstanceDistance(
                dataset->GetInstance(dsi1Index),
                dataset->GetInstance(dsi2Index));
        //cout << i << ", " << j << " => " << distanceMatrix[i][j] << endl;
      }
      if(i && (i % 100 == 0)) {
        cout << Timestamp() << i << "/" << numInstances << endl;
      }
    }
    cout << Timestamp() << numInstances << "/" << numInstances << " done"
            << endl;
  }
  //  DEBUG
//    ofstream outFile;
//    outFile.open("distanceMatrix.csv");
//    for(unsigned int i=0; i < dataset->NumInstances(); ++i) {
//      for(unsigned int j=0; j < dataset->NumInstances(); ++j) {
//        if(j)
//          outFile << "," << distanceMatrix[i][j];
//        else
//          outFile << distanceMatrix[i][j];
//      }
//      outFile << endl;
//    }
//    outFile.close();
  //  DEBUG

  // for each instance: if discrete class, store the distance sums for same
  // and different classes, else store distances to all other instances
  // (regression ReliefF)
  if(dataset->HasContinuousPhenotypes()) {
    cout << Timestamp()
            << "2) Calculating continuous phenotype nearest neighbors... ";
  } else {
    // multiclass - 12/1/11
    if(dataset->NumClasses() > 2) {
      cout << Timestamp()
              << "2) Calculating same and different classes nearest neighbors... ";
    } else {
      cout << Timestamp()
              << "2) Calculating same and different class nearest neighbors... ";
    }
  }
  cout << endl;

  DistancePair nnInfo;
  for(int i = 0; i < numInstances; ++i) {
    unsigned int thisInstanceIndex = instanceMask[instanceIds[i]];
    DatasetInstance* thisInstance = dataset->GetInstance(thisInstanceIndex);

    if(dataset->HasContinuousPhenotypes()) {
      DistancePairs instanceDistances;
      for(int j = 0; j < numInstances; ++j) {
        if(i == j)
          continue;
        double instanceToInstanceDistance = distanceMatrix[i][j];
        DistancePair nearestNeighborInfo;
        nearestNeighborInfo = make_pair(instanceToInstanceDistance,
                instanceIds[j]);
        instanceDistances.push_back(nearestNeighborInfo);
      }
      thisInstance->SetDistanceSums(k, instanceDistances);
    } else {
      ClassLevel thisClass = thisInstance->GetClass();
      DistancePairs sameSums;
      // changed to an array for multiclass - 12/1/11
      map<ClassLevel, DistancePairs> diffSums;
      for(int j = 0; j < numInstances; ++j) {
        if(i == j)
          continue;
        double instanceToInstanceDistance = distanceMatrix[i][j];
        unsigned int otherInstanceIndex = instanceMask[instanceIds[j]];
        DatasetInstance* otherInstance = dataset->GetInstance(
                otherInstanceIndex);
        nnInfo = make_pair(instanceToInstanceDistance, instanceIds[j]);
        if(otherInstance->GetClass() == thisClass) {
          sameSums.push_back(nnInfo);
        } else {
          ClassLevel otherClass = otherInstance->GetClass();
          diffSums[otherClass].push_back(nnInfo);
        }
      }
      thisInstance->SetDistanceSums(k, sameSums, diffSums);
    }

    if(i && (i % 100 == 0)) {
      cout << Timestamp() << i << "/" << numInstances << endl;
    }
  }
  cout << Timestamp() << numInstances << "/" << numInstances << " done"
          << endl;

  cout << Timestamp() << "3) Calculating weight by distance factors for "
          << "nearest neighbors... " << endl;
  ComputeWeightByDistanceFactors();

  // release the dynamically-allocated distance matrix
  cout << Timestamp() << "Freeing distance matrix memory";
  for(int i = 0; i < numInstances; ++i) {
    delete[] distanceMatrix[i];
  }
  delete[] distanceMatrix;
  cout << " done" << endl;

  return true;
}

AttributeScores ReliefF::GetScores() {

  AttributeScores returnScores;
  vector<double>::const_iterator scoresIt = W.begin();
  unsigned int nameIdx = 0;
  vector<string> maskNames = dataset->MaskGetAllVariableNames();
  for(; scoresIt != W.end(); ++scoresIt) {
    returnScores.push_back(make_pair(*scoresIt, maskNames[nameIdx]));
    ++nameIdx;
  }
  return returnScores;
}

AttributeScores ReliefF::ComputeScores() {
  ComputeAttributeScores();
  return GetScores();
}

bool ReliefF::ComputeWeightByDistanceFactors() {

  vector<string> instanceIds = dataset->GetInstanceIds();
  for(unsigned int i = 0; i < dataset->NumInstances(); ++i) {

    // this instance
    unsigned int instanceIndex;
    dataset->GetInstanceIndexForID(instanceIds[i], instanceIndex);
    DatasetInstance* dsi = dataset->GetInstance(instanceIndex);

    vector<double> d1_ij;
    double d1_ij_sum = 0.0;
    for(unsigned int rank_j = 1; rank_j <= k; ++rank_j) {
      double d1_ij_value = 0.0;
      if(weightByDistanceMethod == "exponential") {
        double exponentArg = (double) rank_j / weightByDistanceSigma;
        d1_ij_value = exp(-(exponentArg * exponentArg));
      } else {
        if(weightByDistanceMethod == "one_over_k") {
          d1_ij_value = 1.0 / (double) rank_j;
        } else {
          // equal
          d1_ij_value = 1.0 / (double) k;
        }
      }
      d1_ij.push_back(d1_ij_value);
      d1_ij_sum += d1_ij_value;
    }

    // "normalize" the factors - divide through by the total/sum
    dsi->ClearInfluenceFactors();
    for(unsigned int neighborIdx = 0; neighborIdx < k; ++neighborIdx) {
      double influenceFactorD = d1_ij[neighborIdx] / d1_ij_sum;
      //      cout << "d_ij: " << d1_ij[neighborIdx]
      //              << ", cummulative sum: " << d1_ij_sum
      //              << ", normalized value: " << influenceFactorD << endl;
      dsi->AddInfluenceFactorD(influenceFactorD);
    }
    //    cout << "---------------------------------------------------------" << endl;
  } // end all instances

  return true;
}

void librelieff_is_present(void) {
  ;
}