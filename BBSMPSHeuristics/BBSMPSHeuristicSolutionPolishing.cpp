/*########################################################################
Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.

Created by Geoffrey Oxberry (oxberry1@llnl.gov, goxberry@gmail.com),
Lluis-Miquel Munguia Conejero (lluis.munguia@gatech.edu), and Deepak
Rajan (rajan3@llnl.gov). LLNL-CODE-699387. All rights reserved.

This file is part of PIPS-SBB. For details, see
https://github.com/llnl/PIPS-SBB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License (as
published by the Free Software Foundation) version 2.1, February 1999.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
conditions of the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
########################################################################*/
#include "BBSMPSHeuristicSolutionPolishing.hpp"

using namespace std;

bool sortfunction (pair<int, int> i,pair<int, int> j) { return (i.first<j.first); }



bool BBSMPSHeuristicSolutionPolishing::runHeuristic(BBSMPSNode* node, denseBAVector &nodeSolution){
	int originalSolutionPoolSize=BBSMPSSolver::instance()->getSolPoolSize();

		double objUB=COIN_DBL_MAX;
	if (BBSMPSSolver::instance()->getSolPoolSize()>0)objUB=BBSMPSSolver::instance()->getSoln(0).getObjValue();


	//Steps for the heuristic
	double startTimeStamp = MPI_Wtime();
	int mype=BBSMPSSolver::instance()->getMype();
	PIPSSInterface &rootSolver= BBSMPSSolver::instance()->getPIPSInterface();
	const denseBAVector varObjectives = rootSolver.getVarObjective();

	SMPSInput &input =BBSMPSSolver::instance()->getSMPSInput();

	int nSols=BBSMPSSolver::instance()->getSolPoolSize();

	vector< int > crossoversToDo;
	const denseBAVector &LPrelaxation=BBSMPSSolver::instance()->getLPRelaxation();
	if (nSols==0){
		timesCalled++;
		cumulativeTime+=(MPI_Wtime()-startTimeStamp);
		return false;
	}

	bool exit=false;
	while(!exit){
		const BBSMPSSolution sol1=BBSMPSSolver::instance()->getSoln(0);

		if (seenCrossovers.count(sol1.getSolNumber())==0){
			seenCrossovers[sol1.getSolNumber()]=1;

			//Perform run
			int count=0;
			int totalVars=0;

			denseBAVector solutionVector1;
			sol1.getSolutionVector(solutionVector1);

			//Find Differences
			//Create an empty branching info vector
			vector<BBSMPSBranchingInfo> bInfos;

			node->getAllBranchingInformation(bInfos);
			BAContext &ctx= BBSMPSSolver::instance()->getBAContext();
			for (int scen = 0; scen < input.nScenarios(); scen++)
			{
				if(ctx.assignedScenario(scen)) {

					vector<pair < int, int> > objectiveContribution(input.nSecondStageVars(scen));

					for (int col = 0; col < input.nSecondStageVars(scen); col++)
					{

						objectiveContribution[col].first= varObjectives.getSecondStageVec(scen)[col]*solutionVector1.getSecondStageVec(scen)[col];
						objectiveContribution[col].second=col;
					}

					std::sort(objectiveContribution.begin(), objectiveContribution.end(), sortfunction);

					for (int i = 0; i < input.nSecondStageVars(scen)*0.9; i++)
					{
						int col=objectiveContribution[i].second;
						if(input.isSecondStageColInteger(scen,col)){
							if (isIntFeas(solutionVector1.getSecondStageVec(scen)[col],intTol)){
								double sol1Val=solutionVector1.getSecondStageVec(scen)[col];

								bInfos.push_back(BBSMPSBranchingInfo(col,sol1Val,'E',2,scen));
								count++;
							}
							else{
								if (varObjectives.getSecondStageVec(scen)[col]<0){
									double sol1Val=solutionVector1.getSecondStageVec(scen)[col];
									bInfos.push_back(BBSMPSBranchingInfo(col,ceil(sol1Val),'E',2,scen));
									count++;
								}
								else{
									double sol1Val=solutionVector1.getSecondStageVec(scen)[col];
									bInfos.push_back(BBSMPSBranchingInfo(col,floor(sol1Val),'E',2,scen));
									count++;
								}
							}
						}
						totalVars++;


					}
				}
			}

			//Run
			//Create a node
			BBSMPSNode* rootNode= new BBSMPSNode(NULL, bInfos);
			//BAFlagVector<variableState> ps(BBSMPSSolver::instance()->getOriginalWarmStart());
			//node->reconstructWarmStartState(ps);
			//rootNode.setWarmStartState(ps);
			//rootNode->copyCuttingPlanes(BBSMPSTree::getRootNode());
			//Create a tree && Add node to tree
			BBSMPSTree bb(rootNode,COIN_DBL_MIN,objUB);
			bb.setVerbosity(false);
			//Add simple heuristics to tree
			//bb.loadSimpleHeuristics();
			BBSMPSHeuristicLockRounding *hr= new BBSMPSHeuristicLockRounding(1,1,"LockRounding");
	  		bb.loadLPHeuristic(hr);
			//Add time/node limit
			bb.setNodeLimit(nodeLim);
			bb.setSolLimit(1);
			double bestUB=BBSMPSSolver::instance()->getSoln(0).getObjValue();

			bb.setLB(node->getObjective());
			bb.setUB(bestUB);
			//Run

			bb.branchAndBound();


		}
		else exit=true;


	}

	double objUB2=COIN_DBL_MAX;
	if (BBSMPSSolver::instance()->getSolPoolSize()>0)objUB2=BBSMPSSolver::instance()->getSoln(0).getObjValue();

	bool success=(objUB2!=objUB);

	timesCalled++;
	timesSuccessful+=(success);

	cumulativeTime+=(MPI_Wtime()-startTimeStamp);
	return success;

}

bool BBSMPSHeuristicSolutionPolishing::shouldItRun(BBSMPSNode* node, denseBAVector &nodeSolution){
	return true;

}
