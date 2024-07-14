#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <array>
#include <iterator>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <compare>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <condition_variable>
#include <pthread.h>
#include <semaphore.h>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
std::mutex mutx;

atomic_int first = 0;
atomic_int second = 0;

class ProblemPackWrapper
{
    public:
    AProblemPack problemPack;
    size_t solvedPolygons;
    mutex mutx;
    condition_variable CVariable;

    ProblemPackWrapper(AProblemPack pB )
    {
        problemPack = pB;
        solvedPolygons = 0 ;
    }

    bool isSolved() const
    {
        size_t sizeToFinish = problemPack->m_ProblemsMin.size()+problemPack->m_ProblemsCnt.size();
        return sizeToFinish == solvedPolygons;
    }
};

using AProblemPackWrapper = std::shared_ptr<ProblemPackWrapper>;

class CompanyWrapper
{
public:
    ACompany company;

    mutex mutx;
    condition_variable CVariable;
    queue<AProblemPackWrapper> problemPacks;
    CompanyWrapper(ACompany com) : company(com)
    {
    }
};

using ACompanyWrapper = std::shared_ptr<CompanyWrapper>;

enum class SolverType{
    MIN , CNT
};




class SolverWrapper
{
    public:
    SolverType solverType;
    AProgtestSolver solver;
    map<AProblemPackWrapper,int> containProblemPacks;

    SolverWrapper(AProgtestSolver mySolver , SolverType mySolverType)
    {
        solver = mySolver;
        solverType = mySolverType;
    }
};

using ASolverWrapper = std::shared_ptr<SolverWrapper>;



class COptimizer
{
  public:
    static bool                        usingProgtestSolver                     ( void )
    {
      return true;
    }
    static void                        checkAlgorithmMin                       ( APolygon                              p )
    {
      // dummy implementation if usingProgtestSolver() returns true
    }
    static void                        checkAlgorithmCnt                       ( APolygon                              p )
    {
      // dummy implementation if usingProgtestSolver() returns true
    }
    void                               start                                   ( int                                   threadCount );
    void                               stop                                    ( void );
    void                               addCompany                              ( ACompany                              company );
    
    void workerThread();
    void waitForpackThread(ACompanyWrapper companywrapper);
    void solvedPackThread(ACompanyWrapper companywrapper);
  private:
    vector<thread> workerThreads;

    vector<thread> waitForPackThreads;
    
    vector<thread> solvedPackThreads;

    vector<ACompanyWrapper> companies;

    atomic_size_t assignedCompanies = 0;
    atomic_size_t numberOfCompanies = 0;

    queue<ASolverWrapper> solverQueue;

    condition_variable CVariable_worker;
    condition_variable CVariable_return;


    ASolverWrapper minSolver;
    ASolverWrapper cntSolver;

};

void COptimizer::waitForpackThread(ACompanyWrapper companywrapper)
{   
    unique_lock<mutex> companyLock(companywrapper->mutx,defer_lock);
    unique_lock<mutex> globalLock(mutx,defer_lock);
    while(true)
    {
        AProblemPack problemPack =  companywrapper->company->waitForPack();
        if(!problemPack)
        {   
            // std::cout << "company has finished" << std::endl;
            companyLock.lock();
            companywrapper->problemPacks.push(nullptr);
            companywrapper->CVariable.notify_all();
            companyLock.unlock();
            break;
        }

        // std::cout << "Company Assigning ProblemPack:---------------------" << first++ << std::endl;

        AProblemPackWrapper PBwrapper = AProblemPackWrapper(new ProblemPackWrapper(problemPack));
        companyLock.lock();
        companywrapper->problemPacks.push(PBwrapper);
        companywrapper->CVariable.notify_all();
        companyLock.unlock();


        unique_lock<mutex> PBlock(PBwrapper->mutx);
        size_t i = 0;
        while( i < PBwrapper->problemPack->m_ProblemsMin.size())
        {   
            globalLock.lock();
            if(minSolver->solver->hasFreeCapacity())
            {
                minSolver->solver->addPolygon(PBwrapper->problemPack->m_ProblemsMin[i]);
                if(minSolver->containProblemPacks.count(PBwrapper)==0)
                {
                    minSolver->containProblemPacks[PBwrapper] = 1;
                }
                else
                {
                    minSolver->containProblemPacks[PBwrapper]+=1;
                }
                i++;
            }
            if(!minSolver->solver->hasFreeCapacity())
            {
                solverQueue.push(minSolver);
                CVariable_worker.notify_all();
                minSolver = ASolverWrapper(new SolverWrapper(createProgtestMinSolver(),SolverType::MIN));
            }
            globalLock.unlock();
        }
        size_t j = 0;
        while( j < PBwrapper->problemPack->m_ProblemsCnt.size())
        {   
            globalLock.lock();
            if(cntSolver->solver->hasFreeCapacity())
            {
                cntSolver->solver->addPolygon(PBwrapper->problemPack->m_ProblemsCnt[j]);
                if(cntSolver->containProblemPacks.count(PBwrapper)==0)
                {
                    cntSolver->containProblemPacks[PBwrapper] = 1;
                }
                else
                {
                    cntSolver->containProblemPacks[PBwrapper]+=1;
                }
                j++;
            }
            if(!cntSolver->solver->hasFreeCapacity())
            {
                solverQueue.push(cntSolver);
                CVariable_worker.notify_all();
                cntSolver = ASolverWrapper(new SolverWrapper(createProgtestCntSolver(),SolverType::CNT));
            }
            globalLock.unlock();
        }
        PBlock.unlock();
    }

    if(assignedCompanies == numberOfCompanies-1)
    {
        globalLock.lock();
        solverQueue.push(minSolver);
        solverQueue.push(cntSolver);
        CVariable_worker.notify_all();
        globalLock.unlock();
    }

    assignedCompanies++;


}

