#ifndef EVAPORATIVECOOLINGPRIVACY_H
#define EVAPORATIVECOOLINGPRIVACY_H

/* 
 * File:   EvaporativeCoolingPrivacy.h
 * Author: bwhite
 *
 * Created on October 18, 2016, 10:57 PM
 */

#include <string>
#include <vector>
#include <armadillo>
#include <random>

#include "plink.h"

#include "Dataset.h"

enum DATASET_TYPE {
  TRAIN, 
  HOLDOUT, 
  TEST
};

class EvaporativeCoolingPrivacy {
public:
  EvaporativeCoolingPrivacy(Dataset* trainset, Dataset* holdoset, 
                            Dataset* testset, Plink* plinkPtr);
  virtual ~EvaporativeCoolingPrivacy();
  bool ComputeScores();
private:
  bool ComputeInverseImportance();
  bool ComputeImportance();
  double DeltaQ();
  bool ComputeProbabilities();
  bool GenerateUniformRands();
  bool EvaporateWorst();
  double ClassifyAttributeSet(std::vector<std::string> attrs, DATASET_TYPE);
  bool ComputeBestAttributesErrors();
  bool UpdateTemperature();

  // PLINK environment Plink object
  Plink* PP;

  // CONSTANTS
  double Q_EPS;
  uint MAX_TICKS;
  
  // classification data sets
  Dataset* train;
  Dataset* holdout;
  Dataset* test;
  std::vector<std::string> setOfAllAttributes;

  // importance/quality scores
	AttributeScores trainImportance;       // q_t
	AttributeScores holdoutImportance;     // q_h
	AttributeScores trainInvImportance;    // p_t
	AttributeScores holdoutInvImportance;  // p_h
  double delta_q;
  
  // temperature schedule
  double T_start;
  double T_current;
  double T_final;

  // algorithm constants
  uint numAttributes;
  double probBiological; // probability biological influence
  uint numInstances;
  uint maxPerIteration;
  uint kConstant;

  // evaporation variables
  std::vector<double> probAttributeSelection;
  std::mt19937_64 engine;  // Mersenne twister random number engine
  std::vector<double> randUniformProbs;
  std::vector<std::string> removeAttrs;
  std::vector<std::string> keepAttrs;

  // classification/prediction errors
  double randomForestPredictError;
  double trainError;
  double holdError;
  double testError;
  std::vector<double> trainErrors;
  std::vector<double> holdoutErrors;
  std::vector<double> testErrors;
};

#endif /* EVAPORATIVECOOLINGPRIVACY_H */