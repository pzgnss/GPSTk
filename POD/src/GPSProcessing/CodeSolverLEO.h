#ifndef POD_PR_SOLVER_LEO_H
#define POD_PR_SOLVER_LEO_H

#include"CodeSolverBase.h"
#include"IonoModelStore.hpp"
#include"CodeProcSvData.h"
#include"NeillTropModel.hpp"

using namespace gpstk;

namespace pod
{
    class CodeSolverLEO : public CodeSolverBase
    {
    public:
        CodeSolverLEO(GnssDataStore_sptr data) : CodeSolverBase(data)
        {};
        virtual ~CodeSolverLEO() {};

        std::string virtual getName() override
        {
            return "CodeSolverLEO";
        };

        virtual NeillTropModel initTropoModel(const Position &nominalPos, int DoY) override
        {
            return NULL;
        };
        virtual double CodeSolverLEO::getTropoCorrection(
            const Position &rxPos,
            const Position &svPos,
            const CommonTime &t
        ) const override
        {
            return  0;
        };
     };
}
#endif // !POD_PR_SOLVER_LEO_H