void COptimizer::solvedPackThread(ACompanyWrapper companywrapper)
{   
    unique_lock<mutex> companyLock(companywrapper->mutx,defer_lock);

    while(true)
    {
        companyLock.lock();
        while(companywrapper->problemPacks.empty())
        {
            // std::cout << "company waiting to return the pack" << std::endl;
            companywrapper->CVariable.wait(companyLock); 
        }
        AProblemPackWrapper myPack = companywrapper->problemPacks.front();
        companywrapper->problemPacks.pop();
        companyLock.unlock();

        
        if(!myPack)
        {
            // std::cout << "COmpany second thread has finished" << std::endl;
            break;
        }



        unique_lock<mutex> myLock(myPack->mutx);
        while(!myPack->isSolved())
        {
            // std::cout << "Return thread sleeping" << std::endl;
             myPack->CVariable.wait(myLock);
            
        }   
        myLock.unlock();
        companywrapper->company->solvedPack(myPack->problemPack);
        // std::cout << "Company returing the problemPack:-----------------" << second++ << std::endl;
    }
}


void COptimizer::workerThread()
{   
    unique_lock<mutex> lock(mutx,defer_lock);
    while(true)
    {
        lock.lock();
        while(solverQueue.empty() && assignedCompanies!= numberOfCompanies)
        {
            // std::cout << "woker sleeping" << std::endl;
            CVariable_worker.wait(lock);
        }
        if(assignedCompanies ==  numberOfCompanies && solverQueue.empty())
        {
            // std::cout << "woker has finished his job" << std::endl;
             break;
        }

        // std::cout << "worker working" << std::endl;
        ASolverWrapper currentSolver = solverQueue.front();
        solverQueue.pop();
        lock.unlock();

        currentSolver->solver->solve();

        // std::cout << "Worker solved" << std::endl;

        for( auto x : currentSolver->containProblemPacks)
        {
            
            AProblemPackWrapper PBW = x.first;
            unique_lock<mutex> PBLock(PBW->mutx);
            PBW->solvedPolygons += x.second;
            if(PBW->isSolved())
            {
                PBW->CVariable.notify_all();
            }
            PBLock.unlock();
        }
    }

}


// TODO: COptimizer implementation goes here

void COptimizer::start(int threadCount)
{
    minSolver = ASolverWrapper(new SolverWrapper(createProgtestMinSolver(),SolverType::MIN));
    cntSolver = ASolverWrapper(new SolverWrapper(createProgtestCntSolver(),SolverType::CNT));
    assignedCompanies = 0;

    for (int i = 0; i < threadCount; i++)
    {
        workerThreads.push_back(thread(&COptimizer::workerThread,this));  
    }


    for (size_t i = 0; i < companies.size(); i++)
    {
        waitForPackThreads.push_back(thread(&COptimizer::waitForpackThread, this, companies[i]));
        solvedPackThreads.push_back(thread(&COptimizer::solvedPackThread, this, companies[i]));
    }

}


void COptimizer::stop()
{
    for(size_t i = 0 ; i < companies.size() ; i++)
    {
        waitForPackThreads[i].join();
        solvedPackThreads[i].join();
    }

    for(size_t i = 0 ; i < workerThreads.size() ; i++)
    {
        workerThreads[i].join();
    }
}


void COptimizer::addCompany(ACompany company)
{
    numberOfCompanies++;
    ACompanyWrapper companyWrapper(new CompanyWrapper(company));   
    companies.push_back(companyWrapper);

}




//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int                                    main                                    ( void )
{
  COptimizer optimizer;
  ACompanyTest  company = std::make_shared<CCompanyTest> ();
  ACompanyTest  company2 = std::make_shared<CCompanyTest> ();


  optimizer . addCompany ( company );
  optimizer . addCompany( company2);


  optimizer . start ( 8 );
  optimizer . stop  ();
  if ( ! company -> allProcessed () )
    throw std::logic_error ( "(some) problems were not correctly processsed" );
  return 0;
}
#endif /* __PROGTEST__ */
