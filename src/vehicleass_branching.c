/**@file   vehicleass_branching.c
 * @brief  vehicle assignment branching rule for the VRP
 * @author Lukas Schürmann, University Bonn
 */

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "scip/scip.h"

#include "vehicleass_branching.h"
#include "cons_vehicleass.h"
#include "tools_data.h"
#include "probdata_vrp.h"
#include "vardata_vrp.h"
#include "cons_arcflow.h"
#include "pricer_vrp.h"

#define BRANCHRULE_NAME            "VehAssBranching"
#define BRANCHRULE_DESC            "Branching rule for the vehicle assignment variables"
#define BRANCHRULE_MAXDEPTH        -1
#define BRANCHRULE_MAXBOUNDDIST    1.0


/** branching execution method for fractional LP solutions */
static
SCIP_DECL_BRANCHEXECLP(branchExeclpVehicleAss)
{
    SCIP_PROBDATA* probdata;
    model_data* modeldata;
    SCIP_Real** VeAssVals;
    int* numOfDays;
    SCIP_VAR** modelvars;
    SCIP_VARDATA* vardata;
    SCIP_Real lpval;
    int nvars;
    int i, j;
    SCIP_Real bestval = 0.5;
    int bestnum = 0;
    int best_cust = -1;
    int best_day = -1;

    SCIP_NODE* childprohibit;
    SCIP_NODE* childenforce;
    SCIP_CONS* consprohibit;
    SCIP_CONS* consenforce;

//    printf("Start VA-Branching\n");

    probdata = SCIPgetProbData(scip);
    modeldata = probdata->modeldata;

    /* allocate vehicle assignment data */
    SCIP_CALL( SCIPallocBlockMemoryArray( scip, &numOfDays, probdata->modeldata->nC) );
    SCIP_CALL( SCIPallocBlockMemoryArray( scip, &VeAssVals, probdata->modeldata->nC) );
    for(i = 0; i < probdata->modeldata->nC; i++)
    {
        numOfDays[i] = 0;
        SCIP_CALL( SCIPallocBlockMemoryArray( scip, &VeAssVals[i], probdata->modeldata->nDays) );
        for(j = 0; j < modeldata->nDays; j++)
        {
            VeAssVals[i][j] = 0.0;
        }
    }

    SCIP_CALL( SCIPgetVarsData(scip, &modelvars, &nvars, NULL, NULL, NULL, NULL));

    /* calculate arc flow values */
    for(i = 0; i < nvars; i++)
    {
        lpval = SCIPvarGetLPSol(modelvars[i]);
        if(SCIPisSumPositive(scip, lpval)
        && SCIPisSumPositive(scip, 1 - lpval))
        {
            vardata = SCIPvarGetData(modelvars[i]);
            if(vardata->tourlength == 0) continue;

//            SCIPprintVar(scip, modelvars[i], NULL);
            for(j = 0; j < vardata->tourlength; j++)
            {
                if(SCIPisZero(scip, VeAssVals[vardata->customertour[j]][vardata->day]))
                    numOfDays[vardata->customertour[j]]++;
                VeAssVals[vardata->customertour[j]][vardata->day] += lpval;

            }
        }
    }
    /* search for most fractional vehicle assignment variable */
    for(i = 0; i < modeldata->nC; i++)
    {
        for(j = 0; j < modeldata->nDays; j++)
        {
            double cur_val = fabs(0.5 - VeAssVals[i][j]);
            /* if value is integer, continue */
            if(SCIPisEQ(scip, cur_val, 0.5))
                continue;
            /* if value is worse than best known, continue*/
            if(SCIPisGT(scip, cur_val, bestval))
                continue;
            /* if value is equal, compare number of non-zero days for customer */
            if(SCIPisEQ(scip, cur_val, bestval))
            {
                if(numOfDays[i] <= bestnum)
                    continue;
            }
            /* new best variable found */
            best_cust = i;
            best_day = j;
            bestnum = numOfDays[i];
            bestval = cur_val;

        }
        SCIPfreeBlockMemoryArray( scip, &VeAssVals[i], modeldata->nDays);
    }
    SCIPfreeBlockMemoryArray(scip, &VeAssVals, modeldata->nC);
    SCIPfreeBlockMemoryArray(scip, &numOfDays, modeldata->nC);


    assert(best_cust >= 0 && best_cust < modeldata->nC - 1);
    assert(best_day >= 0 && best_day <= modeldata->nDays - 1);
//    printf("\t--> Most Fractional variable with value %f for customer %d on day %d\n", 0.5-bestval, best_cust, best_day);


    /* create new nodes */
    SCIP_CALL( SCIPcreateChild(scip, &childprohibit, 0.0, SCIPgetLocalTransEstimate(scip)) );
    SCIP_CALL( SCIPcreateChild(scip, &childenforce, 0.0, SCIPgetLocalTransEstimate(scip)) );

    /* create corresponding vehicle assignment constraints */
    SCIP_CALL( SCIPcreateConsVehicleAss(scip, &consprohibit, "prohibit", best_cust, best_day, VA_PROHIBIT, childprohibit, TRUE) );
    SCIP_CALL( SCIPcreateConsVehicleAss(scip, &consenforce, "enforce", best_cust, best_day, VA_ENFORCE, childenforce, TRUE) );

    /* add constraints to nodes */
    SCIP_CALL( SCIPaddConsNode(scip, childprohibit, consprohibit, NULL) );
    SCIP_CALL( SCIPaddConsNode(scip, childenforce, consenforce, NULL) );

    /* release constraints */
    SCIP_CALL( SCIPreleaseCons(scip, &consprohibit) );
    SCIP_CALL( SCIPreleaseCons(scip, &consenforce) );

//    assert(FALSE);

    *result = SCIP_BRANCHED;
    return SCIP_OKAY;
}

/** creates the vehicle assignment branching rule and includes it in SCIP */
SCIP_RETCODE SCIPincludeBranchruleVehicleAss(
        SCIP*                   scip,                /**< SCIP data structure */
        int                     priority
)
{
    SCIP_BRANCHRULEDATA* branchruledata;
    SCIP_BRANCHRULE* branchrule;

    /* create arc flow branching rule data */
    branchruledata = NULL;
    branchrule = NULL;
    /* include branching rule */
    SCIP_CALL( SCIPincludeBranchruleBasic(scip, &branchrule, BRANCHRULE_NAME, BRANCHRULE_DESC, priority, BRANCHRULE_MAXDEPTH,
                                          BRANCHRULE_MAXBOUNDDIST, branchruledata) );
    assert(branchrule != NULL);

    SCIP_CALL( SCIPsetBranchruleExecLp(scip, branchrule, branchExeclpVehicleAss) );

    return SCIP_OKAY;
}