/**
 * \class EvaporativeCooling
 *
 * \brief Evaporative Cooling attribute ranking algorithm.
 *
 * Implements the Evaporative Cooling algorithm in:
 * McKinney, et. al. "Capturing the Spectrum of Interaction Effects in Genetic
 * Association Studies by Simulated Evaporative Cooling Network Analysis."
 * PLoS Genetics, Vol 5, Issue 3, 2009.
 *
 * \sa ReliefF
 * \sa RReliefF
  *
 * \author Bill White
 * \version 2.0
 *
 * Contact: bill.c.white@gmail.com
 * Created on: 7/14/11
 *
 * Made even more generic with main effects and interaction effects algorithms
 * in a class hierarchy from a AttributeRanker base. 8/12/12
 * 
 * Modified for inbix. 9/29/16
 */

#ifndef EVAPORATIVECOOLING_H
#define	EVAPORATIVECOOLING_H

#include <vector>

#include "AttributeRanker.h"
#include "Dataset.h"
#include "Insilico.h"
#include "plink.h"

class EvaporativeCooling {
public:
	/*************************************************************************//**
	 * Construct an EC algorithm object.
	 * \param [in] ds pointer to a Dataset object
	 * \param [in] vm reference to a Boost map of command line options
	 * \param [in] anaType analysis type
	 ****************************************************************************/
	EvaporativeCooling(Dataset* ds, Plink* plinkPtr, 
                     AnalysisType anaType=SNP_ONLY_ANALYSIS);
	virtual ~EvaporativeCooling();
	/// Compute the EC scores based on the current set of attributes.
	bool ComputeECScores();
	/// Get the last computed RandomJungle scores.
	AttributeScores& GetMaineffectScores();
	/// Get the last computed ReliefF scores.
	AttributeScores& GetInteractionScores();
	/// Get the last computed EC scores.
	AttributeScores& GetECScores();
	/// Return the algorithm type.
	EcAlgorithmType GetAlgorithmType();
	/*************************************************************************//**
	 * Write the scores and attribute names to file.
	 * \param [in] baseFilename filename to write score-attribute name pairs
	 ****************************************************************************/
	void WriteAttributeScores(std::string baseFilename);
	/*************************************************************************//**
	 * Write the classification errors to file.
	 * \param [in] filename filename to write classification errors
	 ****************************************************************************/
	void WriteClassificationErrors(std::string filename);
	/*************************************************************************//**
	 * Write the temperatures to file.
	 * \param [in] filename filename to write temperatures
	 ****************************************************************************/
	void WriteTemperatures(std::string filename);
	/*************************************************************************//**
	 * Write the EC scores and attribute names to stream.
	 * \param [in] outStream stream to write score-attribute name pairs
	 ****************************************************************************/
	void PrintAttributeScores(std::ofstream& outStream);
	/*************************************************************************//**
	 * Write the EC scores and attribute names to stream, includes evaporated
	 * attributes with score of zero.
	 * \param [in] outStream stream to write score-attribute name pairs
	 ****************************************************************************/
	void PrintAllAttributeScores(std::ofstream& outStream);
	/*************************************************************************//**
	 * Write the main effect scores and attribute names to stream.
	 * \param [in] outStream stream to write score-attribute name pairs
	 ****************************************************************************/
	void PrintMainEffectAttributeScores(std::ofstream& outStream);
	/*************************************************************************//**
	 * Write the interaction scores and attribute names to stream.
	 * \param [in] outStream stream to write score-attribute name pairs
	 ****************************************************************************/
	void PrintInteractionAttributeScores(std::ofstream& outStream);
	/// Print the current attributes scores to stdout in tab-delimited format.
	bool PrintAllScoresTabular();
	/// Print the Kendall taus between the main effects and interactions scores.
	bool PrintKendallTaus();
private:
	/// Run the random forest algorithm.
	bool RunRandomForest();
	/// Run the ReliefF algorithm.
	bool RunReliefF();
	/*************************************************************************//**
	 * Compute the attribute free energy using the couple temperature.
	 * \param [in] temperature coupling temperature T
	 * \return distance
	 ****************************************************************************/
	bool ComputeFreeEnergy(double temperature);
	/*************************************************************************//**
	 * Remove the worst attribute based on free energy scores.
	 * \param [in] numToRemove number of attributes to remove/evaporate
	 * \return distance
	 ****************************************************************************/
	bool RemoveWorstAttributes(unsigned int numToRemove=1);
	/// optimize the temperature coupling constant
	double OptimizeTemperature(std::vector<double> deltas);
	/// use Random Jungle to compute the classification error of the current
	/// set of attributes with numToRemovePerIteration attributes removed
	double ComputeClassificationErrorRandomForest();

	/// pointer to a Dataset object
	Dataset* dataset;
	/// command line parameters map
	Plink* PP;

	/// type of analysis to perform
	/// \sa ReliefF
	/// \sa RandomForest
	AnalysisType analysisType;
	/// algorithm steps to perform
	EcAlgorithmType algorithmType;
	/// main effects algorithm
	EcMeAlgorithmType meAlgorithmType;
	/// interactions algorithm
	EcItAlgorithmType itAlgorithmType;

	/// pointer to a main effects algorithm object
	AttributeRanker* maineffectAlgorithm;
	/// pointer to an interaction ranker algorithm object
	AttributeRanker* interactionAlgorithm;

	bool optimizeTemperature;
	double optimalTemperature;
	std::vector<double> temperatures;
	double bestClassificationError;
	/// classification errors used by the T optimization algorithm
	std::vector<double> classificationErrors;

	/// current random jungle scores
	AttributeScores maineffectScores;
	/// current interaction scores
	AttributeScores interactionScores;
	/// current free energy scores
	AttributeScores freeEnergyScores;

	// number of threads to use for random jungle
	unsigned int numRFThreads;
	/// number of attributes to remove per iteration
	unsigned int numToRemovePerIteration;
	/// number of attributes to remove next iteration
	unsigned int numToRemoveNextIteration;

	/// number of target attributes
	unsigned int numTargetAttributes;
	/// attributes that have been evaporated so far
	AttributeScores evaporatedAttributes;
	/// current set of ec scores
	AttributeScores ecScores;
};

#endif	/* EVAPORATIVECOOLING_H */
