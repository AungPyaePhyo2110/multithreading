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
      int x = 0 ;
    }
    static void                        checkAlgorithmCnt                       ( APolygon                              p )
    {
      // dummy implementation if usingProgtestSolver() returns true
      int x = 0 ;
    }
    void                               start                                   ( int                                   threadCount );
    void                               stop                                    ( void );
    void                               addCompany                              ( ACompany                              company );
  private:
    vector<thread> waitForThreads;  
    vector<thread> solvedPackThreads;
    vector<thread> workerThreads;
    queue<AProblemPack> problemQueue;
    map<AProblemPack,ACompany> problemCompanyMap;
    map<APolygon,AProblemPack> polygonProblemMap;
    map<ACompany,bool> companyDoneMap;
    vector<ACompany> companies;

    AProgtestSolver minSolver;
    AProgtestSolver cntSolver;

    mutex mtx , array_mtx , minSolverMtx, cntSolverMtx;
    condition_variable cv_solving;
    void waitForPackThread(ACompany company);
    void solvedPackThread(ACompany company);
    void wokerThread();
};


  void COptimizer::waitForPackThread(ACompany company)
  {
    while(true)
    {
      AProblemPack problemPack = company->waitForPack();
      if(!problemPack)
      {
        unique_lock<mutex> arrayLock(array_mtx);
        companyDoneMap[company] = true;
        break;
      }
      problemCompanyMap[problemPack] = company;
      unique_lock<mutex> lock(mtx);
      problemQueue.push(problemPack);
    }
  }


  void COptimizer::solvedPackThread(ACompany company)
  {
    while(true)
    {
      AProblemPack problemPack;
      {
        unique_lock<mutex> lock(mtx);
        while(problemQueue.empty()) cv_solving.wait(lock);
        //no more packs and no more waitforPackThreads
        unique_lock<mutex> arraylock(array_mtx);
        if(companyDoneMap[company] && problemQueue.empty())
          break;
        arraylock.unlock();
        //needs to check if this is his
        problemPack = problemQueue.front();
        if(problemCompanyMap[problemPack] == company)
          problemQueue.pop();
      }
    }
  }


  void COptimizer::wokerThread()
  {
    while(true)
    {
      unique_lock<mutex> minLock(minSolverMtx);
      if(!minSolver || !minSolver->hasFreeCapacity())
      {
        minSolver = createProgtestMinSolver();
      }
      minLock.unlock();
      unique_lock<mutex> cntLock(cntSolverMtx);
      if(!cntSolver || !cntSolver->hasFreeCapacity())
      {
        cntSolver = createProgtestCntSolver();
      } 
      cntLock.unlock();

      
      while()
      {
          //solve
      }
      while()
      {

      }


      cv_solving.notify_all();
    }
  }



// TODO: COptimizer implementation goes here
  void COptimizer::start(int threadCount)
  {

    for(int i = 0 ; i < threadCount ; i++)
    {
      workerThreads.push_back(thread(COptimizer::wokerThread));
    }


    for(size_t i = 0 ; i < companies.size(); i++)
    {
      waitForThreads.push_back(thread(COptimizer::waitForPackThread, companies[i]));
      solvedPackThreads.push_back(thread(COptimizer::solvedPackThread , companies[i]));
    }

  }

  void COptimizer::stop(void)
  {
    for(size_t i = 0 ; i < companies.size(); i++)
    {
      waitForThreads[i].join();
      solvedPackThreads[i].join();
    }

    for(size_t i = 0 ; i < workerThreads.size() ; i++)
    {
      workerThreads[i].join();
    }

  }


  void COptimizer::addCompany(ACompany company)
  {
    companies.push_back(company);
  }

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int                                    main                                    ( void )
{
  COptimizer optimizer;
  ACompanyTest  company = std::make_shared<CCompanyTest> ();
  optimizer . addCompany ( company );
  optimizer . start ( 4 );
  optimizer . stop  ();
  if ( ! company -> allProcessed () )
    throw std::logic_error ( "(some) problems were not correctly processsed" );
  return 0;
}
#endif /* __PROGTEST__ */






/// assign .... notify -> workers
/// solver has no more space || no more problems -> solve -> notify -> returner
// returner -> 